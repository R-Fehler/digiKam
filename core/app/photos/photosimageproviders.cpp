/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - Qt Quick image providers backed by
 *               digiKam's thumbnail database and preview loaders.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosimageproviders.h"

// Qt includes

#include <QApplication>
#include <QFile>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPixmap>
#include <QPointer>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QThread>
#include <QTimer>

// C includes

#if defined(__GLIBC__)
#   include <malloc.h>
#endif

// KDE includes

#include <kmemoryinfo.h>

// Local includes

#include "digikam_debug.h"
#include "dimg.h"
#include "loadingcache.h"
#include "loadingdescription.h"
#include "photoscachepolicy.h"
#include "photoslibrarymodel.h"
#include "previewloadthread.h"
#include "thumbnailloadthread.h"
#include "thumbnailsize.h"

namespace Digikam
{

PhotosThumbnailBroker::PhotosThumbnailBroker(QObject* const parent)
    : QObject(parent)
{
    // Thumbnail sizes: a small one for dense zoom levels (7x less memory than
    // the large one), the stored size (512 px with large thumbnails), and on
    // HiDPI screens 1024 px for very large tiles. digiKam's loader refuses
    // sizes above 512 px unless the screen is HiDPI.

    const int large = ThumbnailLoadThread::maximumThumbnailSize();

    if (qEnvironmentVariableIsSet("DIGIKAM_PHOTOS_SINGLE_SIZE"))
    {
        m_sizes << large;       // Debug / benchmark switch: no size ladder.
    }
    else
    {
        m_sizes << 192 << large;

        if ((qApp->devicePixelRatio() > 1.0) && (large < ThumbnailSize::MAX))
        {
            m_sizes << ThumbnailSize::MAX;
        }
    }

    // Generating thumbnails for a library seen for the first time is CPU bound
    // (decoding full size camera files): one loader per core. They run at a
    // lowered priority so that the user interface stays fluid. digiKam's thread
    // pool (cores + 1 threads, shared by all loaders) bounds the real concurrency.

    const int cores = qMax(1, QThread::idealThreadCount());
    int loaders     = qBound(2, cores, 16);
    int pregens     = qBound(1, cores, 16);     // lowest priority, only while idle
    bool ok         = false;
    int env         = qEnvironmentVariableIntValue("DIGIKAM_PHOTOS_THUMB_THREADS", &ok);

    if (ok && (env > 0))
    {
        loaders = qMin(env, 32);
    }

    env = qEnvironmentVariableIntValue("DIGIKAM_PHOTOS_PREGEN_THREADS", &ok);

    if (ok && (env > 0))
    {
        pregens = qMin(env, 32);
    }

    auto makeLoader = [this] (QThread::Priority priority)
    {
        ThumbnailLoadThread* const loader = new ThumbnailLoadThread(this);

        // We only want QImages of exactly the requested size: no QPixmap
        // conversion, no painted border, no work in the GUI thread.

        loader->setPixmapRequested(false);
        loader->setHighlightPixmap(false);
        loader->setSendSurrogatePixmap(false);
        loader->setThumbnailSize(m_sizes.last());
        loader->setPriority(priority);

        return loader;
    };

    for (int i = 0 ; i < loaders ; ++i)
    {
        ThumbnailLoadThread* const loader = makeLoader(QThread::LowPriority);

        connect(loader, &ThumbnailLoadThread::signalQImageThumbnailLoaded,
                this, &PhotosThumbnailBroker::slotImageLoaded);

        // Not connected to signalThumbnailLoaded(QPixmap): pixmaps coming from the
        // shared cache of the classic views carry a painted 1 px border. A cache hit
        // there also schedules a load, which delivers the clean QImage above.

        m_loaders << loader;
    }

    for (int i = 0 ; i < pregens ; ++i)
    {
        m_pregenerators << makeLoader(QThread::LowestPriority);
    }

    m_pregenTimer = new QTimer(this);
    m_pregenTimer->setInterval(100);

    connect(m_pregenTimer, &QTimer::timeout,
            this, &PhotosThumbnailBroker::slotPregenerationTick);

    // Memory budget of the decoded-thumbnail cache: 20% of the physical memory,
    // between 512 MiB and 8 GiB (e.g. 1.6 GiB with 8 GiB of RAM, holding about
    // 2200 large or 15000 small thumbnails). This is what makes scrolling back
    // instant: Qt Quick itself only keeps the images of instantiated tiles,
    // plus 2 MiB of recently released ones.
    // The budget shrinks when the system runs low on memory, see slotCheckMemory().

    m_cacheBudget = cacheBudgetKiB();
    m_cache.setMaxCost(m_cacheBudget);

    m_memoryTimer = new QTimer(this);
    m_memoryTimer->setInterval(5000);

    connect(m_memoryTimer, &QTimer::timeout,
            this, &PhotosThumbnailBroker::slotCheckMemory);

    m_memoryTimer->start();

    // Scaling a cached large thumbnail down to a smaller size.

    m_scalePool.setMaxThreadCount(qBound(1, cores / 2, 4));

    // Files modified on disk (edited, rotated...): digiKam's loading cache
    // is notified, and so are we. May be emitted under the cache lock.

    connect(LoadingCache::cache(), &LoadingCache::fileChanged,
            this, &PhotosThumbnailBroker::slotFileChanged,
            Qt::QueuedConnection);

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode: thumbnail loaders:" << loaders
                                 << "pre-generators:" << pregens << "sizes:" << m_sizes
                                 << "cache MiB:" << (m_cache.maxCost() / 1024);
}

PhotosThumbnailBroker::~PhotosThumbnailBroker()
{
    m_pregenTimer->stop();

    for (ThumbnailLoadThread* const loader : std::as_const(m_pregenerators))
    {
        loader->stopAllTasks();
    }

    for (ThumbnailLoadThread* const loader : std::as_const(m_loaders))
    {
        loader->stopAllTasks();
    }

    for (ThumbnailLoadThread* const loader : std::as_const(m_pregenerators))
    {
        loader->wait();
    }

    for (ThumbnailLoadThread* const loader : std::as_const(m_loaders))
    {
        loader->wait();
    }

    m_scalePool.waitForDone();
}

namespace
{

#ifdef Q_OS_LINUX

qint64 readKiB(const QString& path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        return -1;
    }

    bool ok            = false;
    const qint64 bytes = file.readAll().trimmed().toLongLong(&ok);    // "max" (unlimited) fails

    return ok ? (bytes / 1024) : -1;
}

/// Value of a key of a cgroup memory.stat file, in KiB (0 if missing).
qint64 statKiB(const QString& path, const QByteArray& key)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        return 0;
    }

    const QList<QByteArray> lines = file.readAll().split('\n');
    const QByteArray prefix       = QByteArray(key).append(' ');

    for (const QByteArray& line : lines)
    {
        if (line.startsWith(prefix))
        {
            return line.mid(key.size() + 1).trimmed().toLongLong() / 1024;
        }
    }

    return 0;
}

/**
 * Memory limit and usage of our cgroup, in KiB, for cgroup v2 and v1.
 * Inactive file cache is not counted as used: the kernel reclaims it first.
 */
bool cgroupMemory(qint64& limitKiB, qint64& usedKiB)
{
    QFile file(QLatin1String("/proc/self/cgroup"));

    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QString v2Path;
    QString v1Path;
    const QStringList lines = QString::fromLatin1(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const QString& line : lines)
    {
        if      (line.startsWith(QLatin1String("0::")))
        {
            v2Path = line.mid(3);
        }
        else if (line.section(QLatin1Char(':'), 1, 1).split(QLatin1Char(',')).contains(QLatin1String("memory")))
        {
            v1Path = line.section(QLatin1Char(':'), 2);
        }
    }

    // cgroup v2 (unified hierarchy, possibly mounted below "unified" on hybrid systems).

    const QStringList v2Roots = { QLatin1String("/sys/fs/cgroup"), QLatin1String("/sys/fs/cgroup/unified") };

    for (const QString& root : v2Roots)
    {
        const QString dir = root + v2Path;

        if (!v2Path.isEmpty() && QFile::exists(dir + QLatin1String("/memory.max")))
        {
            limitKiB = readKiB(dir + QLatin1String("/memory.max"));
            usedKiB  = readKiB(dir + QLatin1String("/memory.current")) -
                       statKiB(dir + QLatin1String("/memory.stat"), "inactive_file");

            return ((limitKiB > 0) && (usedKiB >= 0));
        }
    }

    // cgroup v1 memory controller.

    if (!v1Path.isEmpty())
    {
        const QString dir = QLatin1String("/sys/fs/cgroup/memory") + v1Path;
        limitKiB          = readKiB(dir + QLatin1String("/memory.limit_in_bytes"));
        usedKiB           = readKiB(dir + QLatin1String("/memory.usage_in_bytes")) -
                            statKiB(dir + QLatin1String("/memory.stat"), "total_inactive_file");

        return ((limitKiB > 0) && (usedKiB >= 0));
    }

    return false;
}

#endif

/**
 * Physical memory and memory available, in KiB. On Linux, a cgroup v2 memory
 * limit (Flatpak, Snap, systemd slices, containers) is taken into account:
 * /proc/meminfo, read by KMemoryInfo, only knows about the whole machine.
 */
void memoryState(qint64& totalKiB, qint64& availableKiB)
{
    totalKiB     = 0;
    availableKiB = -1;

    const KMemoryInfo memory;

    if (!memory.isNull())
    {
        totalKiB     = qint64(memory.totalPhysical()     / 1024);
        availableKiB = qint64(memory.availablePhysical() / 1024);
    }

#ifdef Q_OS_LINUX

    qint64 limit = 0;
    qint64 used  = 0;

    if (cgroupMemory(limit, used) && (limit > 0))
    {
        totalKiB     = (totalKiB > 0)      ? qMin(totalKiB, limit)            : limit;
        availableKiB = (availableKiB >= 0) ? qMin(availableKiB, limit - used) : (limit - used);
    }

#endif

}

} // namespace

qint64 PhotosThumbnailBroker::cacheBudgetKiB()
{
    bool ok          = false;
    const int envMiB = qEnvironmentVariableIntValue("DIGIKAM_PHOTOS_CACHE_MB", &ok);

    if (ok && (envMiB > 0))
    {
        return qint64(envMiB) * 1024;
    }

    qint64 totalKiB     = 0;
    qint64 availableKiB = 0;
    memoryState(totalKiB, availableKiB);

    return PhotosCachePolicy::defaultBudgetKiB(totalKiB);
}

void PhotosThumbnailBroker::slotCheckMemory()
{
    qint64 totalKiB     = 0;
    qint64 availableKiB = 0;
    memoryState(totalKiB, availableKiB);

    const qint64 current = m_cache.maxCost();
    const qint64 maxCost = PhotosCachePolicy::nextMaxCostKiB(current, m_cache.totalCost(), m_cacheBudget,
                                                             totalKiB, availableKiB);

    if (maxCost == current)
    {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_cache.setMaxCost(maxCost);        // evicts least recently used entries
    }

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode: thumbnail cache budget" << (maxCost / 1024)
                                 << "MiB, available memory" << (availableKiB / 1024) << "MiB";

#if defined(__GLIBC__)

    // Thumbnails are small allocations which glibc may keep in its heap:
    // really return the freed memory to the system.

    if (maxCost < current)
    {
        malloc_trim(0);
    }

#endif

}

QList<int> PhotosThumbnailBroker::sizes() const
{
    return m_sizes;
}

int PhotosThumbnailBroker::loaderCount() const
{
    return m_loaders.size();
}

int PhotosThumbnailBroker::pregeneratorCount() const
{
    return m_pregenerators.size();
}

QString PhotosThumbnailBroker::cacheKey(const QString& filePath, int size)
{
    return QString::number(size) + QLatin1Char(':') + filePath;
}

ThumbnailLoadThread* PhotosThumbnailBroker::loaderFor(const QString& filePath) const
{
    // Same file, same loader: duplicate requests are merged by the loader.

    return m_loaders.at(int(qHash(filePath) % uint(m_loaders.size())));
}

QImage PhotosThumbnailBroker::cached(const QString& filePath, int size)
{
    QMutexLocker locker(&m_mutex);
    const QImage* const image = m_cache.object(cacheKey(filePath, size));

    return (image ? *image : QImage());
}

void PhotosThumbnailBroker::request(const QString& filePath, qlonglong imageId, int size)
{
    // Visible work has priority over the background pre-generation.

    m_lastRequest.restart();
    pausePregeneration();

    const QImage image = cached(filePath, size);

    if (!image.isNull())
    {
        Q_EMIT signalThumbnailReady(filePath, size, image);

        return;
    }

    const QString key = cacheKey(filePath, size);

    if (m_pending.value(key, 0) > 0)
    {
        ++m_pending[key];       // Already on its way.

        return;
    }

    m_pending[key] = 1;

    if (scaleFromLarger(filePath, size))
    {
        return;
    }

    ThumbnailIdentifier identifier(filePath);
    identifier.id = imageId;

    // Asynchronous: the result arrives through the loader signals.
    // Requests are processed last-in first-out, so what is on screen now loads first.

    loaderFor(filePath)->find(identifier, size);
}

bool PhotosThumbnailBroker::scaleFromLarger(const QString& filePath, int size)
{
    QImage source;

    for (const int larger : std::as_const(m_sizes))
    {
        if (larger > size)
        {
            source = cached(filePath, larger);

            if (!source.isNull())
            {
                break;
            }
        }
    }

    if (source.isNull())
    {
        return false;
    }

    m_scalePool.start([this, source, filePath, size] ()
        {
            const QImage scaled = source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            QMetaObject::invokeMethod(this, [this, filePath, size, scaled] ()
                {
                    store(filePath, size, scaled);
                    deliver(filePath, size, scaled);
                },
                Qt::QueuedConnection
            );
        }
    );

    return true;
}

void PhotosThumbnailBroker::cancel(const QString& filePath, int size)
{
    auto it = m_pending.find(cacheKey(filePath, size));

    if (it == m_pending.end())
    {
        return;
    }

    if (--it.value() > 0)
    {
        return;
    }

    m_pending.erase(it);

    // Nobody waits for it any more (the tile scrolled away): drop the task
    // so that the loader works on what is visible, unless another size of the
    // same file is still wanted (the loader stops tasks by file).

    for (const int other : std::as_const(m_sizes))
    {
        if (m_pending.contains(cacheKey(filePath, other)))
        {
            return;
        }
    }

    loaderFor(filePath)->stopLoading(filePath);
}

void PhotosThumbnailBroker::store(const QString& filePath, int size, const QImage& image)
{
    if (image.isNull())
    {
        return;
    }

    // Store in a format Qt Quick's texture factory takes as is: it then shares
    // the pixel data with this cache instead of keeping a converted copy.

    QImage stored = image;

    if ((stored.format() != QImage::Format_RGB32) &&
        (stored.format() != QImage::Format_ARGB32_Premultiplied))
    {
        stored = stored.convertToFormat(stored.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied
                                                                 : QImage::Format_RGB32);
    }

    QMutexLocker locker(&m_mutex);
    m_cache.insert(cacheKey(filePath, size), new QImage(stored),
                   qMax<qsizetype>(1, stored.sizeInBytes() / 1024));
}

void PhotosThumbnailBroker::deliver(const QString& filePath, int size, const QImage& image)
{
    m_pending.remove(cacheKey(filePath, size));

    Q_EMIT signalThumbnailReady(filePath, size, image);
}

void PhotosThumbnailBroker::slotImageLoaded(const LoadingDescription& description, const QImage& image)
{
    const int size = description.previewParameters.size;

    store(description.filePath, size, image);
    deliver(description.filePath, size, image);
}

void PhotosThumbnailBroker::slotFileChanged(const QString& filePath)
{
    bool known = false;

    {
        QMutexLocker locker(&m_mutex);

        for (const int size : std::as_const(m_sizes))
        {
            known |= m_cache.remove(cacheKey(filePath, size));
        }
    }

    if (known)
    {
        Q_EMIT signalThumbnailChanged(filePath);
    }
}

// --- Background pre-generation -----------------------------------------------------

void PhotosThumbnailBroker::pregenerate(const QList<ThumbnailIdentifier>& identifiers)
{
    for (ThumbnailLoadThread* const loader : std::as_const(m_pregenerators))
    {
        loader->stopAllTasks();
    }

    m_pregenInFlight.clear();
    m_pregenQueue  = identifiers;
    m_pregenCursor = 0;
    m_pregenTotal  = identifiers.size();

    Q_EMIT signalPregenerationProgress(0, m_pregenTotal);

    m_pregenTimer->start();
}

void PhotosThumbnailBroker::pausePregeneration()
{
    for (ThumbnailLoadThread* const loader : std::as_const(m_pregenerators))
    {
        if (!loader->isRunning())
        {
            continue;
        }

        // Give the pool threads back to the visible work; the interrupted batch
        // is done again later (already generated thumbnails are skipped quickly).

        loader->stopAllTasks();
        m_pregenQueue << m_pregenInFlight.take(loader);
    }
}

void PhotosThumbnailBroker::slotPregenerationTick()
{
    // Only while nothing visible is waiting, and the user is not scrolling.

    if (!m_pending.isEmpty() || (m_lastRequest.isValid() && (m_lastRequest.elapsed() < 500)))
    {
        return;
    }

    bool busy = false;

    for (ThumbnailLoadThread* const loader : std::as_const(m_pregenerators))
    {
        if (loader->isRunning())
        {
            busy = true;
            continue;
        }

        m_pregenInFlight.remove(loader);

        if (m_pregenCursor >= m_pregenQueue.size())
        {
            continue;
        }

        // Small batches: pre-generation can always be paused within a few
        // thumbnails when the user scrolls to something not generated yet.

        const QList<ThumbnailIdentifier> batch = m_pregenQueue.mid(m_pregenCursor, 8);
        m_pregenCursor                        += batch.size();
        m_pregenInFlight[loader]               = batch;
        busy                                   = true;

        loader->pregenerateGroup(batch, m_sizes.last());
    }

    const int done = qMin(m_pregenCursor, m_pregenTotal);

    if (!busy && (m_pregenCursor >= m_pregenQueue.size()))
    {
        m_pregenTimer->stop();
        m_pregenQueue.clear();
        m_pregenCursor = 0;

        Q_EMIT signalPregenerationProgress(m_pregenTotal, m_pregenTotal);

        return;
    }

    Q_EMIT signalPregenerationProgress(done, m_pregenTotal);
}

// -------------------------------------------------------------------------------

namespace
{

class PhotosThumbnailResponse : public QQuickImageResponse
{
    Q_OBJECT

public:

    PhotosThumbnailResponse(PhotosThumbnailBroker* const broker,
                            const QString& filePath,
                            qlonglong imageId,
                            int size)
        : m_broker  (broker),
          m_filePath(filePath),
          m_size    (size)
    {
        // Connect before looking at the cache, so a result cannot be missed.

        m_connection = connect(broker, &PhotosThumbnailBroker::signalThumbnailReady,
                               this, &PhotosThumbnailResponse::slotReady);

        const QImage image = broker->cached(filePath, size);

        if (!image.isNull())
        {
            QMetaObject::invokeMethod(this, [this, image] ()
                {
                    finish(image);
                },
                Qt::QueuedConnection
            );

            return;
        }

        QPointer<PhotosThumbnailBroker> guard(broker);

        QMetaObject::invokeMethod(broker, [guard, filePath, imageId, size] ()
            {
                if (guard)
                {
                    guard->request(filePath, imageId, size);
                }
            },
            Qt::QueuedConnection
        );
    }

    QQuickTextureFactory* textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return (m_image.isNull() && !m_cancelled) ? QLatin1String("No thumbnail") : QString();
    }

    void cancel() override
    {
        // The tile went away (scrolled out, recycled): let the broker drop
        // the loading task if nobody else waits for this thumbnail.

        if (!m_done)
        {
            QPointer<PhotosThumbnailBroker> guard(m_broker);
            const QString filePath = m_filePath;
            const int size         = m_size;

            QMetaObject::invokeMethod(m_broker, [guard, filePath, size] ()
                {
                    if (guard)
                    {
                        guard->cancel(filePath, size);
                    }
                },
                Qt::QueuedConnection
            );
        }

        m_cancelled = true;
        finish(QImage());
    }

private:

    void slotReady(const QString& filePath, int size, const QImage& image)
    {
        if ((size == m_size) && (filePath == m_filePath))
        {
            finish(image);
        }
    }

    void finish(const QImage& image)
    {
        if (m_done)
        {
            return;
        }

        m_done  = true;
        m_image = image;
        QObject::disconnect(m_connection);

        Q_EMIT finished();
    }

private:

    QPointer<PhotosThumbnailBroker> m_broker;
    QString                 m_filePath;
    int                     m_size      = 0;
    QImage                  m_image;
    QMetaObject::Connection m_connection;
    bool    m_done      = false;
    bool    m_cancelled = false;
};

// -------------------------------------------------------------------------------

class PhotosPreviewResponse : public QQuickImageResponse
{
    Q_OBJECT

public:

    PhotosPreviewResponse(QThreadPool* const pool, const QString& filePath, int size)
    {
        // The response stays alive until finished() is emitted, which only
        // happens once the job is done: the job can safely post back to us.

        pool->start([this, filePath, size] ()
            {
                const DImg dimg   = (size > 0) ? PreviewLoadThread::loadFastButLargeSynchronously(filePath, size)
                                               : PreviewLoadThread::loadHighQualitySynchronously(filePath);
                const QImage image = dimg.isNull() ? QImage() : dimg.copyQImage();

                QMetaObject::invokeMethod(this, [this, image] ()
                    {
                        m_image = image;

                        Q_EMIT finished();
                    },
                    Qt::QueuedConnection
                );
            }
        );
    }

    QQuickTextureFactory* textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_image.isNull() ? QLatin1String("No preview") : QString();
    }

private:

    QImage m_image;
};

} // namespace

// -------------------------------------------------------------------------------

PhotosThumbnailProvider::PhotosThumbnailProvider(PhotosThumbnailBroker* const broker)
    : m_broker(broker)
{
}

QQuickImageResponse* PhotosThumbnailProvider::requestImageResponse(const QString& id, const QSize&)
{
    // "<imageId>/<version>/<size>/<base64url path>": the version only makes the
    // URL unique after a file change, it is not needed to load the thumbnail.

    const QStringList parts  = id.split(QLatin1Char('/'));
    const qlonglong imageId  = parts.value(0).toLongLong();
    const int size           = parts.value(2).toInt();
    const QString filePath   = photosDecodePath(parts.value(3));

    return new PhotosThumbnailResponse(m_broker, filePath, imageId, size);
}

// -------------------------------------------------------------------------------

PhotosPreviewProvider::PhotosPreviewProvider()
{
    // Previews are big decodes: a couple of threads keep the next/previous
    // images loading while not starving the thumbnail loaders.

    m_pool.setMaxThreadCount(2);
}

PhotosPreviewProvider::~PhotosPreviewProvider()
{
    m_pool.waitForDone();
}

QQuickImageResponse* PhotosPreviewProvider::requestImageResponse(const QString& id, const QSize&)
{
    const int separator    = id.indexOf(QLatin1Char('/'));
    const int size         = id.left(separator).toInt();
    const QString filePath = photosDecodePath(id.mid(separator + 1));

    return new PhotosPreviewResponse(&m_pool, filePath, size);
}

} // namespace Digikam

#include "photosimageproviders.moc"
