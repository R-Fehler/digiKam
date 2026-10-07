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

// Local includes

#include "digikam_debug.h"
#include "dimg.h"
#include "loadingdescription.h"
#include "photoslibrarymodel.h"
#include "previewloadthread.h"
#include "thumbnailloadthread.h"

namespace Digikam
{

PhotosThumbnailBroker::PhotosThumbnailBroker(QObject* const parent)
    : QObject (parent),
      m_thread(new ThumbnailLoadThread(this)),
      m_size  (ThumbnailLoadThread::maximumThumbnailSize())
{
    // We only want QImages: no QPixmap conversion or border painting in the GUI thread.

    m_thread->setPixmapRequested(false);
    m_thread->setSendSurrogatePixmap(false);
    m_thread->setThumbnailSize(m_size);

    // Cost unit is KiB: keep up to 512 MiB of decoded thumbnails,
    // i.e. about 2000 thumbnails of 256x256 or 500 of 512x512.

    m_cache.setMaxCost(512 * 1024);

    connect(m_thread, &ThumbnailLoadThread::signalQImageThumbnailLoaded,
            this, &PhotosThumbnailBroker::slotImageLoaded);

    connect(m_thread, &ThumbnailLoadThread::signalThumbnailLoaded,
            this, &PhotosThumbnailBroker::slotPixmapLoaded);
}

PhotosThumbnailBroker::~PhotosThumbnailBroker()
{
    m_thread->stopAllTasks();
    m_thread->wait();
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

    // Asynchronous: the result arrives through the loader signals.
    // Requests are processed last-in first-out, so what is on screen now loads first.

    m_thread->find(identifier, m_size);
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
    store(description.filePath, image);

    Q_EMIT signalThumbnailReady(description.filePath, image);
}

void PhotosThumbnailBroker::slotPixmapLoaded(const LoadingDescription& description, const QPixmap& pixmap)
{
    // Only emitted when another digiKam view already had this thumbnail in its pixmap cache.

    const QImage image = pixmap.toImage();
    store(description.filePath, image);

    Q_EMIT signalThumbnailReady(description.filePath, image);
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
        : m_filePath(filePath)
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
        // The loading task continues in digiKam's loader and fills the cache,
        // which is still useful when scrolling back.

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
    const int separator      = id.indexOf(QLatin1Char('/'));
    const qlonglong imageId  = id.left(separator).toLongLong();
    const QString filePath   = photosDecodePath(id.mid(separator + 1));

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
