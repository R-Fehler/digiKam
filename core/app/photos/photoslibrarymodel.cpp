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

#include "photoslibrarymodel.h"

// Qt includes

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QTimer>
#include <QtConcurrentRun>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "collectionmanager.h"
#include "coredbaccess.h"
#include "coredbbackend.h"
#include "coredbchangesets.h"
#include "coredbconstants.h"
#include "coredbfields.h"
#include "coredbwatch.h"
#include "dio.h"
#include "dtrash.h"
#include "dtrashiteminfo.h"
#include "fileactionmngr.h"
#include "iteminfo.h"
#include "tagscache.h"

namespace Digikam
{

QString photosEncodePath(const QString& filePath)
{
    return QString::fromLatin1(filePath.toUtf8().toBase64(QByteArray::Base64UrlEncoding |
                                                          QByteArray::OmitTrailingEquals));
}

QString photosDecodePath(const QString& encoded)
{
    return QString::fromUtf8(QByteArray::fromBase64(encoded.toLatin1(),
                                                    QByteArray::Base64UrlEncoding |
                                                    QByteArray::OmitTrailingEquals));
}

QString PhotosLibraryModel::albumsRootTagName()
{
    return QLatin1String("Albums");
}

PhotosLibraryModel::PhotosLibraryModel(QObject* const parent)
    : QAbstractListModel(parent)
{
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(1500);

    m_albumsTimer = new QTimer(this);
    m_albumsTimer->setSingleShot(true);
    m_albumsTimer->setInterval(300);

    connect(m_reloadTimer, &QTimer::timeout,
            this, &PhotosLibraryModel::reload);

    connect(m_albumsTimer, &QTimer::timeout,
            this, &PhotosLibraryModel::slotReloadAlbums);

    connect(&m_watcher, &QFutureWatcher<QList<PhotosEntry> >::finished,
            this, &PhotosLibraryModel::slotLoaded);

    connect(this, &QAbstractItemModel::dataChanged,
            this, [this] ()
        {
            ++m_revision;

            Q_EMIT revisionChanged();
        }
    );

    CoreDbWatch* const watch = CoreDbAccess::databaseWatch();

    connect(watch, &CoreDbWatch::collectionImageChange,
            this, &PhotosLibraryModel::slotCollectionImageChange);

    connect(watch, &CoreDbWatch::imageChange,
            this, &PhotosLibraryModel::slotImageChange);

    connect(watch, &CoreDbWatch::imageTagChange,
            this, &PhotosLibraryModel::slotImageTagChange);

    connect(watch, &CoreDbWatch::tagChange,
            this, &PhotosLibraryModel::slotTagChange);

    slotReloadAlbums();
}

PhotosLibraryModel::~PhotosLibraryModel()
{
    m_watcher.waitForFinished();
}

int PhotosLibraryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant PhotosLibraryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (index.row() >= m_entries.size()))
    {
        return QVariant();
    }

    const PhotosEntry& entry = m_entries.at(index.row());

    switch (role)
    {
        case Qt::DisplayRole:
        case FileNameRole:
            return QFileInfo(entry.filePath).fileName();

        case IdRole:
            return entry.id;

        case FilePathRole:
            return entry.filePath;

        case ThumbSourceRole:
            return thumbSourceAt(index.row(), m_defaultThumbSize);

        case FavoriteRole:
            return (entry.rating >= FavoriteMinRating);

        case VideoRole:
            return entry.isVideo;

        default:
            return QVariant();
    }
}

QHash<int, QByteArray> PhotosLibraryModel::roleNames() const
{
    return {
             { IdRole,          "imageId"     },
             { FilePathRole,    "filePath"    },
             { FileNameRole,    "fileName"    },
             { ThumbSourceRole, "thumbSource" },
             { FavoriteRole,    "favorite"    },
             { VideoRole,       "isVideo"     }
           };
}

int PhotosLibraryModel::count() const
{
    return m_entries.size();
}

bool PhotosLibraryModel::isLoading() const
{
    return m_loading;
}

int PhotosLibraryModel::filter() const
{
    return m_filter;
}

void PhotosLibraryModel::setFilter(int filter)
{
    if (filter == m_filter)
    {
        return;
    }

    m_filter = filter;

    Q_EMIT filterChanged();

    reload();
}

int PhotosLibraryModel::albumTagId() const
{
    return m_albumTagId;
}

void PhotosLibraryModel::setAlbumTagId(int tagId)
{
    if (tagId == m_albumTagId)
    {
        return;
    }

    m_albumTagId = tagId;

    Q_EMIT filterChanged();

    if (m_filter == Album)
    {
        reload();
    }
}

void PhotosLibraryModel::showAlbum(int tagId)
{
    m_albumTagId = tagId;
    m_filter     = Album;

    Q_EMIT filterChanged();

    reload();
}

QString PhotosLibraryModel::title() const
{
    switch (m_filter)
    {
        case Favorites:
            return i18n("Favorites");

        case Videos:
            return i18n("Videos");

        case Album:
        {
            for (const QVariant& album : std::as_const(m_albums))
            {
                const QVariantMap map = album.toMap();

                if (map.value(QLatin1String("tagId")).toInt() == m_albumTagId)
                {
                    return map.value(QLatin1String("name")).toString();
                }
            }

            return i18n("Album");
        }

        default:
            return i18n("Library");
    }
}

QVariantList PhotosLibraryModel::albums() const
{
    return m_albums;
}

int PhotosLibraryModel::revision() const
{
    return m_revision;
}

const QList<PhotosEntry>& PhotosLibraryModel::entries() const
{
    return m_entries;
}

void PhotosLibraryModel::reload()
{
    if (m_loading)
    {
        m_pending = true;

        return;
    }

    m_loading = true;
    m_pending = false;

    Q_EMIT loadingChanged();

    m_watcher.setFuture(QtConcurrent::run(&PhotosLibraryModel::queryEntries, m_filter, m_albumTagId));
}

QList<PhotosEntry> PhotosLibraryModel::queryEntries(int filter, int albumTagId)
{
    QString join;
    QString where;
    QList<QVariant> bound;

    switch (filter)
    {
        case Favorites:
        {
            where = QString::fromLatin1(" AND ImageInformation.rating >= %1").arg(FavoriteMinRating);
            break;
        }

        case Videos:
        {
            where = QString::fromLatin1(" AND Images.category = %1").arg(int(DatabaseItem::Video));
            break;
        }

        case Album:
        {
            join  = QLatin1String(" INNER JOIN ImageTags ON ImageTags.imageid = Images.id ");
            where = QLatin1String(" AND ImageTags.tagid = ?");
            bound << albumTagId;
            break;
        }

        default:
        {
            break;
        }
    }

    // Grouped items (e.g. RAW+JPEG pairs, bursts) are represented by their group leader.

    const QString sql = QString::fromLatin1(
        "SELECT Images.id, Images.name, Images.category, Albums.albumRoot, Albums.relativePath, "
        "       ImageInformation.creationDate, ImageInformation.rating "
        "FROM Images "
        "INNER JOIN Albums ON Albums.id = Images.album "
        "LEFT JOIN ImageInformation ON ImageInformation.imageid = Images.id "
        "%1"
        "WHERE Images.status = %2 "
        "  AND (Images.category = %3 OR Images.category = %4) "
        "  AND Images.id NOT IN (SELECT subject FROM ImageRelations WHERE type = %5) "
        "%6 "
        "ORDER BY ImageInformation.creationDate DESC, Images.id DESC;")
        .arg(join)
        .arg(int(DatabaseItem::Visible))
        .arg(int(DatabaseItem::Image))
        .arg(int(DatabaseItem::Video))
        .arg(int(DatabaseRelation::Grouped))
        .arg(where);

    QList<QVariant> values;

    {
        CoreDbAccess access;
        access.backend()->execSql(sql, bound, &values);
    }

    QHash<int, QString> rootPaths;
    QList<PhotosEntry>  entries;
    entries.reserve(values.size() / 7);

    for (int i = 0 ; (i + 6) < values.size() ; i += 7)
    {
        const int rootId = values.at(i + 3).toInt();

        auto it = rootPaths.constFind(rootId);

        if (it == rootPaths.constEnd())
        {
            it = rootPaths.insert(rootId, CollectionManager::instance()->albumRootPath(rootId));
        }

        if (it.value().isEmpty())
        {
            // Collection not available (e.g. removable media unplugged).

            continue;
        }

        const QString relativePath = values.at(i + 4).toString();

        PhotosEntry entry;
        entry.id       = values.at(i).toLongLong();
        entry.filePath = (relativePath == QLatin1String("/")) ? QString(it.value() + QLatin1Char('/') + values.at(i + 1).toString())
                                                               : QString(it.value() + relativePath + QLatin1Char('/') + values.at(i + 1).toString());
        entry.isVideo  = (values.at(i + 2).toInt() == DatabaseItem::Video);
        entry.dateTime = values.at(i + 5).toDateTime();
        entry.rating   = qMax(0, values.at(i + 6).toInt());

        entries << entry;
    }

    return entries;
}

void PhotosLibraryModel::slotLoaded()
{
    Q_EMIT aboutToReload();

    beginResetModel();

    m_entries = m_watcher.result();

    // Photos being moved to the trash disappear at once, even if the
    // database does not reflect it yet. Once it does, forget about them.

    if (!m_trashPending.isEmpty())
    {
        QSet<qlonglong> stillListed;

        m_entries.removeIf([this, &stillListed] (const PhotosEntry& entry)
            {
                if (m_trashPending.contains(entry.id))
                {
                    stillListed.insert(entry.id);

                    return true;
                }

                return false;
            }
        );

        m_trashPending = stillListed;
    }

    m_rowOfId.clear();
    m_rowOfId.reserve(m_entries.size());
    m_rowOfPath.clear();
    m_rowOfPath.reserve(m_entries.size());

    for (int i = 0 ; i < m_entries.size() ; ++i)
    {
        m_rowOfId.insert(m_entries.at(i).id, i);
        m_rowOfPath.insert(m_entries.at(i).filePath, i);
    }

    endResetModel();

    // Keep only the selected photos which are still shown.

    const int selected = m_selection.size();

    m_selection.removeIf([this] (qlonglong id)
        {
            return !m_rowOfId.contains(id);
        }
    );

    if (m_selection.size() != selected)
    {
        emitSelectionChanged();
    }

    m_loading = false;
    ++m_revision;

    Q_EMIT revisionChanged();

    Q_EMIT loadingChanged();
    Q_EMIT countChanged();
    Q_EMIT reloaded();

    if (m_pending)
    {
        reload();
    }
}

qlonglong PhotosLibraryModel::idAt(int row) const
{
    return ((row >= 0) && (row < m_entries.size())) ? m_entries.at(row).id : -1;
}

int PhotosLibraryModel::rowOfId(qlonglong id) const
{
    return m_rowOfId.value(id, -1);
}

QString PhotosLibraryModel::filePathAt(int row) const
{
    return ((row >= 0) && (row < m_entries.size())) ? m_entries.at(row).filePath : QString();
}

QString PhotosLibraryModel::fileNameAt(int row) const
{
    return QFileInfo(filePathAt(row)).fileName();
}

QString PhotosLibraryModel::dateTextAt(int row) const
{
    if ((row < 0) || (row >= m_entries.size()) || !m_entries.at(row).dateTime.isValid())
    {
        return QString();
    }

    const QDateTime& dt = m_entries.at(row).dateTime;
    const QLocale locale;

    return locale.toString(dt.date(), QLocale::LongFormat) +
           QLatin1String(" ") + QChar(0x00B7) + QLatin1String(" ") +
           locale.toString(dt.time(), QLocale::ShortFormat);
}

bool PhotosLibraryModel::isVideoAt(int row) const
{
    return ((row >= 0) && (row < m_entries.size())) ? m_entries.at(row).isVideo : false;
}

bool PhotosLibraryModel::isFavoriteAt(int row) const
{
    return ((row >= 0) && (row < m_entries.size())) ? (m_entries.at(row).rating >= FavoriteMinRating) : false;
}

void PhotosLibraryModel::setDefaultThumbnailSize(int size)
{
    m_defaultThumbSize = size;
}

QString PhotosLibraryModel::thumbSourceAt(int row, int size) const
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return QString();
    }

    const PhotosEntry& entry = m_entries.at(row);

    // The version segment changes when the file changed on disk, so that Qt Quick
    // does not reuse its cached texture.

    return QLatin1String("image://dkthumb/") + QString::number(entry.id)                  +
           QLatin1Char('/') + QString::number(m_thumbVersion.value(entry.filePath, 0)) +
           QLatin1Char('/') + QString::number((size > 0) ? size : m_defaultThumbSize)  +
           QLatin1Char('/') + photosEncodePath(entry.filePath);
}

QString PhotosLibraryModel::previewSourceAt(int row, int size) const
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return QString();
    }

    return QLatin1String("image://dkpreview/") + QString::number(qMax(0, size)) +
           QLatin1Char('/') + photosEncodePath(m_entries.at(row).filePath);
}

void PhotosLibraryModel::toggleFavoriteAt(int row)
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return;
    }

    PhotosEntry& entry   = m_entries[row];
    const int newRating  = (entry.rating >= FavoriteMinRating) ? 0 : FavoriteRating;
    entry.rating         = newRating;

    // Goes through the regular digiKam path: database, and metadata
    // written to files or sidecars according to the user's settings.

    FileActionMngr::instance()->assignRating(ItemInfo(entry.id), newRating);

    const QModelIndex idx = index(row);

    Q_EMIT dataChanged(idx, idx, { FavoriteRole });
}

bool PhotosLibraryModel::addToAlbum(int row, const QString& albumName)
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return false;
    }

    const int tagId = albumTagForName(albumName);

    if (tagId <= 0)
    {
        return false;
    }

    FileActionMngr::instance()->assignTag(ItemInfo(m_entries.at(row).id), tagId);

    return true;
}

void PhotosLibraryModel::removeFromCurrentAlbum(int row)
{
    if ((m_filter != Album) || (row < 0) || (row >= m_entries.size()))
    {
        return;
    }

    FileActionMngr::instance()->removeTag(ItemInfo(m_entries.at(row).id), m_albumTagId);
}

void PhotosLibraryModel::slotReloadAlbums()
{
    QVariantList albums;
    const int rootTag = TagsCache::instance()->tagForPath(albumsRootTagName());

    if (rootTag > 0)
    {
        QList<QVariant> values;

        {
            CoreDbAccess access;
            access.backend()->execSql(QLatin1String("SELECT id, name FROM Tags WHERE pid = ? ORDER BY name;"),
                                      rootTag, &values);
        }

        for (int i = 0 ; (i + 1) < values.size() ; i += 2)
        {
            QVariantMap map;
            map.insert(QLatin1String("tagId"), values.at(i).toInt());
            map.insert(QLatin1String("name"),  values.at(i + 1).toString());
            albums << map;
        }
    }

    if (albums != m_albums)
    {
        m_albums = albums;

        Q_EMIT albumsChanged();
        Q_EMIT filterChanged();     // album title may have changed
    }
}

void PhotosLibraryModel::scheduleReload()
{
    // Do not restart a running timer: during a long collection scan
    // this still refreshes the view regularly.

    if (!m_reloadTimer->isActive())
    {
        m_reloadTimer->start();
    }
}

void PhotosLibraryModel::slotCollectionImageChange(const CollectionImageChangeset&)
{
    scheduleReload();
}

void PhotosLibraryModel::slotImageChange(const ImageChangeset& changeset)
{
    const DatabaseFields::Set changes = changeset.changes();

    if (changes & DatabaseFields::CreationDate)
    {
        scheduleReload();

        return;
    }

    if (changes & DatabaseFields::Rating)
    {
        if (m_filter == Favorites)
        {
            scheduleReload();
        }
        else
        {
            refreshRatings(changeset.ids());
        }
    }
}

void PhotosLibraryModel::slotImageTagChange(const ImageTagChangeset& changeset)
{
    if (
        (m_filter == Album) &&
        (changeset.containsTag(m_albumTagId) || (changeset.operation() == ImageTagChangeset::RemovedAll))
       )
    {
        scheduleReload();
    }
}

void PhotosLibraryModel::slotTagChange(const TagChangeset&)
{
    m_albumsTimer->start();
}

void PhotosLibraryModel::refreshRatings(const QList<qlonglong>& ids)
{
    QStringList known;

    for (const qlonglong id : ids)
    {
        if (m_rowOfId.contains(id))
        {
            known << QString::number(id);
        }
    }

    if (known.isEmpty())
    {
        return;
    }

    QList<QVariant> values;

    {
        CoreDbAccess access;
        access.backend()->execSql(QString::fromLatin1("SELECT imageid, rating FROM ImageInformation "
                                                      "WHERE imageid IN (%1);").arg(known.join(QLatin1Char(','))),
                                  &values);
    }

    for (int i = 0 ; (i + 1) < values.size() ; i += 2)
    {
        const int row = m_rowOfId.value(values.at(i).toLongLong(), -1);

        if (row < 0)
        {
            continue;
        }

        const int rating = qMax(0, values.at(i + 1).toInt());

        if (m_entries.at(row).rating != rating)
        {
            m_entries[row].rating = rating;
            const QModelIndex idx = index(row);

            Q_EMIT dataChanged(idx, idx, { FavoriteRole });
        }
    }
}

int PhotosLibraryModel::albumTagForName(const QString& albumName)
{
    QString name = albumName.trimmed();
    name.replace(QLatin1Char('/'), QLatin1Char('-'));

    if (name.isEmpty())
    {
        return 0;
    }

    return TagsCache::instance()->getOrCreateTag(albumsRootTagName() + QLatin1Char('/') + name);
}

void PhotosLibraryModel::invalidateThumbnail(const QString& filePath)
{
    const int row = m_rowOfPath.value(filePath, -1);

    if (row < 0)
    {
        return;
    }

    m_thumbVersion[filePath] += 1;

    const QModelIndex idx = index(row);

    Q_EMIT dataChanged(idx, idx, { ThumbSourceRole });
}

// --- Selection ----------------------------------------------------------------

int PhotosLibraryModel::selectionCount() const
{
    return m_selection.size();
}

int PhotosLibraryModel::selectionRevision() const
{
    return m_selectionRevision;
}

void PhotosLibraryModel::emitSelectionChanged()
{
    ++m_selectionRevision;

    Q_EMIT selectionChanged();
}

bool PhotosLibraryModel::isSelectedAt(int row) const
{
    return ((row >= 0) && (row < m_entries.size())) ? m_selection.contains(m_entries.at(row).id) : false;
}

void PhotosLibraryModel::toggleSelectedAt(int row)
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return;
    }

    const qlonglong id = m_entries.at(row).id;

    if (!m_selection.remove(id))
    {
        m_selection.insert(id);
    }

    m_anchorId = id;
    emitSelectionChanged();
}

void PhotosLibraryModel::selectRangeTo(int row)
{
    const int anchor = rowOfId(m_anchorId);

    if ((anchor < 0) || (row < 0) || (row >= m_entries.size()))
    {
        toggleSelectedAt(row);

        return;
    }

    for (int i = qMin(anchor, row) ; i <= qMax(anchor, row) ; ++i)
    {
        m_selection.insert(m_entries.at(i).id);
    }

    emitSelectionChanged();
}

void PhotosLibraryModel::selectOnly(int row)
{
    m_selection.clear();

    if ((row >= 0) && (row < m_entries.size()))
    {
        m_selection.insert(m_entries.at(row).id);
        m_anchorId = m_entries.at(row).id;
    }

    emitSelectionChanged();
}

void PhotosLibraryModel::selectAll()
{
    for (const PhotosEntry& entry : std::as_const(m_entries))
    {
        m_selection.insert(entry.id);
    }

    emitSelectionChanged();
}

void PhotosLibraryModel::clearSelection()
{
    if (m_selection.isEmpty())
    {
        return;
    }

    m_selection.clear();
    emitSelectionChanged();
}

void PhotosLibraryModel::beginBandSelection(bool additive)
{
    m_bandBase = additive ? m_selection : QSet<qlonglong>();
}

void PhotosLibraryModel::updateBandSelection(const QList<int>& rows)
{
    QSet<qlonglong> selection = m_bandBase;

    for (const int row : rows)
    {
        if ((row >= 0) && (row < m_entries.size()))
        {
            selection.insert(m_entries.at(row).id);
        }
    }

    if (selection != m_selection)
    {
        m_selection = selection;
        emitSelectionChanged();
    }
}

QList<int> PhotosLibraryModel::selectedRows() const
{
    QList<int> rows;

    for (const qlonglong id : std::as_const(m_selection))
    {
        const int row = m_rowOfId.value(id, -1);

        if (row >= 0)
        {
            rows << row;
        }
    }

    std::sort(rows.begin(), rows.end());

    return rows;
}

// --- Actions on the selection -------------------------------------------------

bool PhotosLibraryModel::selectionAllFavorite() const
{
    if (m_selection.isEmpty())
    {
        return false;
    }

    for (const int row : selectedRows())
    {
        if (m_entries.at(row).rating < FavoriteMinRating)
        {
            return false;
        }
    }

    return true;
}

void PhotosLibraryModel::setFavoriteForSelection(bool favorite)
{
    QList<ItemInfo> infos;

    for (const int row : selectedRows())
    {
        PhotosEntry& entry = m_entries[row];

        if (favorite == (entry.rating >= FavoriteMinRating))
        {
            continue;
        }

        entry.rating = favorite ? FavoriteRating : 0;
        infos << ItemInfo(entry.id);
    }

    if (infos.isEmpty())
    {
        return;
    }

    FileActionMngr::instance()->assignRating(infos, favorite ? FavoriteRating : 0);

    Q_EMIT dataChanged(index(0), index(m_entries.size() - 1), { FavoriteRole });
}

bool PhotosLibraryModel::addSelectionToAlbum(const QString& albumName)
{
    const QList<int> rows = selectedRows();

    if (rows.isEmpty())
    {
        return false;
    }

    const int tagId = albumTagForName(albumName);

    if (tagId <= 0)
    {
        return false;
    }

    QList<ItemInfo> infos;

    for (const int row : rows)
    {
        infos << ItemInfo(m_entries.at(row).id);
    }

    FileActionMngr::instance()->assignTag(infos, tagId);

    return true;
}

void PhotosLibraryModel::removeSelectionFromCurrentAlbum()
{
    if (m_filter != Album)
    {
        return;
    }

    QList<ItemInfo> infos;

    for (const int row : selectedRows())
    {
        infos << ItemInfo(m_entries.at(row).id);
    }

    if (!infos.isEmpty())
    {
        FileActionMngr::instance()->removeTag(infos, m_albumTagId);
    }
}

// --- Trash ----------------------------------------------------------------------

bool PhotosLibraryModel::canUndoTrash() const
{
    return !m_undoIds.isEmpty();
}

void PhotosLibraryModel::trashSelection()
{
    QList<qlonglong> ids;

    for (const int row : selectedRows())
    {
        ids << m_entries.at(row).id;
    }

    trashIds(ids);
}

void PhotosLibraryModel::trashAt(int row)
{
    if ((row >= 0) && (row < m_entries.size()))
    {
        trashIds(QList<qlonglong>() << m_entries.at(row).id);
    }
}

void PhotosLibraryModel::trashIds(const QList<qlonglong>& ids)
{
    if (ids.isEmpty())
    {
        return;
    }

    // A new deletion replaces what can be undone.

    m_undoIds.clear();
    m_undoRoots.clear();
    m_undoAttempts = 0;

    QList<ItemInfo> infos;

    for (const qlonglong id : ids)
    {
        const int row = m_rowOfId.value(id, -1);

        if (row < 0)
        {
            continue;
        }

        infos << ItemInfo(id);
        m_trashPending.insert(id);
        m_undoIds.insert(id);
        m_undoRoots.insert(CollectionManager::instance()->albumRootPath(m_entries.at(row).filePath));
        m_selection.remove(id);
    }

    if (infos.isEmpty())
    {
        return;
    }

    // Same code path as the classic views: files go to the collection's
    // .dtrash folder and can be restored from stock digiKam's trash too.

    DIO::del(infos, true);

    // Remove the photos from the view right away.

    Q_EMIT aboutToReload();

    beginResetModel();

    m_entries.removeIf([this] (const PhotosEntry& entry)
        {
            return m_trashPending.contains(entry.id);
        }
    );

    m_rowOfId.clear();
    m_rowOfPath.clear();

    for (int i = 0 ; i < m_entries.size() ; ++i)
    {
        m_rowOfId.insert(m_entries.at(i).id, i);
        m_rowOfPath.insert(m_entries.at(i).filePath, i);
    }

    endResetModel();

    ++m_revision;

    Q_EMIT revisionChanged();
    Q_EMIT countChanged();
    Q_EMIT reloaded();

    emitSelectionChanged();

    Q_EMIT canUndoTrashChanged();
    Q_EMIT trashed(infos.size());

    // The trash job reports nothing when done: refresh once the database changed.

    scheduleReload();
}

void PhotosLibraryModel::undoTrash()
{
    if (m_undoIds.isEmpty())
    {
        return;
    }

    // Find our items in the collection trash folders: each trash entry
    // records the database id of the image it came from.

    DTrashItemInfoList items;

    for (const QString& root : std::as_const(m_undoRoots))
    {
        if (root.isEmpty())
        {
            continue;
        }

        const QDir filesDir(root + QLatin1Char('/') + DTrash::TRASH_FOLDER +
                            QLatin1Char('/') + DTrash::FILES_FOLDER);

        const auto files = filesDir.entryInfoList(QDir::Files);

        for (const QFileInfo& file : files)
        {
            DTrashItemInfo info;
            info.trashPath = file.filePath();
            DTrash::extractJsonForItem(root, file.baseName(), info);

            if (m_undoIds.contains(info.imageId))
            {
                items << info;
            }
        }
    }

    // The trash job may still be moving files: wait a little for the rest.

    if ((items.size() < m_undoIds.size()) && (m_undoAttempts < 10))
    {
        ++m_undoAttempts;
        QTimer::singleShot(300, this, &PhotosLibraryModel::undoTrash);

        return;
    }

    m_undoIds.clear();
    m_undoRoots.clear();
    m_undoAttempts = 0;

    Q_EMIT canUndoTrashChanged();

    if (!items.isEmpty())
    {
        DIO::restoreTrash(items);
    }
}

} // namespace Digikam
