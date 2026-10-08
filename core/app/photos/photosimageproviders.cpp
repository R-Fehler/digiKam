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

#include <QMetaObject>
#include <QMutexLocker>
#include <QPixmap>
#include <QPointer>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QThread>

// Local includes

#include "digikam_debug.h"
#include "dimg.h"
#include "loadingcache.h"
#include "loadingdescription.h"
#include "photoslibrarymodel.h"
#include "previewloadthread.h"
#include "thumbnailloadthread.h"

namespace Digikam
{

PhotosThumbnailBroker::PhotosThumbnailBroker(QObject* const parent)
    : QObject(parent),
      m_size (ThumbnailLoadThread::maximumThumbnailSize())
{
    // Generating thumbnails for a library seen for the first time is CPU bound
    // (decoding full size camera files): one loader per core, measured fastest.
    // They run at a lowered priority so that the user interface stays fluid.

    int loaders = qBound(2, QThread::idealThreadCount(), 6);
    bool ok     = false;
    const int env = qEnvironmentVariableIntValue("DIGIKAM_PHOTOS_THUMB_THREADS", &ok);

    if (ok && (env > 0))
    {
        loaders = qMin(env, 16);
    }

    for (int i = 0 ; i < loaders ; ++i)
    {
        ThumbnailLoadThread* const loader = new ThumbnailLoadThread(this);

        // We only want QImages: no QPixmap conversion or border painting in the GUI thread.

        loader->setPixmapRequested(false);
        loader->setSendSurrogatePixmap(false);
        loader->setThumbnailSize(m_size);
        loader->setPriority(QThread::LowPriority);

        connect(loader, &ThumbnailLoadThread::signalQImageThumbnailLoaded,
                this, &PhotosThumbnailBroker::slotImageLoaded);

        // Not connected to signalThumbnailLoaded(QPixmap): pixmaps coming from the
        // shared cache of the classic views carry a painted 1 px border. A cache hit
        // there also schedules a load, which delivers the clean QImage above.

        m_loaders << loader;
    }

    m_pregenerator = new ThumbnailLoadThread(this);
    m_pregenerator->setPixmapRequested(false);
    m_pregenerator->setSendSurrogatePixmap(false);
    m_pregenerator->setThumbnailSize(m_size);
    m_pregenerator->setPriority(QThread::LowestPriority);

    // Cost unit is KiB: keep up to 512 MiB of decoded thumbnails,
    // i.e. about 2000 thumbnails of 256x256 or 500 of 512x512.

    m_cache.setMaxCost(512 * 1024);

    // Files modified on disk (edited, rotated...): digiKam's loading cache
    // is notified, and so are we. May be emitted under the cache lock.

    connect(LoadingCache::cache(), &LoadingCache::fileChanged,
            this, &PhotosThumbnailBroker::slotFileChanged,
            Qt::QueuedConnection);

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode: thumbnail loaders:" << loaders << "size:" << m_size;
}

PhotosThumbnailBroker::~PhotosThumbnailBroker()
{
    m_pregenerator->stopAllTasks();

    for (ThumbnailLoadThread* const loader : std::as_const(m_loaders))
    {
        loader->stopAllTasks();
    }

    m_pregenerator->wait();

    for (ThumbnailLoadThread* const loader : std::as_const(m_loaders))
    {
        loader->wait();
    }
}

int PhotosThumbnailBroker::loaderCount() const
{
    return m_loaders.size();
}

ThumbnailLoadThread* PhotosThumbnailBroker::loaderFor(const QString& filePath) const
{
    // Same file, same loader: duplicate requests are merged by the loader.

    return m_loaders.at(int(qHash(filePath) % uint(m_loaders.size())));
}

QImage PhotosThumbnailBroker::cached(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    const QImage* const image = m_cache.object(filePath);

    return (image ? *image : QImage());
}

void PhotosThumbnailBroker::request(const QString& filePath, qlonglong imageId)
{
    const QImage image = cached(filePath);

    if (!image.isNull())
    {
        Q_EMIT signalThumbnailReady(filePath, image);

        return;
    }

    ThumbnailIdentifier identifier(filePath);
    identifier.id = imageId;

    ++m_pending[filePath];

    // Asynchronous: the result arrives through the loader signals.
    // Requests are processed last-in first-out, so what is on screen now loads first.

    loaderFor(filePath)->find(identifier, m_size);
}

void PhotosThumbnailBroker::cancel(const QString& filePath)
{
    auto it = m_pending.find(filePath);

    if (it == m_pending.end())
    {
        return;
    }

    if (--it.value() <= 0)
    {
        // Nobody waits for it any more (the tile scrolled away): drop the task
        // so that the loader works on what is visible. The background
        // pre-generation will still create it later.

        m_pending.erase(it);
        loaderFor(filePath)->stopLoading(filePath);
    }
}

void PhotosThumbnailBroker::pregenerate(const QList<ThumbnailIdentifier>& identifiers)
{
    m_pregenerator->stopAllTasks();
    m_pregenerator->pregenerateGroup(identifiers, m_size);
}

void PhotosThumbnailBroker::store(const QString& filePath, const QImage& image)
{
    if (image.isNull())
    {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_cache.insert(filePath, new QImage(image), qMax<qsizetype>(1, image.sizeInBytes() / 1024));
}

void PhotosThumbnailBroker::slotImageLoaded(const LoadingDescription& description, const QImage& image)
{
    m_pending.remove(description.filePath);
    store(description.filePath, image);

    Q_EMIT signalThumbnailReady(description.filePath, image);
}

void PhotosThumbnailBroker::slotFileChanged(const QString& filePath)
{
    {
        QMutexLocker locker(&m_mutex);

        if (!m_cache.remove(filePath))
        {
            return;     // Never shown by us: nothing to refresh.
        }
    }

    Q_EMIT signalThumbnailChanged(filePath);
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
                            qlonglong imageId)
        : m_broker  (broker),
          m_filePath(filePath)
    {
        // Connect before looking at the cache, so a result cannot be missed.

        m_connection = connect(broker, &PhotosThumbnailBroker::signalThumbnailReady,
                               this, &PhotosThumbnailResponse::slotReady);

        const QImage image = broker->cached(filePath);

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

        QMetaObject::invokeMethod(broker, [guard, filePath, imageId] ()
            {
                if (guard)
                {
                    guard->request(filePath, imageId);
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

            QMetaObject::invokeMethod(m_broker, [guard, filePath] ()
                {
                    if (guard)
                    {
                        guard->cancel(filePath);
                    }
                },
                Qt::QueuedConnection
            );
        }

        m_cancelled = true;
        finish(QImage());
    }

private:

    void slotReady(const QString& filePath, const QImage& image)
    {
        if (filePath == m_filePath)
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
    // "<imageId>/<version>/<base64url path>": the version only makes the URL
    // unique after a file change, it is not needed to load the thumbnail.

    const QStringList parts  = id.split(QLatin1Char('/'));
    const qlonglong imageId  = parts.value(0).toLongLong();
    const QString filePath   = photosDecodePath(parts.value(2));

    return new PhotosThumbnailResponse(m_broker, filePath, imageId);
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
