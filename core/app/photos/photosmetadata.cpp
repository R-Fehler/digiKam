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

#include <QFutureWatcher>
#include <QSet>
#include <QtConcurrentRun>

// KDE includes

#include <kconfiggroup.h>
#include <ksharedconfig.h>

// Local includes

#include "digikam_debug.h"
#include "collectionmanager.h"
#include "coredbaccess.h"
#include "coredbbackend.h"
#include "coredbconstants.h"
#include "dmetadata.h"
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

} // namespace Digikam
