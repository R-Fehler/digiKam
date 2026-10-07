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
            return thumbSourceAt(index.row());

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
    m_rowOfId.clear();
    m_rowOfId.reserve(m_entries.size());

    for (int i = 0 ; i < m_entries.size() ; ++i)
    {
        m_rowOfId.insert(m_entries.at(i).id, i);
    }

    endResetModel();

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

QString PhotosLibraryModel::thumbSourceAt(int row) const
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return QString();
    }

    const PhotosEntry& entry = m_entries.at(row);

    return QLatin1String("image://dkthumb/") + QString::number(entry.id) +
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
    QString name = albumName.trimmed();
    name.replace(QLatin1Char('/'), QLatin1Char('-'));

    if ((row < 0) || (row >= m_entries.size()) || name.isEmpty())
    {
        return false;
    }

    const int tagId = TagsCache::instance()->getOrCreateTag(albumsRootTagName() + QLatin1Char('/') + name);

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

} // namespace Digikam
