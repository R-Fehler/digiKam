/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - moving photos between the personal and
 *               the shared library (library folders of each kind).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosmover.h"

// Qt includes

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QtConcurrentRun>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "collectionlocation.h"
#include "collectionmanager.h"
#include "coredb.h"
#include "coredbaccess.h"
#include "iteminfo.h"
#include "photosimporter.h"
#include "photoslibraries.h"
#include "photoslibrarymodel.h"

namespace Digikam
{

namespace
{

/// "IMG_0001.HEIC", else "IMG_0001_1.HEIC"... (a sidecar there counts too).
QString freeDestination(const QString& dir, const QFileInfo& source)
{
    auto taken = [] (const QString& path)
    {
        return (QFileInfo::exists(path) || QFileInfo::exists(path + QLatin1String(".xmp")));
    };

    QString dest = dir + QLatin1Char('/') + source.fileName();

    for (int i = 1 ; taken(dest) ; ++i)
    {
        dest = dir + QLatin1Char('/') + source.completeBaseName() +
               QString::fromLatin1("_%1.").arg(i) + source.suffix();
    }

    return dest;
}

/// Same drive: a rename. Else a copy under a hidden name, checked, then the original removed.
bool moveFile(const QString& from, const QString& to)
{
    if (QDir().rename(from, to))
    {
        return true;
    }

    const QFileInfo target(to);
    const QString temp = target.path() + QLatin1String("/.") + target.fileName() + QLatin1String(".part");

    QFile::remove(temp);

    if (!QFile::copy(from, temp))
    {
        QFile::remove(temp);

        return false;
    }

    if ((QFileInfo(temp).size() != QFileInfo(from).size()) || !QDir().rename(temp, to))
    {
        QFile::remove(temp);

        return false;
    }

    {
        QFile file(to);

        if (file.open(QIODevice::ReadWrite))
        {
            file.setFileTime(QFileInfo(from).lastModified(), QFileDevice::FileModificationTime);
        }
    }

    if (!QFile::remove(from))
    {
        // Two copies are better than none: the original stays.

        qCWarning(DIGIKAM_GENERAL_LOG) << "Photos move: cannot remove" << from << "after copying it";
    }

    return true;
}

/// The sidecars of a file, both naming schemes, and their names next to its new name.
QList<QPair<QString, QString> > sidecarsOf(const QString& from, const QString& to)
{
    QList<QPair<QString, QString> > sidecars;
    const QFileInfo source(from);
    const QFileInfo target(to);

    for (const QString& suffix : { QStringLiteral(".xmp"), QStringLiteral(".XMP") })
    {
        if (QFileInfo::exists(from + suffix))
        {
            sidecars << qMakePair(from + suffix, to + suffix);
        }

        const QString base = source.path() + QLatin1Char('/') + source.completeBaseName() + suffix;

        if (QFileInfo::exists(base))
        {
            sidecars << qMakePair(base, target.path() + QLatin1Char('/') + target.completeBaseName() + suffix);
        }
    }

    return sidecars;
}

PhotosMover::Result moveJobs(const QList<PhotosMover::Job>& jobs, int targetRootId, const QString& targetRoot)
{
    PhotosMover::Result result;

    for (const PhotosMover::Job& job : jobs)
    {
        const QString dir = (job.relativeDir == QLatin1String("/")) ? targetRoot : targetRoot + job.relativeDir;

        if (!QDir().mkpath(dir))
        {
            ++result.failed;
            continue;
        }

        const QString dest = freeDestination(dir, QFileInfo(job.path));
        const QList<QPair<QString, QString> > sidecars = sidecarsOf(job.path, dest);

        if (!moveFile(job.path, dest))
        {
            qCWarning(DIGIKAM_GENERAL_LOG) << "Photos move: cannot move" << job.path << "to" << dest;
            ++result.failed;
            continue;
        }

        for (const QPair<QString, QString>& sidecar : sidecars)
        {
            moveFile(sidecar.first, sidecar.second);
        }

        // The database entry follows the file: albums, favorites and the rest stay.

        {
            CoreDbAccess access;
            const int albumId = access.db()->getAlbumForPath(targetRootId, job.relativeDir, true);

            if (albumId > 0)
            {
                access.db()->moveItem(job.albumId, job.name, albumId, QFileInfo(dest).fileName());
            }
        }

        result.moved << qMakePair(job.path, dest);
    }

    return result;
}

} // namespace

PhotosMover::PhotosMover(PhotosLibraryModel* const library, PhotosLibraries* const libraries,
                         PhotosImporter* const importer, QObject* const parent)
    : QObject    (parent),
      m_library  (library),
      m_libraries(libraries),
      m_importer (importer)
{
    connect(&m_watcher, &QFutureWatcher<Result>::finished,
            this, &PhotosMover::slotMoved);
}

PhotosMover::~PhotosMover()
{
    m_watcher.waitForFinished();
}

bool PhotosMover::busy() const
{
    return m_watcher.isRunning();
}

void PhotosMover::moveSelection(bool toShared)
{
    if (busy())
    {
        return;
    }

    const QString targetRoot = m_libraries->targetFolder(toShared);

    if (targetRoot.isEmpty())
    {
        Q_EMIT failed(toShared ? i18n("No shared library folder is connected.")
                               : i18n("No personal library folder is connected."));

        return;
    }

    const CollectionLocation target = CollectionManager::instance()->locationForPath(targetRoot);
    const QSet<int> shared          = m_libraries->sharedRootIds();
    const QList<int> rows           = m_library->selectedRows();
    const QList<PhotosEntry>& all   = m_library->entries();
    QList<Job> jobs;

    auto addJob = [&jobs, &shared, toShared] (const ItemInfo& info)
    {
        if (info.isNull() || (shared.contains(info.albumRootId()) == toShared))
        {
            return;     // in a library of that kind already
        }

        Job job;
        job.id          = info.id();
        job.path        = info.filePath();
        job.relativeDir = info.relativePath();
        job.albumId     = info.albumId();
        job.name        = info.name();
        jobs << job;
    };

    for (const int row : rows)
    {
        const PhotosEntry& entry = all.at(row);

        addJob(ItemInfo(entry.id));

        if (!entry.livePath.isEmpty())
        {
            addJob(ItemInfo::fromLocalFile(entry.livePath));    // the video of a Live Photo
        }
    }

    if (jobs.isEmpty())
    {
        return;
    }

    m_targetRoot = targetRoot;
    m_toShared   = toShared;

    m_library->clearSelection();
    m_watcher.setFuture(QtConcurrent::run(&moveJobs, jobs, target.id(), targetRoot));

    Q_EMIT busyChanged();
}

void PhotosMover::slotMoved()
{
    const Result result = m_watcher.result();

    // The devices photos were imported from stay with them.

    QHash<QString, QStringList> byDevice;
    const QDir root(m_targetRoot);

    for (const QPair<QString, QString>& move : result.moved)
    {
        const QString device = m_importer->deviceOfFile(move.first);

        if (!device.isEmpty())
        {
            byDevice[device] << root.relativeFilePath(move.second);
        }
    }

    if (!byDevice.isEmpty())
    {
        m_importer->recordMoved(m_targetRoot, byDevice);
    }

    Q_EMIT busyChanged();
    Q_EMIT moved(result.moved.size(), result.failed, m_toShared);
}

} // namespace Digikam
