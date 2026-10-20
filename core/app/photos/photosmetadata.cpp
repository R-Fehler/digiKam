/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - where favorites, albums, captions and
 *               people are stored: XMP sidecar files next to the
 *               photos (default), or the library database only.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosmetadata.h"

// Qt includes

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QtConcurrentRun>

#if defined(Q_OS_WIN)
#   include <windows.h>
#elif defined(Q_OS_MACOS)
#   include <sys/stat.h>
#   include <unistd.h>
#endif

// KDE includes

#include <kconfiggroup.h>
#include <ksharedconfig.h>

// Local includes

#include "digikam_debug.h"
#include "collectionmanager.h"
#include "coredbaccess.h"
#include "coredbbackend.h"
#include "coredbchangesets.h"
#include "coredbconstants.h"
#include "coredbwatch.h"
#include "dmetadata.h"
#include "iteminfo.h"
#include "iteminfolist.h"
#include "metadatasynchronizer.h"
#include "metaengine.h"
#include "metaenginesettings.h"
#include "metaenginesettingscontainer.h"
#include "tagscache.h"

namespace Digikam
{

namespace
{

const char* const s_photosGroup  = "Photos Mode";
const char* const s_pendingEntry = "Sidecar Sync Pending";

} // namespace

bool PhotosMetadata::sidecarMode(const MetaEngineSettingsContainer& settings)
{
    return (settings.metadataWritingMode == MetaEngine::WRITE_TO_SIDECAR_ONLY) &&
           settings.useXMPSidecar4Reading                                      &&
           settings.rescanImageIfModified                                      &&
           settings.saveRating                                                 &&
           settings.saveTags;
}

void PhotosMetadata::setSidecarMode(MetaEngineSettingsContainer& settings, bool sidecars)
{
    settings.saveTags              = sidecars;
    settings.saveFaceTags          = sidecars;
    settings.savePosition          = sidecars;
    settings.saveComments          = sidecars;
    settings.saveDateTime          = sidecars;
    settings.savePickLabel         = sidecars;
    settings.saveColorLabel        = sidecars;
    settings.saveRating            = sidecars;
    settings.useXMPSidecar4Reading = sidecars;
    settings.rescanImageIfModified = sidecars;
    settings.metadataWritingMode   = sidecars ? MetaEngine::WRITE_TO_SIDECAR_ONLY
                                              : MetaEngine::WRITE_TO_FILE_ONLY;
}

void PhotosMetadata::writeSidecarDefaults(KConfigGroup& group)
{
    // Keys of MetaEngineSettingsContainer::readFromConfig().

    const char* const saves[] =
    {
        "Save Tags",
        "Save FaceTags",
        "Save Position",
        "Save EXIF Comments",
        "Save Date Time",
        "Save Pick Label",
        "Save Color Label",
        "Save Rating"
    };

    for (const char* const key : saves)
    {
        group.writeEntry(key, true);
    }

    group.writeEntry("Use XMP Sidecar For Reading", true);
    group.writeEntry("Rescan File If Modified",     true);
    group.writeEntry("Metadata Writing Mode",       int(MetaEngine::WRITE_TO_SIDECAR_ONLY));

    KConfigGroup photos = group.config()->group(QLatin1String(s_photosGroup));
    photos.writeEntry(s_pendingEntry, true);
}

// ---------------------------------------------------------------------------------------

PhotosSidecarSync::PhotosSidecarSync(QObject* const parent)
    : QObject(parent)
{
}

PhotosSidecarSync::~PhotosSidecarSync()
{
    if (m_tool)
    {
        m_tool->cancel();
    }
}

int PhotosSidecarSync::progress() const
{
    return m_progress;
}

void PhotosSidecarSync::setProgress(int percent)
{
    if (percent != m_progress)
    {
        m_progress = percent;

        Q_EMIT signalProgressChanged();
    }
}

void PhotosSidecarSync::restart()
{
    KConfigGroup photos = KSharedConfig::openConfig()->group(QLatin1String(s_photosGroup));
    photos.writeEntry(s_pendingEntry, true);
    photos.sync();

    startIfPending();
}

/**
 * Photos (not in a sidecar yet) whose rating or tags (albums, people, labels)
 * may only be in the database. Captions are left out: most come from the
 * files themselves (camera comments), writing them would create a sidecar
 * for nearly every photo.
 */
static QList<qlonglong> photosWithDatabaseOnlyInfo()
{
    QList<QVariant> rated;
    QList<QVariant> tagged;

    {
        CoreDbAccess access;

        access.backend()->execSql(QString::fromLatin1("SELECT imageid FROM ImageInformation WHERE rating > 0;"),
                                  &rated);

        access.backend()->execSql(QString::fromLatin1("SELECT imageid, tagid FROM ImageTags;"),
                                  &tagged);
    }

    QSet<qlonglong> ids;

    for (const QVariant& id : std::as_const(rated))
    {
        ids.insert(id.toLongLong());
    }

    TagsCache* const tags = TagsCache::instance();

    for (int i = 0 ; (i + 1) < tagged.size() ; i += 2)
    {
        if (!tags->isInternalTag(tagged.at(i + 1).toInt()))
        {
            ids.insert(tagged.at(i).toLongLong());
        }
    }

    // Keep visible photos and videos without a sidecar.

    QList<qlonglong> result;

    if (ids.isEmpty())
    {
        return result;
    }

    QList<QVariant> values;

    {
        CoreDbAccess access;

        access.backend()->execSql(QString::fromLatin1(
            "SELECT Images.id, Images.name, Albums.albumRoot, Albums.relativePath "
            "FROM Images INNER JOIN Albums ON Albums.id = Images.album "
            "WHERE Images.status = %1;").arg(int(DatabaseItem::Visible)), &values);
    }

    QHash<int, QString> roots;

    for (int i = 0 ; (i + 3) < values.size() ; i += 4)
    {
        const qlonglong id = values.at(i).toLongLong();

        if (!ids.contains(id))
        {
            continue;
        }

        const int rootId = values.at(i + 2).toInt();

        if (!roots.contains(rootId))
        {
            roots.insert(rootId, CollectionManager::instance()->albumRootPath(rootId));
        }

        const QString root = roots.value(rootId);

        if (root.isEmpty())
        {
            continue;
        }

        const QString relative = values.at(i + 3).toString();
        const QString path     = root + ((relative == QLatin1String("/")) ? QString() : relative) +
                                 QLatin1Char('/') + values.at(i + 1).toString();

        if (!MetaEngine::hasSidecar(path))
        {
            result << id;
        }
    }

    return result;
}

void PhotosSidecarSync::startIfPending()
{
    if (m_tool || (m_progress >= 0))
    {
        return;
    }

    KConfigGroup photos = KSharedConfig::openConfig()->group(QLatin1String(s_photosGroup));

    if (!photos.readEntry(s_pendingEntry, false) ||
        !PhotosMetadata::sidecarMode(MetaEngineSettings::instance()->settings()))
    {
        return;
    }

    setProgress(0);

    auto* const watcher = new QFutureWatcher<QList<qlonglong> >(this);

    connect(watcher, &QFutureWatcher<QList<qlonglong> >::finished,
            this, [this, watcher] ()
        {
            const QList<qlonglong> ids = watcher->result();
            watcher->deleteLater();

            qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode: writing database-only information of"
                                         << ids.size() << "photos to sidecars";

            if (ids.isEmpty())
            {
                finish();

                return;
            }

            MetadataSynchronizer* const tool = new MetadataSynchronizer(ItemInfoList(ids),
                                                                        MetadataSynchronizer::WriteFromDatabaseToFile);
            tool->setNotificationEnabled(false);
            tool->setUseMultiCoreCPU(true);
            m_tool = tool;

            connect(tool, &ProgressItem::progressItemProgress,
                    this, [this] (ProgressItem*, unsigned int percent)
                {
                    setProgress(qBound(0, int(percent), 99));
                }
            );

            connect(tool, &MaintenanceTool::signalComplete,
                    this, &PhotosSidecarSync::finish);

            connect(tool, &MaintenanceTool::signalCanceled,
                    this, [this] ()
                {
                    // Try again at the next start.

                    setProgress(-1);
                }
            );

            tool->start();
        }
    );

    watcher->setFuture(QtConcurrent::run(&photosWithDatabaseOnlyInfo));
}

void PhotosSidecarSync::finish()
{
    KConfigGroup photos = KSharedConfig::openConfig()->group(QLatin1String(s_photosGroup));
    photos.writeEntry(s_pendingEntry, false);
    photos.sync();

    setProgress(-1);
}

// ---------------------------------------------------------------------------------------

namespace
{

const char* const s_hideEntry = "Hide Sidecars";

/// Sidecars of the folder: "IMG.HEIC.xmp" or "IMG.xmp" next to a file of the same name.
QStringList sidecarsIn(const QDir& dir)
{
    const QStringList names = dir.entryList(QDir::Files | QDir::Hidden | QDir::System);
    const QSet<QString> nameSet(names.constBegin(), names.constEnd());
    QSet<QString> baseNames;

    for (const QString& name : names)
    {
        if (!name.endsWith(QLatin1String(".xmp"), Qt::CaseInsensitive))
        {
            baseNames.insert(QFileInfo(name).completeBaseName().toLower());
        }
    }

    QStringList sidecars;

    for (const QString& name : names)
    {
        if (
            name.endsWith(QLatin1String(".xmp"), Qt::CaseInsensitive) &&
            (nameSet.contains(name.chopped(4)) || baseNames.contains(QFileInfo(name).completeBaseName().toLower()))
           )
        {
            sidecars << name;
        }
    }

    return sidecars;
}

} // namespace

PhotosSidecarVisibility::PhotosSidecarVisibility(QObject* const parent)
    : QObject(parent)
{
    m_hidden = KSharedConfig::openConfig()->group(QLatin1String(s_photosGroup)).readEntry(s_hideEntry, false);

    QTimer* const timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(3000);
    m_timer = timer;

    connect(timer, &QTimer::timeout,
            this, &PhotosSidecarVisibility::slotApplyScheduled);

    // Sidecars are written after favorites, albums, captions... change.

    CoreDbWatch* const watch = CoreDbAccess::databaseWatch();

    connect(watch, &CoreDbWatch::imageChange,
            this, [this] (const ImageChangeset& changeset)
        {
            if (m_hidden)
            {
                QStringList paths;

                for (const qlonglong id : changeset.ids())
                {
                    paths << ItemInfo(id).filePath();
                }

                scheduleFolders(paths);
            }
        }
    );

    connect(watch, &CoreDbWatch::imageTagChange,
            this, [this] (const ImageTagChangeset& changeset)
        {
            if (m_hidden)
            {
                QStringList paths;

                for (const qlonglong id : changeset.ids())
                {
                    paths << ItemInfo(id).filePath();
                }

                scheduleFolders(paths);
            }
        }
    );

    // New and rescanned files: imports, restores, sidecars synced from other computers.

    connect(watch, &CoreDbWatch::collectionImageChange,
            this, [this] (const CollectionImageChangeset& changeset)
        {
            if (m_hidden)
            {
                QStringList paths;

                for (const qlonglong id : changeset.ids())
                {
                    paths << ItemInfo(id).filePath();
                }

                scheduleFolders(paths);
            }
        }
    );

    if (m_hidden)
    {
        // Sidecars synced from other computers: hide them here too.

        QTimer::singleShot(15000, this, &PhotosSidecarVisibility::applyAll);
    }
}

PhotosSidecarVisibility::~PhotosSidecarVisibility()
{
}

bool PhotosSidecarVisibility::hidden() const
{
    return m_hidden;
}

void PhotosSidecarVisibility::setHidden(bool hidden)
{
    if (hidden == m_hidden)
    {
        return;
    }

    m_hidden = hidden;

    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(s_photosGroup));
    group.writeEntry(s_hideEntry, hidden);
    group.sync();

    applyAll();

    Q_EMIT signalHiddenChanged();
}

void PhotosSidecarVisibility::applyAll()
{
    const QStringList roots = CollectionManager::instance()->allAvailableAlbumRootPaths();
    const bool hide         = m_hidden;

    (void)QtConcurrent::run([roots, hide] ()
        {
            for (const QString& root : roots)
            {
                applyToTree(root, hide);
            }
        }
    );
}

void PhotosSidecarVisibility::scheduleFolders(const QStringList& filePaths)
{
    for (const QString& path : filePaths)
    {
        if (!path.isEmpty())
        {
            const QString folder = QFileInfo(path).path();

            if (!m_scheduled.contains(folder))
            {
                m_scheduled << folder;
            }
        }
    }

    if (!m_scheduled.isEmpty())
    {
        static_cast<QTimer*>(m_timer)->start();
    }
}

void PhotosSidecarVisibility::slotApplyScheduled()
{
    const QStringList folders = m_scheduled;
    const bool hide           = m_hidden;
    m_scheduled.clear();

    (void)QtConcurrent::run([folders, hide] ()
        {
            for (const QString& folder : folders)
            {
                applyToFolder(folder, hide);
            }
        }
    );
}

void PhotosSidecarVisibility::applyToTree(const QString& root, bool hide)
{
    applyToFolder(root, hide);

    // Not into hidden folders (.dtrash, .photos-imports...).

    QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        const QString folder = it.next();

        if (!folder.mid(root.size()).contains(QLatin1String("/.")))
        {
            applyToFolder(folder, hide);
        }
    }
}

void PhotosSidecarVisibility::applyToFolder(const QString& folder, bool hide)
{
    const QDir dir(folder);
    const QStringList sidecars = sidecarsIn(dir);

#if defined(Q_OS_WIN)

    for (const QString& name : sidecars)
    {
        const std::wstring native = QDir::toNativeSeparators(dir.filePath(name)).toStdWString();
        const DWORD attributes    = GetFileAttributesW(native.c_str());

        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }

        const DWORD wanted = hide ? (attributes | FILE_ATTRIBUTE_HIDDEN) : (attributes & ~FILE_ATTRIBUTE_HIDDEN);

        if (wanted != attributes)
        {
            SetFileAttributesW(native.c_str(), wanted);
        }
    }

#elif defined(Q_OS_MACOS)

    for (const QString& name : sidecars)
    {
        const QByteArray path = QFile::encodeName(dir.filePath(name));
        struct stat st;

        if (lstat(path.constData(), &st) != 0)
        {
            continue;
        }

        const unsigned int wanted = hide ? (st.st_flags | UF_HIDDEN) : (st.st_flags & ~UF_HIDDEN);

        if (wanted != st.st_flags)
        {
            chflags(path.constData(), wanted);
        }
    }

#else

    // ".hidden": one name per line. Keep the entries of the user.

    const QString hiddenFile = dir.filePath(QLatin1String(".hidden"));
    QStringList lines;
    QFile file(hiddenFile);

    if (file.open(QIODevice::ReadOnly))
    {
        lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        file.close();
    }

    QStringList wanted = lines;

    if (hide)
    {
        for (const QString& name : sidecars)
        {
            if (!wanted.contains(name))
            {
                wanted << name;
            }
        }
    }
    else
    {
        wanted.removeIf([] (const QString& line)
            {
                return line.endsWith(QLatin1String(".xmp"), Qt::CaseInsensitive);
            }
        );
    }

    if (wanted == lines)
    {
        return;
    }

    if (wanted.isEmpty())
    {
        QFile::remove(hiddenFile);

        return;
    }

    QSaveFile out(hiddenFile);

    if (out.open(QIODevice::WriteOnly))
    {
        out.write(wanted.join(QLatin1Char('\n')).toUtf8() + '\n');
        out.commit();
    }

#endif

}

} // namespace Digikam
