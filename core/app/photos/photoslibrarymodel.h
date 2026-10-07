/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - flat, date sorted list of all photos
 *               and videos of the library (read only queries).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QAbstractListModel>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QTimer;

namespace Digikam
{

class CollectionImageChangeset;
class ImageChangeset;
class ImageTagChangeset;
class TagChangeset;

struct PhotosEntry
{
    qlonglong id       = 0;
    QString   filePath;
    QDateTime dateTime;
    int       rating   = 0;
    bool      isVideo  = false;
};

class PhotosLibraryModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int          count        READ count        NOTIFY countChanged)
    Q_PROPERTY(bool         loading      READ isLoading    NOTIFY loadingChanged)
    Q_PROPERTY(int          filter       READ filter       WRITE setFilter      NOTIFY filterChanged)
    Q_PROPERTY(int          albumTagId   READ albumTagId   WRITE setAlbumTagId  NOTIFY filterChanged)
    Q_PROPERTY(QString      title        READ title        NOTIFY filterChanged)
    Q_PROPERTY(QVariantList albums       READ albums       NOTIFY albumsChanged)

    /// Incremented whenever per-photo flags change: lets QML bindings refresh.
    Q_PROPERTY(int          revision     READ revision     NOTIFY revisionChanged)

public:

    enum Filter
    {
        Library   = 0,
        Favorites = 1,
        Album     = 2,
        Videos    = 3
    };
    Q_ENUM(Filter)

    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        FilePathRole,
        FileNameRole,
        ThumbSourceRole,
        FavoriteRole,
        VideoRole
    };

    /// Rating used to mark a photo as favorite, and minimum rating counted as favorite.
    static const int FavoriteRating    = 5;
    static const int FavoriteMinRating = 4;

    /// Name of the top level tag holding Photos mode albums.
    static QString albumsRootTagName();

public:

    explicit PhotosLibraryModel(QObject* const parent = nullptr);
    ~PhotosLibraryModel() override;

    int                    rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant               data(const QModelIndex& index, int role = Qt::DisplayRole)   const override;
    QHash<int, QByteArray> roleNames()                                                   const override;

    int          count()      const;
    bool         isLoading()  const;
    int          filter()     const;
    void         setFilter(int filter);
    int          albumTagId() const;
    void         setAlbumTagId(int tagId);
    QString      title()      const;
    QVariantList albums()     const;
    int          revision()   const;

    const QList<PhotosEntry>& entries() const;

    // --- QML API ---

    Q_INVOKABLE void      reload();
    Q_INVOKABLE void      showAlbum(int tagId);
    Q_INVOKABLE qlonglong idAt(int row)                         const;
    Q_INVOKABLE int       rowOfId(qlonglong id)                 const;
    Q_INVOKABLE QString   filePathAt(int row)                   const;
    Q_INVOKABLE QString   fileNameAt(int row)                   const;
    Q_INVOKABLE QString   dateTextAt(int row)                   const;
    Q_INVOKABLE bool      isVideoAt(int row)                    const;
    Q_INVOKABLE bool      isFavoriteAt(int row)                 const;
    Q_INVOKABLE QString   thumbSourceAt(int row)                const;
    Q_INVOKABLE QString   previewSourceAt(int row, int size)    const;
    Q_INVOKABLE void      toggleFavoriteAt(int row);
    Q_INVOKABLE bool      addToAlbum(int row, const QString& albumName);
    Q_INVOKABLE void      removeFromCurrentAlbum(int row);

Q_SIGNALS:

    void countChanged();
    void loadingChanged();
    void filterChanged();
    void albumsChanged();
    void revisionChanged();

    /// Emitted before the content is replaced by a reload, then after it.
    void aboutToReload();
    void reloaded();

private Q_SLOTS:

    void slotLoaded();
    void slotCollectionImageChange(const CollectionImageChangeset& changeset);
    void slotImageChange(const ImageChangeset& changeset);
    void slotImageTagChange(const ImageTagChangeset& changeset);
    void slotTagChange(const TagChangeset& changeset);
    void slotReloadAlbums();

private:

    static QList<PhotosEntry> queryEntries(int filter, int albumTagId);
    void refreshRatings(const QList<qlonglong>& ids);
    void scheduleReload();

private:

    QList<PhotosEntry>                   m_entries;
    QHash<qlonglong, int>                m_rowOfId;
    QFutureWatcher<QList<PhotosEntry> >  m_watcher;
    QTimer*                              m_reloadTimer = nullptr;
    QTimer*                              m_albumsTimer = nullptr;
    QVariantList                         m_albums;
    int                                  m_revision    = 0;
    int                                  m_filter      = Library;
    int                                  m_albumTagId  = -1;
    bool                                 m_loading     = false;
    bool                                 m_pending     = false;
};

/// URL helpers shared by the models and the image providers.
QString photosEncodePath(const QString& filePath);
QString photosDecodePath(const QString& encoded);

} // namespace Digikam
