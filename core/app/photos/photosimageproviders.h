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
#include <QHash>
#include <QList>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QString>
#include <QThreadPool>

namespace Digikam
{

class LoadingDescription;
class ThumbnailIdentifier;
class ThumbnailLoadThread;

/**
 * Lives in the GUI thread. Owns dedicated digiKam thumbnail loaders
 * (reading thumbnails-digikam.db, generating missing thumbnails exactly like
 * the classic views do) and a large in-memory cache of decoded thumbnails.
 *
 * Thumbnails are always requested at the stored size: the grid zooms by
 * scaling on the GPU, so zooming never triggers reloading.
 *
 * Speed for libraries seen for the first time:
 *  - several loaders work in parallel (a file always goes to the same one);
 *  - requests for tiles which scrolled away are cancelled, so the queue holds
 *    what is on screen (newest request first);
 *  - a low priority loader pre-generates missing thumbnails of the whole
 *    library in the background.
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

    /// Must be called in the GUI thread: the requester does not need it any more.
    void cancel(const QString& filePath);

    /// Background generation of missing thumbnails (GUI thread).
    void pregenerate(const QList<ThumbnailIdentifier>& identifiers);

    int loaderCount() const;

Q_SIGNALS:

    void signalThumbnailReady(const QString& filePath, const QImage& image);

    /// The file changed on disk: its thumbnail must be requested again.
    void signalThumbnailChanged(const QString& filePath);

private Q_SLOTS:

    void slotImageLoaded(const LoadingDescription& description, const QImage& image);
    void slotFileChanged(const QString& filePath);

private:

    void                 store(const QString& filePath, const QImage& image);
    ThumbnailLoadThread* loaderFor(const QString& filePath) const;

private:

    QList<ThumbnailLoadThread*> m_loaders;
    ThumbnailLoadThread*        m_pregenerator = nullptr;
    QHash<QString, int>         m_pending;              ///< GUI thread only.
    QMutex                      m_mutex;
    QCache<QString, QImage>     m_cache;
    int                         m_size         = 256;
};

// -------------------------------------------------------------------------------

class PhotosThumbnailProvider : public QQuickAsyncImageProvider
{
public:

    explicit PhotosThumbnailProvider(PhotosThumbnailBroker* const broker);

    /// id format: "<imageId>/<version>/<base64url file path>"
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
