/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - inbox folders: folders where phones and
 *               other devices drop new photos (sync apps, uploads),
 *               imported into the library automatically.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosinboxes.h"

// Qt includes

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QtConcurrentRun>

// KDE includes

#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>

// Local includes

#include "digikam_debug.h"
#include "collectionmanager.h"
#include "coredb.h"
#include "coredbaccess.h"
#include "photosimporter.h"
#include "scancontroller.h"

namespace Digikam
{

namespace
{

const char* const s_definitionsFile = ".photos-imports/inboxes.json";
const char* const s_localGroup      = "Photos Inboxes";

const int s_pollSeconds     = 30;   ///< listing interval
const int s_stableSeconds   = 30;   ///< unchanged for this long: complete
const int s_maxWaitSeconds  = 300;  ///< import even if other files are still arriving
const int s_retrySeconds    = 600;  ///< a file which failed is tried again after

QString hostName()
{
    return QSysInfo::machineHostName();
}

QString cleanPath(const QString& path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();

    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

bool isInside(const QString& path, const QString& folder)
{
    const Qt::CaseSensitivity cs =
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
                                   Qt::CaseInsensitive;
#else
                                   Qt::CaseSensitive;
#endif

    return (path.compare(folder, cs) == 0) || path.startsWith(folder + QLatin1Char('/'), cs);
}

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

PhotosInboxes::Listing listInboxes(const QList<PhotosInboxes::Definition>& definitions,
                                   const QSet<QString>& suffixes)
{
    PhotosInboxes::Listing listing;

    for (const PhotosInboxes::Definition& inbox : definitions)
    {
        const QDir dir(inbox.path);
        const bool found = !inbox.path.isEmpty() && dir.exists();
        listing.found.insert(inbox.id, found);

        if (!found)
        {
            continue;
        }

        QList<PhotosInboxes::Stamp>& stamps = listing.files[inbox.id];
        QDirIterator it(inbox.path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

        while (it.hasNext())
        {
            const QString path = it.next();

            if (PhotosInboxes::isTemporaryName(dir.relativeFilePath(path)))
            {
                continue;
            }

            const QFileInfo info = it.fileInfo();

            if (!suffixes.contains(info.suffix().toLower()))
            {
                continue;
            }

            PhotosInboxes::Stamp stamp;
            stamp.path     = path;
            stamp.size     = info.size();
            stamp.modified = info.lastModified();
            stamps << stamp;
        }
    }

    return listing;
}

} // namespace

bool PhotosInboxes::isTemporaryName(const QString& relativePath)
{
    const QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    for (const QString& part : parts)
    {
        // Hidden files and folders: .stversions, .thumbnails, .trashed-*,
        // ._resource forks, Syncthing's .syncthing.NAME.tmp...

        if (part.startsWith(QLatin1Char('.')) || part.startsWith(QLatin1Char('~')))
        {
            return true;
        }
    }

    const QString name = parts.isEmpty() ? relativePath : parts.constLast();

    static const char* const suffixes[] =
    {
        ".tmp", ".temp", ".part", ".partial", ".crdownload", ".download", ".!sync", ".filepart"
    };

    for (const char* const suffix : suffixes)
    {
        if (name.endsWith(QLatin1String(suffix), Qt::CaseInsensitive))
        {
            return true;
        }
    }

    return (
            name.contains(QLatin1String(".syncthing."), Qt::CaseInsensitive) ||
            name.contains(QLatin1String("sync-conflict"), Qt::CaseInsensitive) ||
            name.contains(QLatin1String("conflicted copy"), Qt::CaseInsensitive)
           );
}

PhotosInboxes::PhotosInboxes(PhotosImporter* const importer, QObject* const parent)
    : QObject   (parent),
      m_importer(importer)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(s_pollSeconds * 1000);

    connect(m_pollTimer, &QTimer::timeout,
            this, &PhotosInboxes::checkNow);

    // A change reported by the system: look a bit later, the writer is
    // probably not done.

    m_soonTimer = new QTimer(this);
    m_soonTimer->setSingleShot(true);
    m_soonTimer->setInterval(5000);

    connect(m_soonTimer, &QTimer::timeout,
            this, &PhotosInboxes::checkNow);

    m_watcher = new QFileSystemWatcher(this);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this] ()
        {
            if (!m_soonTimer->isActive())
            {
                m_soonTimer->start();
            }
        }
    );

    connect(&m_listWatcher, &QFutureWatcher<Listing>::finished,
            this, &PhotosInboxes::slotListed);

    connect(m_importer, &PhotosImporter::backgroundImported,
            this, &PhotosInboxes::slotBackgroundImported);

    connect(m_importer, &PhotosImporter::historyChanged,
            this, &PhotosInboxes::inboxesChanged);

    // Definitions come with the library folders (synced), which may be
    // mounted later (network shares, removable drives).

    connect(CollectionManager::instance(), &CollectionManager::locationStatusChanged,
            this, [this] ()
        {
            m_definitionStamps.clear();
            checkNow();
        }
    );

    QTimer::singleShot(5000, this, [this] ()
        {
            checkNow();
            m_pollTimer->start();
        }
    );
}

PhotosInboxes::~PhotosInboxes()
{
    m_listWatcher.waitForFinished();
}

// --- Definitions ------------------------------------------------------------------

void PhotosInboxes::loadDefinitions()
{
    QList<Definition> definitions;
    const KConfigGroup local = KSharedConfig::openConfig()->group(QLatin1String(s_localGroup));

    for (const QString& root : CollectionManager::instance()->allAvailableAlbumRootPaths())
    {
        QFile file(root + QLatin1Char('/') + QLatin1String(s_definitionsFile));

        if (!file.open(QIODevice::ReadOnly))
        {
            continue;
        }

        const QJsonArray array = QJsonDocument::fromJson(file.readAll()).object()
                                                                       .value(QLatin1String("inboxes")).toArray();

        for (const QJsonValue& value : array)
        {
            const QJsonObject object = value.toObject();

            Definition inbox;
            inbox.id         = object.value(QLatin1String("id")).toString();
            inbox.root       = root;
            inbox.storedPath = object.value(QLatin1String("path")).toString();
            inbox.device     = object.value(QLatin1String("device")).toString();
            inbox.importer   = object.value(QLatin1String("importer")).toString();
            inbox.move       = (object.value(QLatin1String("mode")).toString() != QLatin1String("leave"));

            if (inbox.id.isEmpty() || inbox.storedPath.isEmpty())
            {
                continue;
            }

            // Inside the library folder: relative, the same on every computer.
            // Elsewhere: this computer's path, or where it was located here.

            const QString located = local.readEntry(inbox.id, QString());

            if      (!located.isEmpty())
            {
                inbox.path = located;
            }
            else if (QDir::isRelativePath(inbox.storedPath))
            {
                inbox.path = QDir::cleanPath(root + QLatin1Char('/') + inbox.storedPath);
            }
            else
            {
                inbox.path = inbox.storedPath;
            }

            definitions << inbox;
            ignoreInLibrary(inbox);
        }
    }

    m_definitions = definitions;

    updateWatcher();

    Q_EMIT inboxesChanged();
}

bool PhotosInboxes::saveDefinitions(const QString& root)
{
    QJsonArray array;

    for (const Definition& inbox : std::as_const(m_definitions))
    {
        if (inbox.root != root)
        {
            continue;
        }

        QJsonObject object;
        object.insert(QLatin1String("id"),       inbox.id);
        object.insert(QLatin1String("path"),     inbox.storedPath);
        object.insert(QLatin1String("device"),   inbox.device);
        object.insert(QLatin1String("mode"),     inbox.move ? QLatin1String("move") : QLatin1String("leave"));
        object.insert(QLatin1String("importer"), inbox.importer);
        array << object;
    }

    QJsonObject json;
    json.insert(QLatin1String("version"), 1);
    json.insert(QLatin1String("inboxes"), array);

    const QString path = root + QLatin1Char('/') + QLatin1String(s_definitionsFile);
    QDir().mkpath(QFileInfo(path).path());

    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        qCWarning(DIGIKAM_GENERAL_LOG) << "Photos inboxes: cannot write" << path;

        return false;
    }

    file.write(QJsonDocument(json).toJson());

    return file.commit();
}

void PhotosInboxes::ignoreInLibrary(const Definition& inbox)
{
    // An inbox inside a library folder must not be indexed: its photos would
    // appear twice. digiKam ignores folders by name.

    if (!QDir::isRelativePath(inbox.storedPath))
    {
        return;
    }

    const QString name = inbox.storedPath.section(QLatin1Char('/'), 0, 0, QString::SectionSkipEmpty);

    if (name.isEmpty())
    {
        return;
    }

    QString current;
    CoreDbAccess().db()->getUserIgnoreDirectoryFilterSettings(&current);
    QStringList names = current.split(QLatin1Char(';'), Qt::SkipEmptyParts);

    if (names.contains(name))
    {
        return;
    }

    names << name;
    CoreDbAccess().db()->setUserIgnoreDirectoryFilterSettings(names);

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos inboxes: folders named" << name << "are not indexed";

    // Drops what was indexed in it already.

    ScanController::instance()->completeCollectionScanInBackground(false, true);
}

void PhotosInboxes::updateWatcher()
{
    if (!m_watcher->directories().isEmpty())
    {
        m_watcher->removePaths(m_watcher->directories());
    }

    for (const Definition& inbox : std::as_const(m_definitions))
    {
        if (!inbox.path.isEmpty() && QFileInfo(inbox.path).isDir())
        {
            m_watcher->addPath(inbox.path);
        }
    }
}

QVariantList PhotosInboxes::inboxes() const
{
    QVariantList list;
    const QLocale locale;
    const QVariantList history = m_importer->history();     // newest first

    for (const Definition& inbox : m_definitions)
    {
        const bool here      = (inbox.importer == hostName());
        const bool found     = m_found.value(inbox.id, QFileInfo(inbox.path).isDir());
        const bool importing = m_importer->backgroundBusy(inbox.id);
        int waiting          = 0;

        for (const FileState& state : m_state.value(inbox.id))
        {
            if (!state.handled)
            {
                ++waiting;
            }
        }

        QString lastImport;

        for (const QVariant& entry : history)
        {
            const QVariantMap map = entry.toMap();

            if (map.value(QLatin1String("inbox")).toString() == inbox.id)
            {
                lastImport = map.value(QLatin1String("dateText")).toString();
                break;
            }
        }

        QString status;

        if      (!found)
        {
            status = i18n("Folder not found on this computer");
        }
        else if (importing)
        {
            status = i18n("Importing…");
        }
        else if (!here)
        {
            status = (waiting > 0) ? i18np("1 waiting, imported by %2", "%1 waiting, imported by %2", waiting, inbox.importer)
                                   : i18n("Imported by %1", inbox.importer);
        }
        else if (waiting > 0)
        {
            status = i18np("1 waiting", "%1 waiting", waiting);
        }
        else
        {
            status = i18n("Up to date");
        }

        if (!m_lastError.value(inbox.id).isEmpty())
        {
            status += QLatin1String(" · ") + m_lastError.value(inbox.id);
        }

        QVariantMap map;
        map.insert(QLatin1String("id"),           inbox.id);
        map.insert(QLatin1String("path"),         QDir::toNativeSeparators(inbox.path));
        map.insert(QLatin1String("root"),         inbox.root);
        map.insert(QLatin1String("device"),       inbox.device);
        map.insert(QLatin1String("mode"),         inbox.move ? QLatin1String("move") : QLatin1String("leave"));
        map.insert(QLatin1String("importer"),     inbox.importer);
        map.insert(QLatin1String("thisComputer"), here);
        map.insert(QLatin1String("found"),        found);
        map.insert(QLatin1String("waiting"),      waiting);
        map.insert(QLatin1String("importing"),    importing);
        map.insert(QLatin1String("lastImport"),   lastImport);
        map.insert(QLatin1String("status"),       status);
        list << map;
    }

    return list;
}

// --- QML API ------------------------------------------------------------------------

QString PhotosInboxes::chooseFolder() const
{
    return QFileDialog::getExistingDirectory(QApplication::activeWindow(),
                                             i18n("Inbox folder"),
                                             QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
}

QString PhotosInboxes::suggestedDevice(const QString& folder) const
{
    QString name = QFileInfo(folder).fileName();
    name.replace(QLatin1Char('-'), QLatin1Char(' ')).replace(QLatin1Char('_'), QLatin1Char(' '));

    return name.simplified();
}

QString PhotosInboxes::libraryFolderOf(const QString& folder) const
{
    const QString path = cleanPath(folder);

    for (const QString& root : CollectionManager::instance()->allAvailableAlbumRootPaths())
    {
        if (isInside(path, cleanPath(root)))
        {
            return root;
        }
    }

    return QString();
}

QString PhotosInboxes::addInbox(const QString& folder, const QString& device, bool move, const QString& root)
{
    const QString path = cleanPath(folder);

    if (!QFileInfo(path).isDir())
    {
        return i18n("%1 is not a folder.", QDir::toNativeSeparators(path));
    }

    QString target = libraryFolderOf(path);

    if (target.isEmpty())
    {
        target = root.isEmpty() ? m_importer->importFolder() : root;
    }

    if (target.isEmpty())
    {
        return i18n("Add a library folder first.");
    }

    for (const QString& libraryRoot : CollectionManager::instance()->allAvailableAlbumRootPaths())
    {
        if (isInside(cleanPath(libraryRoot), path))
        {
            return i18n("An inbox cannot be a library folder, or contain one.");
        }
    }

    for (const Definition& inbox : std::as_const(m_definitions))
    {
        if (cleanPath(inbox.path) == path)
        {
            return i18n("This folder is already an inbox (%1).", inbox.device);
        }
    }

    Definition inbox;
    inbox.id         = QString::number(QRandomGenerator::global()->generate64() & 0xFFFFFFFFFFFFULL, 16);
    inbox.root       = target;
    inbox.storedPath = isInside(path, cleanPath(target)) ? QDir(cleanPath(target)).relativeFilePath(path) : path;
    inbox.path       = path;
    inbox.device     = device.simplified();
    inbox.importer   = hostName();
    inbox.move       = move;

    m_definitions << inbox;

    if (!saveDefinitions(target))
    {
        m_definitions.removeLast();

        return i18n("The inbox could not be saved in %1.", QDir::toNativeSeparators(target));
    }

    ignoreInLibrary(inbox);
    updateWatcher();

    Q_EMIT inboxesChanged();

    checkNow();

    return QString();
}

void PhotosInboxes::removeInbox(const QString& id)
{
    for (int i = 0 ; i < m_definitions.size() ; ++i)
    {
        if (m_definitions.at(i).id == id)
        {
            const QString root = m_definitions.at(i).root;
            m_definitions.removeAt(i);
            m_state.remove(id);
            saveDefinitions(root);

            KConfigGroup local = KSharedConfig::openConfig()->group(QLatin1String(s_localGroup));
            local.deleteEntry(id);

            updateWatcher();

            Q_EMIT inboxesChanged();

            return;
        }
    }
}

void PhotosInboxes::importOnThisComputer(const QString& id)
{
    for (Definition& inbox : m_definitions)
    {
        if (inbox.id == id)
        {
            inbox.importer = hostName();
            saveDefinitions(inbox.root);

            Q_EMIT inboxesChanged();

            checkNow();

            return;
        }
    }
}

void PhotosInboxes::locateInbox(const QString& id, const QString& folder)
{
    KConfigGroup local = KSharedConfig::openConfig()->group(QLatin1String(s_localGroup));
    local.writeEntry(id, cleanPath(folder));
    local.sync();

    loadDefinitions();
    checkNow();
}

void PhotosInboxes::checkNow()
{
    // Inboxes added, removed or taken over on another computer arrive with
    // the sync of the library folders.

    QHash<QString, QDateTime> stamps;

    for (const QString& root : CollectionManager::instance()->allAvailableAlbumRootPaths())
    {
        const QFileInfo info(root + QLatin1Char('/') + QLatin1String(s_definitionsFile));

        if (info.exists())
        {
            stamps.insert(root, info.lastModified());
        }
    }

    if (stamps != m_definitionStamps)
    {
        m_definitionStamps = stamps;
        loadDefinitions();
    }

    if (m_definitions.isEmpty())
    {
        return;
    }

    if (m_listWatcher.isRunning())
    {
        m_listAgain = true;

        return;
    }

    m_listWatcher.setFuture(QtConcurrent::run(&listInboxes, m_definitions, mediaSuffixes()));
}

// --- Importing ------------------------------------------------------------------------

void PhotosInboxes::slotListed()
{
    const Listing listing = m_listWatcher.result();
    const QDateTime now   = QDateTime::currentDateTime();

    for (const Definition& inbox : std::as_const(m_definitions))
    {
        m_found.insert(inbox.id, listing.found.value(inbox.id, false));

        QHash<QString, FileState>& states = m_state[inbox.id];
        QSet<QString> listed;

        for (const Stamp& stamp : listing.files.value(inbox.id))
        {
            listed.insert(stamp.path);
            FileState& state = states[stamp.path];

            if (!state.changedAt.isValid() || (state.size != stamp.size) || (state.modified != stamp.modified))
            {
                // New, or still being written.

                state.size       = stamp.size;
                state.modified   = stamp.modified;
                state.changedAt  = now;
                state.handled    = false;
                state.retryAfter = QDateTime();
            }
        }

        // Gone: moved into the library, or removed by the sync.

        states.removeIf([&listed] (const QHash<QString, FileState>::iterator& it)
            {
                return !listed.contains(it.key());
            }
        );

        if ((inbox.importer != hostName()) || m_importer->backgroundBusy(inbox.id))
        {
            continue;
        }

        QStringList ready;
        int  arriving       = 0;
        bool waitedTooLong  = false;

        for (auto it = states.begin() ; it != states.end() ; ++it)
        {
            FileState& state = it.value();

            if (state.handled || (state.retryAfter.isValid() && (now < state.retryAfter)))
            {
                continue;
            }

            const qint64 quiet = state.changedAt.secsTo(now);

            if (quiet < s_stableSeconds)
            {
                ++arriving;
            }
            else
            {
                ready << it.key();
                waitedTooLong |= (quiet > s_maxWaitSeconds);
            }
        }

        // Files arriving together form one import: wait for the rest (e.g.
        // the video of a Live Photo), but not forever.

        if (ready.isEmpty() || ((arriving > 0) && !waitedTooLong))
        {
            continue;
        }

        for (const QString& path : std::as_const(ready))
        {
            states[path].retryAfter = now.addSecs(s_retrySeconds);
        }

        PhotosImporter::BackgroundJob job;
        job.inboxId     = inbox.id;
        job.device      = inbox.device;
        job.source      = inbox.path;
        job.root        = inbox.root;
        job.files       = ready;
        job.moveSources = inbox.move;

        qCDebug(DIGIKAM_GENERAL_LOG) << "Photos inbox" << inbox.device << ":" << ready.size() << "files to import";

        m_importer->importInBackground(job);
    }

    Q_EMIT inboxesChanged();

    if (m_listAgain)
    {
        m_listAgain = false;
        checkNow();
    }
}

void PhotosInboxes::slotBackgroundImported(const QString& inboxId, const QString&, int, int failed,
                                           const QString&, const QStringList& handled)
{
    QHash<QString, FileState>& states = m_state[inboxId];

    for (const QString& path : handled)
    {
        if (states.contains(path))
        {
            states[path].handled = true;
        }
    }

    if (failed > 0)
    {
        m_lastError.insert(inboxId, i18np("1 file could not be imported", "%1 files could not be imported", failed));
    }
    else
    {
        m_lastError.remove(inboxId);
    }

    Q_EMIT inboxesChanged();

    // The next files, if more arrived meanwhile.

    checkNow();
}

} // namespace Digikam
