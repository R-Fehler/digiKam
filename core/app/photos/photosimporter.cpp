/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - import photos and videos from a phone,
 *               a memory card or a folder into the library, with a
 *               history of the imports which can be undone.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosimporter.h"

// Qt includes

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>
#include <QtConcurrentRun>

#ifdef Q_OS_WIN
#   include <windows.h>
#endif

// KDE includes

#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>

// Local includes

#include "digikam_debug.h"
#include "collectionmanager.h"
#include "coredb.h"
#include "coredbaccess.h"
#include "coredbalbuminfo.h"
#include "coredbconstants.h"
#include "dimg.h"
#include "dmetadata.h"
#include "iteminfo.h"
#include "metaengine.h"
#include "photoslibrarymodel.h"
#include "scancontroller.h"

namespace Digikam
{

namespace
{

const char* const s_historyFolder = ".photos-imports";

QSet<QString> mediaSuffixes()
{
    QStringList image;
    QStringList video;

    CoreDbAccess().db()->getFilterSettings(&image, &video, nullptr);

    QSet<QString> suffixes;

    for (const QString& s : std::as_const(image) + video)
    {
        const QString suffix = s.trimmed().remove(QLatin1String("*.")).toLower();

        if (!suffix.isEmpty())
        {
            suffixes.insert(suffix);
        }
    }

    return suffixes;
}

/// The XMP sidecar of a file in a source folder, either naming scheme.
QString sourceSidecar(const QFileInfo& info)
{
    const QString candidates[] =
    {
        info.filePath()                                       + QLatin1String(".xmp"),
        info.filePath()                                       + QLatin1String(".XMP"),
        info.path() + QLatin1Char('/') + info.completeBaseName() + QLatin1String(".xmp"),
        info.path() + QLatin1Char('/') + info.completeBaseName() + QLatin1String(".XMP")
    };

    for (const QString& candidate : candidates)
    {
        if (QFileInfo::exists(candidate))
        {
            return candidate;
        }
    }

    return QString();
}

/**
 * Folders starting with a dot are hidden on Linux and macOS, not on Windows:
 * give them the hidden attribute there, as Explorer expects.
 */
void hideFolder(const QString& path)
{

#ifdef Q_OS_WIN

    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    const DWORD attributes    = GetFileAttributesW(native.c_str());

    if ((attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_HIDDEN))
    {
        SetFileAttributesW(native.c_str(), attributes | FILE_ATTRIBUTE_HIDDEN);
    }

#else

    Q_UNUSED(path);

#endif

}

QString hashOf(const QString& filePath, int version)
{
    return QString::fromUtf8((version >= 2) ? DImg::getUniqueHashVersion(filePath, version)
                                            : DImg::getUniqueHash(filePath));
}

void examineFiles(const QStringList& paths, int hashVersion, PhotosImporter::ScanResult& result,
                  QAtomicInt* done, QAtomicInt* cancel);

PhotosImporter::ScanResult scanFolder(const QString& folder, int hashVersion,
                                      QAtomicInt* done, QAtomicInt* total, QAtomicInt* cancel)
{
    PhotosImporter::ScanResult result;
    result.source = folder;

    const QSet<QString> suffixes = mediaSuffixes();

    // 1. List the files (fast, even over MTP).

    QStringList paths;
    QDirIterator it(folder, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        if (cancel->loadRelaxed())
        {
            result.canceled = true;

            return result;
        }

        const QString path = it.next();

        // Thumbnails caches of phones and cameras.

        if (path.contains(QLatin1String("/.thumbnails/")) || path.contains(QLatin1String("/.trashed")))
        {
            continue;
        }

        if (suffixes.contains(it.fileInfo().suffix().toLower()))
        {
            paths << path;
            total->storeRelaxed(paths.size());
        }
    }

    examineFiles(paths, hashVersion, result, done, cancel);

    return result;
}

/**
 * Already in the library? Capture date and device of the new ones.
 * Files twice in the list (same content) go to result.duplicates.
 */
void examineFiles(const QStringList& paths, int hashVersion, PhotosImporter::ScanResult& result,
                  QAtomicInt* done, QAtomicInt* cancel)
{
    QSet<QString> seen;

    for (const QString& path : std::as_const(paths))
    {
        if (cancel->loadRelaxed())
        {
            result.canceled = true;

            return;
        }

        const QFileInfo info(path);

        PhotosImporter::Candidate candidate;
        candidate.path = path;
        candidate.size = info.size();

        const QString hash = hashOf(path, hashVersion);
        const QString key  = hash + QLatin1Char('/') + QString::number(candidate.size);

        if (seen.contains(key))
        {
            // Same file twice in the source (e.g. copies in two folders).

            result.duplicates << path;
            done->fetchAndAddRelaxed(1);

            continue;
        }

        seen.insert(key);

        if (!hash.isEmpty())
        {
            const QList<ItemScanInfo> identical = CoreDbAccess().db()->getIdenticalFiles(hash, candidate.size);

            for (const ItemScanInfo& item : identical)
            {
                if (item.status == DatabaseItem::Visible)
                {
                    candidate.existing = true;
                    break;
                }
            }
        }

        if (!candidate.existing)
        {
            DMetadata meta;

            if (meta.load(path))
            {
                candidate.date   = meta.getItemDateTime();
                QString make     = meta.getExifTagString("Exif.Image.Make");
                QString model    = meta.getExifTagString("Exif.Image.Model");

                if (make.isEmpty() && model.isEmpty())
                {
                    make  = meta.getXmpTagString("Xmp.video.Make");
                    model = meta.getXmpTagString("Xmp.video.Model");
                }

                candidate.device = photosPrettyDevice(make, model);
            }

            if (!candidate.date.isValid())
            {
                candidate.date = info.lastModified();
            }
        }

        result.files << candidate;
        done->fetchAndAddRelaxed(1);
    }
}

/// destination path which does not exist yet: "IMG_0001.HEIC", "IMG_0001_1.HEIC"...
QString freeDestination(const QString& dir, const QFileInfo& source)
{
    QString dest = dir + QLatin1Char('/') + source.fileName();

    for (int i = 1 ; QFileInfo::exists(dest) ; ++i)
    {
        dest = dir + QLatin1Char('/') + source.completeBaseName() +
               QString::fromLatin1("_%1.").arg(i) + source.suffix();
    }

    return dest;
}

bool copyFile(const QString& source, const QString& dest)
{
    // Copy under a temporary hidden name, then rename: folder monitoring and
    // scans never see a partial file.

    const QFileInfo destInfo(dest);
    const QString temp = destInfo.path() + QLatin1String("/.") + destInfo.fileName() + QLatin1String(".part");

    QFile::remove(temp);

    if (!QFile::copy(source, temp))
    {
        return false;
    }

    QFile file(temp);

    if (file.open(QIODevice::ReadWrite))
    {
        file.setFileTime(QFileInfo(source).lastModified(), QFileDevice::FileModificationTime);
        file.close();
    }

    if (!QFile::rename(temp, dest))
    {
        QFile::remove(temp);

        return false;
    }

    return true;
}

PhotosImporter::ImportResult importFiles(const QList<PhotosImporter::Candidate>& files, const QString& root,
                                         QAtomicInt* done, QAtomicInt* cancel)
{
    PhotosImporter::ImportResult result;

    for (const PhotosImporter::Candidate& file : files)
    {
        if (cancel->loadRelaxed())
        {
            result.canceled = true;
            break;
        }

        const QFileInfo source(file.path);
        const QDate date     = file.date.date();
        const QString dir    = root + QLatin1Char('/') +
                               (date.isValid() ? date.toString(QLatin1String("yyyy/MM"))
                                               : QStringLiteral("Undated"));

        if (!QDir().mkpath(dir))
        {
            ++result.failed;
            done->fetchAndAddRelaxed(1);
            continue;
        }

        const QString dest = freeDestination(dir, source);

        if (!copyFile(file.path, dest))
        {
            qCWarning(DIGIKAM_GENERAL_LOG) << "Photos import: cannot copy" << file.path << "to" << dest;
            ++result.failed;
            done->fetchAndAddRelaxed(1);
            continue;
        }

        const QString sidecar = sourceSidecar(source);

        if (!sidecar.isEmpty())
        {
            copyFile(sidecar, MetaEngine::sidecarFilePathForFile(dest));
        }

        // Adds the file to the database now (folder monitoring would later).

        const ItemInfo info = ScanController::instance()->scannedInfo(dest);

        if (!info.isNull())
        {
            result.ids << info.id();
        }

        result.copied << QDir(root).relativeFilePath(dest);
        result.pairs  << qMakePair(file.path, dest);
        done->fetchAndAddRelaxed(1);
    }

    return result;
}

/**
 * An import without the sheet (inbox): examine, copy what is new, then, when
 * asked, remove from the source what is safely in the library: verified
 * copies (same size and digiKam hash) and files already in the library.
 */
PhotosImporter::BackgroundResult importBackground(const PhotosImporter::BackgroundJob& job, int hashVersion)
{
    PhotosImporter::BackgroundResult result;
    PhotosImporter::ScanResult scan;
    QAtomicInt done;
    QAtomicInt cancel;

    examineFiles(job.files, hashVersion, scan, &done, &cancel);

    QList<PhotosImporter::Candidate> files;
    QStringList alreadyThere = scan.duplicates;

    for (const PhotosImporter::Candidate& file : std::as_const(scan.files))
    {
        if (file.existing)
        {
            alreadyThere << file.path;
            ++result.existing;
        }
        else
        {
            files << file;
        }
    }

    std::sort(files.begin(), files.end(),
              [] (const PhotosImporter::Candidate& a, const PhotosImporter::Candidate& b)
        {
            return (a.date < b.date);
        }
    );

    const PhotosImporter::ImportResult imported = importFiles(files, job.root, &done, &cancel);

    result.copied = imported.copied;
    result.failed = imported.failed;
    result.handled << alreadyThere;

    for (const QPair<QString, QString>& pair : imported.pairs)
    {
        result.handled << pair.first;
    }

    if (!job.moveSources)
    {
        return result;
    }

    auto removeSource = [] (const QString& path)
    {
        const QString sidecar = sourceSidecar(QFileInfo(path));

        if (QFile::remove(path) && !sidecar.isEmpty())
        {
            QFile::remove(sidecar);
        }
    };

    for (const QPair<QString, QString>& pair : imported.pairs)
    {
        const QFileInfo source(pair.first);
        const QFileInfo copy(pair.second);

        if (
            (source.size() == copy.size()) &&
            (hashOf(pair.first, qMax(2, hashVersion)) == hashOf(pair.second, qMax(2, hashVersion)))
           )
        {
            removeSource(pair.first);
        }
        else
        {
            qCWarning(DIGIKAM_GENERAL_LOG) << "Photos inbox: copy of" << pair.first << "differs, kept in the inbox";
        }
    }

    for (const QString& path : std::as_const(alreadyThere))
    {
        removeSource(path);
    }

    return result;
}

PhotosImporter::DeviceCount countDeviceFiles(const QHash<QString, QStringList>& deviceFiles)
{
    PhotosImporter::DeviceCount result;

    for (auto it = deviceFiles.constBegin() ; it != deviceFiles.constEnd() ; ++it)
    {
        int count = 0;
        QSet<QString> seen;

        for (const QString& path : it.value())
        {
            // A file can be listed twice: imported here, moved away and back.

            if (seen.contains(path))
            {
                continue;
            }

            seen.insert(path);

            if (QFileInfo::exists(path))
            {
                result.deviceOfPath.insert(path, it.key());
                ++count;
            }
        }

        if (count > 0)
        {
            QVariantMap device;
            device.insert(QLatin1String("name"),  it.key());
            device.insert(QLatin1String("count"), count);
            result.devices << device;
        }
    }

    std::sort(result.devices.begin(), result.devices.end(),
              [] (const QVariant& a, const QVariant& b)
        {
            return (a.toMap().value(QLatin1String("count")).toInt() > b.toMap().value(QLatin1String("count")).toInt());
        }
    );

    return result;
}

} // namespace

QString PhotosImporter::devicesRootTagName()
{
    return QLatin1String("Devices");
}

PhotosImporter::PhotosImporter(PhotosLibraryModel* const library, QObject* const parent)
    : QObject  (parent),
      m_library(library)
{
    m_importFolder = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"))
                                                .readEntry("Import Folder", QString());

    connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished,
            this, &PhotosImporter::slotScanned);

    connect(&m_importWatcher, &QFutureWatcher<ImportResult>::finished,
            this, &PhotosImporter::slotImported);

    connect(&m_devicesWatcher, &QFutureWatcher<DeviceCount>::finished,
            this, &PhotosImporter::slotDevicesCounted);

    connect(&m_backgroundWatcher, &QFutureWatcher<BackgroundResult>::finished,
            this, &PhotosImporter::slotBackgroundImported);

    QTimer* const timer = new QTimer(this);
    timer->setInterval(100);
    m_progressTimer     = timer;

    connect(timer, &QTimer::timeout,
            this, &PhotosImporter::progressChanged);

    // Collections can be mounted later (e.g. network drives): update the
    // folders and the history.

    connect(CollectionManager::instance(), &CollectionManager::locationStatusChanged,
            this, [this] ()
        {
            Q_EMIT importFolderChanged();
            reloadHistory();
        }
    );

    QTimer::singleShot(0, this, &PhotosImporter::reloadHistory);
}

PhotosImporter::~PhotosImporter()
{
    m_cancel.storeRelaxed(1);
    m_scanWatcher.waitForFinished();
    m_importWatcher.waitForFinished();
    m_devicesWatcher.waitForFinished();
    m_backgroundWatcher.waitForFinished();
}

QVariantList PhotosImporter::sources() const
{
    return m_sources;
}

int PhotosImporter::state() const
{
    return m_state;
}

int PhotosImporter::done() const
{
    return m_done.loadRelaxed();
}

int PhotosImporter::total() const
{
    return m_total.loadRelaxed();
}

QVariantMap PhotosImporter::summary() const
{
    return m_summary;
}

QVariantList PhotosImporter::history() const
{
    return m_history;
}

QStringList PhotosImporter::libraryFolders() const
{
    return CollectionManager::instance()->allAvailableAlbumRootPaths();
}

QString PhotosImporter::importFolder() const
{
    const QStringList roots = libraryFolders();

    if (roots.contains(m_importFolder))
    {
        return m_importFolder;
    }

    return roots.isEmpty() ? QString() : roots.constFirst();
}

void PhotosImporter::setImportFolder(const QString& folder)
{
    if (folder == m_importFolder)
    {
        return;
    }

    m_importFolder = folder;

    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"));
    group.writeEntry("Import Folder", folder);
    group.sync();

    Q_EMIT importFolderChanged();
}

void PhotosImporter::setState(State state)
{
    m_state = state;

    if ((state == Scanning) || (state == Importing))
    {
        static_cast<QTimer*>(m_progressTimer)->start();
    }
    else
    {
        static_cast<QTimer*>(m_progressTimer)->stop();
    }

    Q_EMIT progressChanged();
    Q_EMIT stateChanged();
}

void PhotosImporter::refreshSources()
{
    QVariantList sources;
    QSet<QString> known;

    auto addIfMedia = [&sources, &known] (const QString& path, const QString& name)
    {
        // A camera, phone or memory card has a DCIM folder (Design rule for
        // Camera File system), possibly one level down on phones
        // ("Internal shared storage/DCIM").

        QString dcim;

        if (QFileInfo(path + QLatin1String("/DCIM")).isDir())
        {
            dcim = path + QLatin1String("/DCIM");
        }
        else
        {
            const QFileInfoList children = QDir(path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

            for (const QFileInfo& child : children)
            {
                if (QFileInfo(child.filePath() + QLatin1String("/DCIM")).isDir())
                {
                    dcim = child.filePath() + QLatin1String("/DCIM");
                    break;
                }
            }
        }

        if (!dcim.isEmpty() && !known.contains(dcim))
        {
            known.insert(dcim);

            QVariantMap source;
            source.insert(QLatin1String("path"), dcim);
            source.insert(QLatin1String("name"), name);
            sources << source;
        }
    };

    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    for (const QStorageInfo& volume : volumes)
    {
        if (!volume.isValid() || !volume.isReady() || volume.isRoot())
        {
            continue;
        }

        const QString name = volume.displayName().isEmpty() ? volume.rootPath() : volume.displayName();
        addIfMedia(volume.rootPath(), name);
    }

#ifdef Q_OS_LINUX

    // Phones connected over MTP or PTP, mounted by GVfs (GNOME, Xfce...).
    // KDE Plasma uses KIO instead: those are reached through "Choose a folder".

    const QString gvfs = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QLatin1String("/gvfs");
    const QFileInfoList mounts = QDir(gvfs).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo& mount : mounts)
    {
        // "mtp:host=SAMSUNG_SAMSUNG_Android_R58N..." -> "SAMSUNG Android"

        QString name = mount.fileName().section(QLatin1Char('='), 1);
        name.replace(QLatin1Char('_'), QLatin1Char(' '));
        name = name.simplified();

        const QStringList words = name.split(QLatin1Char(' '));

        if ((words.size() >= 2) && (words.at(0) == words.at(1)))
        {
            name = words.mid(1).join(QLatin1Char(' '));
        }

        name = name.section(QLatin1Char(' '), 0, 1);
        addIfMedia(mount.filePath(), name.isEmpty() ? mount.fileName() : name);
    }

#endif

    if (sources != m_sources)
    {
        m_sources = sources;

        Q_EMIT sourcesChanged();
    }
}

QString PhotosImporter::chooseFolder()
{
    return QFileDialog::getExistingDirectory(QApplication::activeWindow(),
                                             i18n("Import photos from"),
                                             QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
}

void PhotosImporter::scan(const QString& folder)
{
    if ((m_state == Scanning) || (m_state == Importing) || folder.isEmpty())
    {
        return;
    }

    m_cancel.storeRelaxed(0);
    m_done.storeRelaxed(0);
    m_total.storeRelaxed(0);
    m_summary.clear();
    m_summary.insert(QLatin1String("source"), folder);

    setState(Scanning);

    const int hashVersion = CoreDbAccess().db()->getUniqueHashVersion();

    m_scanWatcher.setFuture(QtConcurrent::run(&scanFolder, folder, hashVersion,
                                              &m_done, &m_total, &m_cancel));
}

void PhotosImporter::slotScanned()
{
    m_scan = m_scanWatcher.result();

    if (m_scan.canceled)
    {
        setState(Idle);

        return;
    }

    int newCount      = 0;
    int existingCount = 0;
    QDateTime first;
    QDateTime last;
    QHash<QString, int> devices;

    for (const Candidate& file : std::as_const(m_scan.files))
    {
        if (file.existing)
        {
            ++existingCount;
            continue;
        }

        ++newCount;

        if (file.date.isValid())
        {
            if (!first.isValid() || (file.date < first))
            {
                first = file.date;
            }

            if (!last.isValid() || (file.date > last))
            {
                last = file.date;
            }
        }

        if (!file.device.isEmpty())
        {
            devices[file.device]++;
        }
    }

    // Suggested device name: the one given at the last import from this
    // source, else the camera of most photos, else the volume name.

    QString device = m_deviceOfSource.value(QDir::cleanPath(m_scan.source));
    const bool knownDevice = !device.isEmpty();

    if (!knownDevice)
    {
        int best = 0;

        for (auto it = devices.constBegin() ; it != devices.constEnd() ; ++it)
        {
            if (it.value() > best)
            {
                best   = it.value();
                device = it.key();
            }
        }
    }

    if (device.isEmpty())
    {
        for (const QVariant& source : std::as_const(m_sources))
        {
            if (m_scan.source.startsWith(source.toMap().value(QLatin1String("path")).toString()))
            {
                device = source.toMap().value(QLatin1String("name")).toString();
            }
        }
    }

    const QLocale locale;

    // Nothing new on a device imported before: everything on it is in the
    // library now, which tells what can be deleted from it.

    if ((newCount == 0) && knownDevice && !m_scan.files.isEmpty() && !importFolder().isEmpty())
    {
        writeDeviceCheck(importFolder(), device, QDateTime::currentDateTime());
        reloadHistory();
    }

    m_summary.insert(QLatin1String("knownDevice"),   knownDevice);
    m_summary.insert(QLatin1String("newCount"),      newCount);
    m_summary.insert(QLatin1String("existingCount"), existingCount);
    m_summary.insert(QLatin1String("device"),        device);
    m_summary.insert(QLatin1String("dateRange"),
                     !first.isValid() ? QString()
                                      : (first.date() == last.date())
                                        ? locale.toString(first.date(), QLocale::LongFormat)
                                        : QString::fromUtf8("%1 \u2013 %2")
                                              .arg(locale.toString(first.date(), QLocale::ShortFormat))
                                              .arg(locale.toString(last.date(),  QLocale::ShortFormat)));

    setState(Ready);
}

void PhotosImporter::start(const QString& deviceName)
{
    if (m_state != Ready)
    {
        return;
    }

    const QString root = importFolder();

    if (root.isEmpty())
    {
        return;
    }

    QList<Candidate> files;

    for (const Candidate& file : std::as_const(m_scan.files))
    {
        if (!file.existing)
        {
            files << file;
        }
    }

    // Oldest first: the library fills in capture order.

    std::sort(files.begin(), files.end(),
              [] (const Candidate& a, const Candidate& b)
        {
            return (a.date < b.date);
        }
    );

    // "/" separates tag levels.

    m_deviceName = deviceName.simplified().replace(QLatin1Char('/'), QLatin1Char('-'));
    m_startTime  = QDateTime::currentDateTime();

    m_cancel.storeRelaxed(0);
    m_done.storeRelaxed(0);
    m_total.storeRelaxed(files.size());

    setState(Importing);

    m_importWatcher.setFuture(QtConcurrent::run(&importFiles, files, root, &m_done, &m_cancel));
}

void PhotosImporter::slotImported()
{
    const ImportResult result = m_importWatcher.result();
    const QString root        = importFolder();
    QString importId;

    // What is in the library now: the files there already, and the copies.
    // The first file left out (failed, or after Stop) limits what can be
    // deleted from the device: the photos taken before it.

    QSet<QString> copiedSources;

    for (const QPair<QString, QString>& pair : result.pairs)
    {
        copiedSources.insert(pair.first);
    }

    Safety safety;
    safety.complete = true;

    for (const Candidate& file : std::as_const(m_scan.files))
    {
        if (!file.existing && !copiedSources.contains(file.path))
        {
            safety.complete = false;

            if (file.date.isValid() && (!safety.safeBefore.isValid() || (file.date < safety.safeBefore)))
            {
                safety.safeBefore = file.date;
            }
        }
    }

    if (safety.complete)
    {
        safety.safeBefore = m_startTime;
    }

    if (!result.copied.isEmpty())
    {
        // History record, with the device: photos nobody touches get no
        // sidecar (see devices()).

        importId = writeRecord(root, m_deviceName, m_scan.source, result.copied,
                               m_summary.value(QLatin1String("existingCount")).toInt(), m_startTime, QString(),
                               safety);
    }

    reloadHistory();

    m_summary.insert(QLatin1String("importedCount"), result.copied.size());
    m_summary.insert(QLatin1String("failedCount"),   result.failed);
    m_summary.insert(QLatin1String("importId"),      importId);
    m_summary.insert(QLatin1String("canceled"),      result.canceled);
    m_summary.insert(QLatin1String("safety"),        importId.isEmpty() ? QVariantMap() : deviceSafety(m_deviceName));

    setState(Done);

    if (!importId.isEmpty())
    {
        Q_EMIT imported(importId, result.copied.size());
    }

    // Inbox imports wait while an import of the user runs.

    startNextBackground();
}

QString PhotosImporter::writeRecord(const QString& root, const QString& device, const QString& source,
                                    const QStringList& copied, int skipped, const QDateTime& startTime,
                                    const QString& inboxId, const Safety& safety)
{
    const QString importId = startTime.toString(QLatin1String("yyyyMMdd-HHmmss")) + QLatin1Char('-') +
                             QString::number(QRandomGenerator::global()->bounded(0x10000), 16);

    QJsonObject record;
    record.insert(QLatin1String("version"),  1);
    record.insert(QLatin1String("date"),     startTime.toOffsetFromUtc(startTime.offsetFromUtc()).toString(Qt::ISODate));
    record.insert(QLatin1String("device"),   device);
    record.insert(QLatin1String("source"),   source);
    record.insert(QLatin1String("computer"), QSysInfo::machineHostName());
    record.insert(QLatin1String("skipped"),  skipped);
    record.insert(QLatin1String("files"),    QJsonArray::fromStringList(copied));

    if (!inboxId.isEmpty())
    {
        record.insert(QLatin1String("inbox"), inboxId);
    }

    // What can be deleted from the device (see deviceSafety()).

    record.insert(QLatin1String("complete"), safety.complete);

    if (safety.safeBefore.isValid())
    {
        record.insert(QLatin1String("safeBefore"), safety.safeBefore.toOffsetFromUtc(safety.safeBefore.offsetFromUtc())
                                                                    .toString(Qt::ISODate));
    }

    const QString dir = root + QLatin1Char('/') + QLatin1String(s_historyFolder);
    QDir().mkpath(dir);
    hideFolder(dir);

    QSaveFile file(dir + QLatin1Char('/') + importId + QLatin1String(".json"));

    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(record).toJson());
        file.commit();
    }
    else
    {
        qCWarning(DIGIKAM_GENERAL_LOG) << "Photos import: cannot write the import record in" << dir;
    }

    return importId;
}

// --- Imports without the sheet (inboxes) ------------------------------------------

void PhotosImporter::importInBackground(const BackgroundJob& job)
{
    m_queue << job;
    startNextBackground();
}

bool PhotosImporter::backgroundBusy(const QString& inboxId) const
{
    if (m_backgroundWatcher.isRunning() && (m_currentJob.inboxId == inboxId))
    {
        return true;
    }

    for (const BackgroundJob& job : std::as_const(m_queue))
    {
        if (job.inboxId == inboxId)
        {
            return true;
        }
    }

    return false;
}

void PhotosImporter::startNextBackground()
{
    if (
        m_backgroundWatcher.isRunning()                      ||
        m_queue.isEmpty()                                    ||
        (m_state == Scanning) || (m_state == Importing)
       )
    {
        return;
    }

    m_currentJob          = m_queue.takeFirst();
    m_currentJob.started  = QDateTime::currentDateTime();
    const int hashVersion = CoreDbAccess().db()->getUniqueHashVersion();

    m_backgroundWatcher.setFuture(QtConcurrent::run(&importBackground, m_currentJob, hashVersion));
}

void PhotosImporter::slotBackgroundImported()
{
    const BackgroundResult result = m_backgroundWatcher.result();
    QString importId;

    if (!result.copied.isEmpty())
    {
        // An inbox holds what the device sent: all of it is in the library
        // when nothing failed.

        Safety safety;
        safety.complete   = (result.failed == 0);
        safety.safeBefore = safety.complete ? m_currentJob.started : QDateTime();

        importId = writeRecord(m_currentJob.root, m_currentJob.device, m_currentJob.source, result.copied,
                               result.existing, m_currentJob.started, m_currentJob.inboxId, safety);

        reloadHistory();

        Q_EMIT imported(importId, result.copied.size());
    }

    Q_EMIT backgroundImported(m_currentJob.inboxId, m_currentJob.device, result.copied.size(),
                              result.failed, importId, result.handled);

    startNextBackground();
}

void PhotosImporter::cancel()
{
    m_cancel.storeRelaxed(1);
}

void PhotosImporter::reset()
{
    if ((m_state == Scanning) || (m_state == Importing))
    {
        return;
    }

    m_scan = ScanResult();
    m_summary.clear();
    setState(Idle);
}

void PhotosImporter::reloadHistory()
{
    QVariantList history;
    QHash<QString, QStringList> deviceFiles;
    QHash<QString, QVariantMap> safety;
    QHash<QString, QDateTime>   sourceDate;
    const QLocale locale;

    m_deviceOfSource.clear();

    // The newest check of a device wins.

    auto addCheck = [&safety] (const QString& device, const QVariantMap& check)
    {
        const QDateTime checkedOn = check.value(QLatin1String("checkedOn")).toDateTime();

        if (
            !device.isEmpty() && checkedOn.isValid() &&
            (safety.value(device).value(QLatin1String("checkedOn")).toDateTime() < checkedOn)
           )
        {
            safety.insert(device, check);
        }
    };

    const QStringList roots = libraryFolders();

    for (const QString& root : roots)
    {
        // Also digiKam's trash folder, created visible on Windows.

        hideFolder(root + QLatin1String("/.dtrash"));

        const QDir dir(root + QLatin1Char('/') + QLatin1String(s_historyFolder));
        const QStringList files = dir.entryList(QStringList() << QLatin1String("*.json"), QDir::Files);

        for (const QString& name : files)
        {
            QFile file(dir.filePath(name));

            if (!file.open(QIODevice::ReadOnly))
            {
                continue;
            }

            const QJsonObject record = QJsonDocument::fromJson(file.readAll()).object();

            // Device checks (devices.json, and the conflict copies sync tools make of it).

            if (name.startsWith(QLatin1String("devices")))
            {
                for (auto it = record.constBegin() ; it != record.constEnd() ; ++it)
                {
                    const QDateTime checkedOn = QDateTime::fromString(it.value().toObject()
                                                                        .value(QLatin1String("checkedOn")).toString(),
                                                                      Qt::ISODate).toLocalTime();
                    QVariantMap check;
                    check.insert(QLatin1String("checkedOn"),  checkedOn);
                    check.insert(QLatin1String("safeBefore"), checkedOn);
                    check.insert(QLatin1String("complete"),   true);
                    addCheck(it.key(), check);
                }

                continue;
            }

            // Not an import record (inboxes.json...).

            if (!record.value(QLatin1String("files")).isArray())
            {
                continue;
            }

            // Photos moved here from another library folder: their device only.

            if (record.value(QLatin1String("kind")).toString() == QLatin1String("move"))
            {
                const QString device = record.value(QLatin1String("device")).toString();

                if (!device.isEmpty())
                {
                    QStringList& paths = deviceFiles[device];

                    for (const QJsonValue& file : record.value(QLatin1String("files")).toArray())
                    {
                        paths << root + QLatin1Char('/') + file.toString();
                    }
                }

                continue;
            }

            const QDateTime date = QDateTime::fromString(record.value(QLatin1String("date")).toString(), Qt::ISODate)
                                                                                                    .toLocalTime();

            QVariantMap entry;
            entry.insert(QLatin1String("id"),       QFileInfo(name).completeBaseName());
            entry.insert(QLatin1String("date"),     date);
            entry.insert(QLatin1String("dateText"), QString(locale.toString(date.date(), QLocale::LongFormat) +
                                                    QString::fromUtf8(" \u00B7 ") +
                                                    locale.toString(date.time(), QLocale::ShortFormat)));
            entry.insert(QLatin1String("device"),   record.value(QLatin1String("device")).toString());
            entry.insert(QLatin1String("source"),   record.value(QLatin1String("source")).toString());
            entry.insert(QLatin1String("computer"), record.value(QLatin1String("computer")).toString());
            entry.insert(QLatin1String("count"),    record.value(QLatin1String("files")).toArray().size());
            entry.insert(QLatin1String("undone"),   record.contains(QLatin1String("undone")));
            entry.insert(QLatin1String("inbox"),    record.value(QLatin1String("inbox")).toString());
            history << entry;

            const QString device = record.value(QLatin1String("device")).toString();
            const bool undone    = record.contains(QLatin1String("undone"));

            if (!device.isEmpty() && !undone && record.contains(QLatin1String("complete")))
            {
                QVariantMap check;
                check.insert(QLatin1String("checkedOn"),  date);
                check.insert(QLatin1String("complete"),   record.value(QLatin1String("complete")).toBool());
                check.insert(QLatin1String("inbox"),      !record.value(QLatin1String("inbox")).toString().isEmpty());
                check.insert(QLatin1String("safeBefore"),
                             QDateTime::fromString(record.value(QLatin1String("safeBefore")).toString(), Qt::ISODate)
                                                                                                    .toLocalTime());
                addCheck(device, check);
            }

            // The device name to suggest when importing from this source again.

            const QString source = QDir::cleanPath(record.value(QLatin1String("source")).toString());

            if (
                !device.isEmpty() && !undone && !source.isEmpty() &&
                record.value(QLatin1String("inbox")).toString().isEmpty() &&
                (!sourceDate.contains(source) || (sourceDate.value(source) < date))
               )
            {
                sourceDate.insert(source, date);
                m_deviceOfSource.insert(source, device);
            }

            if (!device.isEmpty())
            {
                QStringList& paths = deviceFiles[device];

                for (const QJsonValue& file : record.value(QLatin1String("files")).toArray())
                {
                    paths << root + QLatin1Char('/') + file.toString();
                }
            }
        }
    }

    std::sort(history.begin(), history.end(),
              [] (const QVariant& a, const QVariant& b)
        {
            return (a.toMap().value(QLatin1String("date")).toDateTime() >
                    b.toMap().value(QLatin1String("date")).toDateTime());
        }
    );

    if ((history != m_history) || (safety != m_safety))
    {
        m_history = history;
        m_safety  = safety;

        Q_EMIT historyChanged();
    }

    // Devices: which files are still there (in the background, stat calls).

    m_deviceFiles = deviceFiles;

    if (m_devicesWatcher.isRunning())
    {
        m_devicesPending = true;

        return;
    }

    m_devicesWatcher.setFuture(QtConcurrent::run(&countDeviceFiles, deviceFiles));
}

QString PhotosImporter::deviceOfFile(const QString& filePath) const
{
    return m_deviceOfPath.value(filePath);
}

void PhotosImporter::recordMoved(const QString& root, const QHash<QString, QStringList>& relativeFiles)
{
    const QString dir = root + QLatin1Char('/') + QLatin1String(s_historyFolder);
    QDir().mkpath(dir);
    hideFolder(dir);

    const QDateTime now = QDateTime::currentDateTime();

    for (auto it = relativeFiles.constBegin() ; it != relativeFiles.constEnd() ; ++it)
    {
        const QString id = now.toString(QLatin1String("yyyyMMdd-HHmmss")) + QLatin1String("-moved-") +
                           QString::number(QRandomGenerator::global()->bounded(0x10000), 16);

        QJsonObject record;
        record.insert(QLatin1String("version"),  1);
        record.insert(QLatin1String("kind"),     QLatin1String("move"));
        record.insert(QLatin1String("date"),     now.toOffsetFromUtc(now.offsetFromUtc()).toString(Qt::ISODate));
        record.insert(QLatin1String("device"),   it.key());
        record.insert(QLatin1String("computer"), QSysInfo::machineHostName());
        record.insert(QLatin1String("files"),    QJsonArray::fromStringList(it.value()));

        QSaveFile file(dir + QLatin1Char('/') + id + QLatin1String(".json"));

        if (file.open(QIODevice::WriteOnly))
        {
            file.write(QJsonDocument(record).toJson());
            file.commit();
        }
    }

    reloadHistory();
}

void PhotosImporter::writeDeviceCheck(const QString& root, const QString& device, const QDateTime& checkedOn)
{
    const QString dir  = root + QLatin1Char('/') + QLatin1String(s_historyFolder);
    const QString path = dir + QLatin1String("/devices.json");

    QDir().mkpath(dir);
    hideFolder(dir);

    QJsonObject json;
    QFile in(path);

    if (in.open(QIODevice::ReadOnly))
    {
        json = QJsonDocument::fromJson(in.readAll()).object();
        in.close();
    }

    QJsonObject check;
    check.insert(QLatin1String("checkedOn"), checkedOn.toOffsetFromUtc(checkedOn.offsetFromUtc()).toString(Qt::ISODate));
    check.insert(QLatin1String("computer"),  QSysInfo::machineHostName());
    json.insert(device, check);

    QSaveFile out(path);

    if (out.open(QIODevice::WriteOnly))
    {
        out.write(QJsonDocument(json).toJson());
        out.commit();
    }
}

QVariantMap PhotosImporter::deviceSafety(const QString& name) const
{
    QVariantMap check = m_safety.value(name);

    if (check.isEmpty())
    {
        return check;
    }

    const QLocale locale;

    auto when = [&locale] (const QDateTime& date)
    {
        const QDate today = QDate::currentDate();

        if (date.date() == today)
        {
            return i18nc("@info: time today", "today at %1", locale.toString(date.time(), QLocale::ShortFormat));
        }

        return i18nc("@info: date and time", "%1 at %2",
                     locale.toString(date.date(), QLocale::LongFormat),
                     locale.toString(date.time(), QLocale::ShortFormat));
    };

    const QDateTime checkedOn  = check.value(QLatin1String("checkedOn")).toDateTime();
    const QDateTime safeBefore = check.value(QLatin1String("safeBefore")).toDateTime();
    const bool complete        = check.value(QLatin1String("complete")).toBool();
    const bool inbox           = check.value(QLatin1String("inbox")).toBool();
    QString text;

    if (inbox)
    {
        text = complete ? i18n("Everything %1 sent up to %2 is in the library.", name, when(checkedOn))
                        : i18n("Some photos %1 sent on %2 could not be imported: keep them on the device "
                               "for now.", name, when(checkedOn));
    }
    else if (complete)
    {
        text = i18n("Everything that was on %1 %2 is in the library. You can delete those photos and videos "
                    "from it.", name, when(checkedOn));
    }
    else if (safeBefore.isValid())
    {
        text = i18n("Photos and videos taken before %1 are in the library: you can delete those from %2. "
                    "Keep the newer ones until they are imported.", when(safeBefore), name);
    }
    else
    {
        text = i18n("Not everything on %1 could be imported: keep its photos for now.", name);
    }

    check.insert(QLatin1String("known"), true);
    check.insert(QLatin1String("text"),  text);

    return check;
}

QVariantList PhotosImporter::devices() const
{
    return m_devices;
}

void PhotosImporter::slotDevicesCounted()
{
    const DeviceCount result = m_devicesWatcher.result();

    if (result.devices != m_devices)
    {
        m_devices = result.devices;

        Q_EMIT devicesChanged();
    }

    m_deviceOfPath = result.deviceOfPath;
    m_library->setImportDevices(result.deviceOfPath);

    // The device shown got new photos (an import, a move): show them.

    const QString key = m_library->filesKey();

    if (
        (m_library->filter() == PhotosLibraryModel::Files) &&
        key.startsWith(QLatin1String("device:"))           &&
        (m_deviceFiles.value(key.mid(7)) != m_shownDeviceFiles)
       )
    {
        showDevice(key.mid(7));
    }

    if (m_devicesPending)
    {
        m_devicesPending = false;
        m_devicesWatcher.setFuture(QtConcurrent::run(&countDeviceFiles, m_deviceFiles));
    }
}

void PhotosImporter::showDevice(const QString& name)
{
    m_shownDeviceFiles = m_deviceFiles.value(name);
    m_library->showFiles(m_deviceFiles.value(name), name, QLatin1String("device:") + name);
}

QVariantMap PhotosImporter::readImport(const QString& importId, QString* const filePath) const
{
    const QStringList roots = libraryFolders();

    for (const QString& root : roots)
    {
        const QString path = root + QLatin1Char('/') + QLatin1String(s_historyFolder) +
                             QLatin1Char('/') + importId + QLatin1String(".json");
        QFile file(path);

        if (file.open(QIODevice::ReadOnly))
        {
            QVariantMap record = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
            record.insert(QLatin1String("root"), root);

            if (filePath)
            {
                *filePath = path;
            }

            return record;
        }
    }

    return QVariantMap();
}

QStringList PhotosImporter::importedPaths(const QVariantMap& record) const
{
    const QString root = record.value(QLatin1String("root")).toString();
    QStringList paths;

    for (const QVariant& file : record.value(QLatin1String("files")).toList())
    {
        paths << root + QLatin1Char('/') + file.toString();
    }

    return paths;
}

void PhotosImporter::showImport(const QString& importId)
{
    const QVariantMap record = readImport(importId);

    if (record.isEmpty())
    {
        return;
    }

    const QDateTime date  = QDateTime::fromString(record.value(QLatin1String("date")).toString(), Qt::ISODate)
                                                                                              .toLocalTime();
    const QString device  = record.value(QLatin1String("device")).toString();
    const QLocale locale;
    QString title         = i18n("Imported %1", locale.toString(date.date(), QLocale::ShortFormat));

    if (!device.isEmpty())
    {
        title += QString::fromUtf8(" \u00B7 ") + device;
    }

    m_library->showFiles(importedPaths(record), title, importId);
}

int PhotosImporter::remainingCount(const QString& importId) const
{
    int count = 0;

    for (const QString& path : importedPaths(readImport(importId)))
    {
        if (QFileInfo::exists(path))
        {
            ++count;
        }
    }

    return count;
}

int PhotosImporter::undoImport(const QString& importId)
{
    QString recordPath;
    const QVariantMap record = readImport(importId, &recordPath);

    if (record.isEmpty())
    {
        return 0;
    }

    QList<qlonglong> ids;

    for (const QString& path : importedPaths(record))
    {
        if (!QFileInfo::exists(path))
        {
            continue;
        }

        const ItemInfo info = ItemInfo::fromLocalFile(path);

        if (!info.isNull() && info.isVisible())
        {
            ids << info.id();
        }
    }

    // digiKam's trash: restorable from "Recently Deleted" (or the classic trash view).

    m_library->trashImageIds(ids);

    // Keep the record, marked as undone.

    QFile file(recordPath);

    if (file.open(QIODevice::ReadOnly))
    {
        QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        json.insert(QLatin1String("undone"), QDateTime::currentDateTime().toString(Qt::ISODate));

        QSaveFile out(recordPath);

        if (out.open(QIODevice::WriteOnly))
        {
            out.write(QJsonDocument(json).toJson());
            out.commit();
        }
    }

    reloadHistory();

    return ids.size();
}

} // namespace Digikam
