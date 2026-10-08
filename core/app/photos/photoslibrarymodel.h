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
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>

// Local includes

#include "dtrashiteminfo.h"

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
    QString   livePath;     ///< Video of a Live Photo (same name, same folder).
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

    /// Devices the photos come from (QVariantMap: kind "tag" or "camera", name, count,
    /// tagId or make/model). Named devices are tags given at import (see PhotosImporter).
    Q_PROPERTY(QVariantList devices      READ devices      NOTIFY devicesChanged)

    /// Files filter: identifies what is shown (e.g. an import id).
    Q_PROPERTY(QString      filesKey     READ filesKey     NOTIFY filterChanged)

    /// Incremented whenever per-photo flags change: lets QML bindings refresh.
    Q_PROPERTY(int          revision     READ revision     NOTIFY revisionChanged)

    /// Selection, kept by image id so that it survives reloads.
    Q_PROPERTY(int          selectionCount    READ selectionCount    NOTIFY selectionChanged)
    Q_PROPERTY(int          selectionRevision READ selectionRevision NOTIFY selectionChanged)
    Q_PROPERTY(bool         canUndoTrash      READ canUndoTrash      NOTIFY canUndoTrashChanged)

public:

    enum Filter
    {
        Library     = 0,
        Favorites   = 1,
        Album       = 2,
        Videos      = 3,
        Files       = 4,    ///< A list of files, e.g. the photos of an import.
        DeviceTag   = 5,    ///< Photos with a "Devices/<name>" tag.
        Camera      = 6,    ///< Photos taken with a camera make and model (EXIF).
        Screenshots = 7,
        Raw         = 8,
        Panoramas   = 9,
        Selfies     = 10,
        Hidden      = 11,
        Trash       = 12    ///< "Recently Deleted": digiKam's collection trash.
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

    /// Tag hiding photos from all the views but "Hidden".
    static QString hiddenTagName();

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
    QVariantList devices()    const;
    QString      filesKey()   const;
    int          revision()   const;
    int          selectionCount()    const;
    int          selectionRevision() const;
    bool         canUndoTrash()      const;

    const QList<PhotosEntry>& entries() const;

    // --- QML API ---

    Q_INVOKABLE void      reload();
    Q_INVOKABLE void      showAlbum(int tagId);
    Q_INVOKABLE void      showDeviceTag(int tagId);
    Q_INVOKABLE void      showCamera(const QString& make, const QString& model);
    Q_INVOKABLE void      showFiles(const QStringList& filePaths, const QString& title, const QString& key);
    Q_INVOKABLE qlonglong idAt(int row)                         const;
    Q_INVOKABLE int       rowOfId(qlonglong id)                 const;
    Q_INVOKABLE QString   filePathAt(int row)                   const;
    Q_INVOKABLE QString   fileNameAt(int row)                   const;
    Q_INVOKABLE QUrl      fileUrlAt(int row)                    const;

    /// The video of a Live Photo, or an empty url.
    Q_INVOKABLE QUrl      liveUrlAt(int row)                    const;
    Q_INVOKABLE QString   dateTextAt(int row)                   const;
    Q_INVOKABLE bool      isVideoAt(int row)                    const;
    Q_INVOKABLE bool      isFavoriteAt(int row)                 const;
    Q_INVOKABLE QString   thumbSourceAt(int row, int size)      const;
    Q_INVOKABLE QString   previewSourceAt(int row, int size)    const;
    Q_INVOKABLE void      toggleFavoriteAt(int row);
    Q_INVOKABLE bool      addToAlbum(int row, const QString& albumName);
    Q_INVOKABLE void      removeFromCurrentAlbum(int row);

    /// Details for the info panel of the viewer (see the implementation for the keys).
    Q_INVOKABLE QVariantMap infoAt(int row)                     const;

    /// Sets the caption (description), written like the other information.
    Q_INVOKABLE void      setCaptionAt(int row, const QString& caption);

    /// Size used for the ThumbSourceRole of the model (QML passes sizes explicitly).
    void                  setDefaultThumbnailSize(int size);

    /// Called when a thumbnail file changed on disk: makes tiles request it again.
    void                  invalidateThumbnail(const QString& filePath);

    // --- Selection ---

    Q_INVOKABLE bool       isSelectedAt(int row)                const;
    Q_INVOKABLE void       toggleSelectedAt(int row);
    Q_INVOKABLE void       selectRangeTo(int row);
    Q_INVOKABLE void       selectOnly(int row);
    Q_INVOKABLE void       selectAll();
    Q_INVOKABLE void       clearSelection();
    Q_INVOKABLE void       beginBandSelection(bool additive);
    Q_INVOKABLE void       updateBandSelection(const QList<int>& rows);
    Q_INVOKABLE QList<int> selectedRows()                       const;

    // --- Actions on the selection ---

    Q_INVOKABLE bool       selectionAllFavorite()               const;
    Q_INVOKABLE void       setFavoriteForSelection(bool favorite);
    Q_INVOKABLE bool       addSelectionToAlbum(const QString& albumName);
    Q_INVOKABLE void       removeSelectionFromCurrentAlbum();

    // --- Move to trash (digiKam's collection trash, restorable) ---

    Q_INVOKABLE void       trashSelection();
    Q_INVOKABLE void       trashAt(int row);
    Q_INVOKABLE void       undoTrash();

    /// Moves these photos to the trash, shown or not (undoable as above).
    void                   trashImageIds(const QList<qlonglong>& ids);

    // --- Hidden photos ---

    Q_INVOKABLE bool       isHiddenAt(int row)                  const;
    Q_INVOKABLE void       setHiddenAt(int row, bool hidden);
    Q_INVOKABLE void       setHiddenForSelection(bool hidden);

    // --- Recently Deleted (Trash filter) ---

    Q_INVOKABLE QString    deletedTextAt(int row)               const;
    Q_INVOKABLE void       restoreAt(int row);
    Q_INVOKABLE void       restoreSelection();
    Q_INVOKABLE void       deleteForeverAt(int row);
    Q_INVOKABLE void       deleteSelectionForever();
    Q_INVOKABLE void       emptyTrash();

Q_SIGNALS:

    void countChanged();
    void loadingChanged();
    void filterChanged();
    void albumsChanged();
    void devicesChanged();
    void revisionChanged();
    void selectionChanged();
    void canUndoTrashChanged();

    /// Emitted when photos were moved to the trash by this model.
    void trashed(int count);

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
    void slotReloadDevices();

private:

public:

    struct Query
    {
        int           filter      = Library;
        int           tagId       = -1;
        int           hiddenTagId = -1;
        QString       make;
        QString       model;
        QSet<QString> files;
    };

    struct QueryResult
    {
        QList<PhotosEntry>                   entries;
        QHash<qlonglong, DTrashItemInfoList> trash;     ///< sidecars of the trashed file, then the file
    };

private:

    static QueryResult queryEntries(const Query& query);
    static QueryResult queryTrash();
    void setView(int filter, int tagId);
    void setHiddenIds(const QList<qlonglong>& ids, bool hidden);
    void trashActionIds(const QList<qlonglong>& ids, bool restore);
    void refreshRatings(const QList<qlonglong>& ids);
    void scheduleReload();
    void emitSelectionChanged();
    int  albumTagForName(const QString& albumName);
    void trashIds(const QList<qlonglong>& ids);

private:

    QList<PhotosEntry>                   m_entries;
    QHash<qlonglong, int>                m_rowOfId;
    QHash<QString, int>                  m_rowOfPath;
    QFutureWatcher<QueryResult>          m_watcher;
    QFutureWatcher<QVariantList>         m_devicesWatcher;
    QHash<qlonglong, DTrashItemInfoList> m_trashInfos;
    QVariantList                         m_devices;
    QString                              m_cameraMake;
    QString                              m_cameraModel;
    QSet<QString>                        m_files;
    QString                              m_filesTitle;
    QString                              m_filesKey;
    QTimer*                              m_reloadTimer = nullptr;
    QTimer*                              m_albumsTimer = nullptr;
    QVariantList                         m_albums;
    int                                  m_revision    = 0;
    int                                  m_filter      = Library;
    int                                  m_albumTagId  = -1;
    bool                                 m_loading     = false;
    bool                                 m_pending     = false;

    QHash<QString, int>                  m_thumbVersion;
    int                                  m_defaultThumbSize  = 512;

    QSet<qlonglong>                      m_selection;
    QSet<qlonglong>                      m_bandBase;
    qlonglong                            m_anchorId          = -1;
    int                                  m_selectionRevision = 0;

    QSet<qlonglong>                      m_trashPending;
    QSet<qlonglong>                      m_undoIds;
    QSet<QString>                        m_undoRoots;
    int                                  m_undoAttempts      = 0;
};

/// URL helpers shared by the models and the image providers.
QString photosEncodePath(const QString& filePath);
QString photosDecodePath(const QString& encoded);

/// Readable device name from EXIF make and model:
/// "Apple" + "iPhone 15 Pro" -> "iPhone 15 Pro", "samsung" + "SM-S918B" -> "Samsung SM-S918B".
QString photosPrettyDevice(const QString& make, const QString& model);

} // namespace Digikam
