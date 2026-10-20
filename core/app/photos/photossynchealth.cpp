/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - keeping a synced or shared library in
 *               shape: rescans where changes are not reported, and
 *               merging of sidecar sync conflicts.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photossynchealth.h"

// Qt includes

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTimer>
#include <QtConcurrentRun>

// Local includes

#include "digikam_debug.h"
#include "captionvalues.h"
#include "collectionmanager.h"
#include "disjointmetadata.h"
#include "dmetadata.h"
#include "dtrash.h"
#include "fileactionmngr.h"
#include "iteminfo.h"
#include "metaenginesettings.h"
#include "photoslibraries.h"
#include "photosmetadata.h"
#include "photossyncnames.h"
#include "scancontroller.h"
#include "tagscache.h"

namespace Digikam
{

namespace
{

const int s_networkMinutes  = 10;
const int s_comebackMinutes = 15;

QList<QPair<QString, QString> > findConflicts(const QStringList& roots)
{
    QList<QPair<QString, QString> > conflicts;

    const QStringList patterns =
    {
        QLatin1String("*.sync-conflict-*"),
        QLatin1String("*conflicted copy*"),
        QLatin1String("*_conflict-*")
    };

    for (const QString& root : roots)
    {
        QDirIterator it(root, patterns, QDir::Files, QDirIterator::Subdirectories);

        while (it.hasNext())
        {
            const QString path = it.next();

            if (path.contains(QLatin1String("/.")))
            {
                continue;       // .dtrash, .stversions...
            }

            const QString original = PhotosSyncHealth::conflictOriginal(it.fileName());

            if (!original.isEmpty())
            {
                const QString sidecar = it.fileInfo().path() + QLatin1Char('/') + original;

                if (QFileInfo::exists(sidecar))
                {
                    conflicts << qMakePair(path, sidecar);
                }
            }
        }
    }

    return conflicts;
}

} // namespace

QString PhotosSyncHealth::conflictOriginal(const QString& fileName)
{
    return photosConflictOriginal(fileName);
}

PhotosSyncHealth::PhotosSyncHealth(QObject* const parent)
    : QObject(parent)
{
    m_lastFullScan = QDateTime::currentDateTime();     // digiKam scans everything at start

    m_timer = new QTimer(this);
    m_timer->setInterval(s_networkMinutes * 60 * 1000);

    connect(m_timer, &QTimer::timeout,
            this, [this] ()
        {
            run(false);
        }
    );

    connect(&m_conflicts, &QFutureWatcher<QList<QPair<QString, QString> > >::finished,
            this, &PhotosSyncHealth::slotConflictsFound);

    // Back to Photos after a while (other window, sleep): what changed meanwhile.

    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, [this] (Qt::ApplicationState state)
        {
            if (
                (state == Qt::ApplicationActive) &&
                (m_lastFullScan.secsTo(QDateTime::currentDateTime()) > s_comebackMinutes * 60)
               )
            {
                run(true);
            }
        }
    );

    QTimer::singleShot(60 * 1000, this, [this] ()
        {
            run(false);
            m_timer->start();
        }
    );
}

PhotosSyncHealth::~PhotosSyncHealth()
{
    m_conflicts.waitForFinished();
}

void PhotosSyncHealth::run(bool allFolders)
{
    const QStringList roots = CollectionManager::instance()->allAvailableAlbumRootPaths();

    for (const QString& root : roots)
    {
        if (allFolders || PhotosLibraries::isNetworkPath(root))
        {
            ScanController::instance()->scheduleCollectionScanRelaxed(root);
        }
    }

    if (allFolders)
    {
        m_lastFullScan = QDateTime::currentDateTime();
    }

    if (
        !m_conflicts.isRunning() &&
        PhotosMetadata::sidecarMode(MetaEngineSettings::instance()->settings())
       )
    {
        m_conflicts.setFuture(QtConcurrent::run(&findConflicts, roots));
    }
}

void PhotosSyncHealth::slotConflictsFound()
{
    const QList<QPair<QString, QString> > conflicts = m_conflicts.result();
    int merged = 0;

    for (const QPair<QString, QString>& conflict : conflicts)
    {
        if (mergeConflict(conflict.first, conflict.second))
        {
            ++merged;
        }
    }

    if (merged > 0)
    {
        Q_EMIT conflictsMerged(merged);
    }
}

bool PhotosSyncHealth::mergeConflict(const QString& conflictPath, const QString& sidecarPath)
{
    // The photo: "IMG.JPG" for "IMG.JPG.xmp", else the file named "IMG.*" for "IMG.xmp".

    const QFileInfo sidecar(sidecarPath);
    QString photo = sidecarPath.chopped(4);

    if (!QFileInfo::exists(photo))
    {
        photo.clear();
        const QStringList candidates = sidecar.dir().entryList(QStringList() << sidecar.completeBaseName() + QLatin1String(".*"),
                                                              QDir::Files);

        for (const QString& name : candidates)
        {
            if (!name.endsWith(QLatin1String(".xmp"), Qt::CaseInsensitive))
            {
                photo = sidecar.dir().filePath(name);
                break;
            }
        }
    }

    if (photo.isEmpty())
    {
        return false;
    }

    const ItemInfo info = ItemInfo::fromLocalFile(photo);

    if (info.isNull())
    {
        return false;   // Not scanned yet: next time.
    }

    DMetadata copy;

    if (!copy.load(conflictPath))
    {
        qCWarning(DIGIKAM_GENERAL_LOG) << "Photos sync: cannot read the conflict copy" << conflictPath;

        return false;
    }

    DisjointMetadata hub;
    hub.load(info);
    bool changed = false;

    // Albums, people, tags: of both.

    QStringList tagPaths;
    copy.getItemTagsPath(tagPaths);

    const QList<int> current = info.tagIds();

    for (const QString& path : std::as_const(tagPaths))
    {
        const int tagId = TagsCache::instance()->getOrCreateTag(path);

        if ((tagId > 0) && !TagsCache::instance()->isInternalTag(tagId) && !current.contains(tagId))
        {
            hub.setTag(tagId);
            changed = true;
        }
    }

    // The higher rating (a favorite stays a favorite).

    const int rating = copy.getItemRating();

    if (rating > info.rating())
    {
        hub.setRating(rating);
        changed = true;
    }

    // Caption: the one there is, or the newer one.

    const CaptionsMap comments = copy.getItemComments();
    const QString theirs       = comments.value(QLatin1String("x-default")).caption.trimmed();
    const QString ours         = info.comment().trimmed();

    if (
        !theirs.isEmpty() && (theirs != ours) &&
        (ours.isEmpty() || (QFileInfo(conflictPath).lastModified() > sidecar.lastModified()))
       )
    {
        hub.setComments(comments);
        changed = true;
    }

    if (changed)
    {
        FileActionMngr::instance()->applyMetadata(QList<ItemInfo>() << info, hub);
    }

    // Kept in digiKam's trash, in case.

    DTrash::deleteImage(conflictPath, QDateTime::currentDateTime());

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos sync: merged the conflict copy" << conflictPath
                                 << (changed ? "(changes taken)" : "(nothing new)");

    return true;
}

} // namespace Digikam
