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

#pragma once

// Qt includes

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QString>
#include <QThreadPool>

namespace Digikam
{

class LoadingDescription;
class ThumbnailLoadThread;

/**
 * Lives in the GUI thread. Owns a dedicated digiKam thumbnail loader
 * (reading thumbnails-digikam.db, generating missing thumbnails exactly like
 * the classic views do) and a large in-memory cache of decoded thumbnails.
 *
 * Thumbnails are always requested at the stored size: the grid zooms by
 * scaling on the GPU, so zooming never triggers reloading.
 */
class PhotosThumbnailBroker : public QObject
{
    Q_OBJECT

public:

    explicit PhotosThumbnailBroker(QObject* const parent = nullptr);
    ~PhotosThumbnailBroker() override;

    /// Thread-safe cache lookup.
    QImage cached(const QString& filePath);

    /// Must be called in the GUI thread (use a queued invocation).
    void request(const QString& filePath, qlonglong imageId);

Q_SIGNALS:

    void signalThumbnailReady(const QString& filePath, const QImage& image);

private Q_SLOTS:

    void slotImageLoaded(const LoadingDescription& description, const QImage& image);
    void slotPixmapLoaded(const LoadingDescription& description, const QPixmap& pixmap);

private:

    void store(const QString& filePath, const QImage& image);

private:

    ThumbnailLoadThread*    m_thread = nullptr;
    QMutex                  m_mutex;
    QCache<QString, QImage> m_cache;
    int                     m_size   = 256;
};

// -------------------------------------------------------------------------------

class PhotosThumbnailProvider : public QQuickAsyncImageProvider
{
public:

    explicit PhotosThumbnailProvider(PhotosThumbnailBroker* const broker);

    /// id format: "<imageId>/<base64url file path>"
    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

private:

    PhotosThumbnailBroker* m_broker = nullptr;
};

// -------------------------------------------------------------------------------

class PhotosPreviewProvider : public QQuickAsyncImageProvider
{
public:

    PhotosPreviewProvider();
    ~PhotosPreviewProvider() override;

    /**
     * id format: "<size>/<base64url file path>".
     * size > 0 loads a fast preview at least that large (embedded previews
     * when big enough), size == 0 loads the full resolution image.
     */
    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

private:

    QThreadPool m_pool;
};

} // namespace Digikam
