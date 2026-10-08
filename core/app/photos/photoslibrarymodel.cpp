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
#include <QRegularExpression>
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
#include "captionvalues.h"
#include "dio.h"
#include "disjointmetadata.h"
#include "itemposition.h"
#include "photoinfocontainer.h"
#include "videoinfocontainer.h"
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

QString photosPrettyDevice(const QString& make, const QString& model)
{
    QString mk = make.simplified();
    QString md = model.simplified();

    if (md.isEmpty())
    {
        return mk;
    }

    if (mk.isEmpty() || md.startsWith(mk, Qt::CaseInsensitive) ||
        (mk.compare(QLatin1String("Apple"), Qt::CaseInsensitive) == 0))
    {
        return md;
    }

    static const QRegularExpression suffix(QLatin1String("[ ,]+(corporation|corp\\.?|inc\\.?|co\\.?,? ?ltd\\.?|"
                                                         "imaging|optical.*|camera|company)$"),
                                           QRegularExpression::CaseInsensitiveOption);
    mk.remove(suffix);

    if ((mk == mk.toLower()) || (mk == mk.toUpper()))
    {
        mk = mk.left(1).toUpper() + mk.mid(1).toLower();
    }

    if (md.startsWith(mk, Qt::CaseInsensitive))
    {
        return md;
    }

    return mk + QLatin1Char(' ') + md;
}

QString PhotosLibraryModel::albumsRootTagName()
{
    return QLatin1String("Albums");
}

QString PhotosLibraryModel::hiddenTagName()
{
    return QLatin1String("Hidden");
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

    connect(&m_watcher, &QFutureWatcher<QueryResult>::finished,
            this, &PhotosLibraryModel::slotLoaded);

    connect(&m_devicesWatcher, &QFutureWatcher<QVariantList>::finished,
            this, [this] ()
        {
            const QVariantList devices = m_devicesWatcher.result();

            if (devices != m_devices)
            {
                m_devices = devices;

                Q_EMIT devicesChanged();
            }
        }
    );

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
    m_devicesWatcher.waitForFinished();
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

void PhotosLibraryModel::setView(int filter, int tagId)
{
    m_albumTagId = tagId;
    m_filter     = filter;

    Q_EMIT filterChanged();

    reload();
}

void PhotosLibraryModel::showAlbum(int tagId)
{
    setView(Album, tagId);
}

void PhotosLibraryModel::showDeviceTag(int tagId)
{
    setView(DeviceTag, tagId);
}

void PhotosLibraryModel::showCamera(const QString& make, const QString& model)
{
    m_cameraMake  = make;
    m_cameraModel = model;

    setView(Camera, -1);
}

void PhotosLibraryModel::showFiles(const QStringList& filePaths, const QString& title, const QString& key)
{
    m_files      = QSet<QString>(filePaths.constBegin(), filePaths.constEnd());
    m_filesTitle = title;
    m_filesKey   = key;

    setView(Files, -1);
}

void PhotosLibraryModel::showFolder(const QString& folderPath)
{
    m_folder = QDir::cleanPath(folderPath);

    setView(Folder, -1);
}

QString PhotosLibraryModel::folderPath() const
{
    return (m_filter == Folder) ? m_folder : QString();
}

int PhotosLibraryModel::rowOfPath(const QString& filePath) const
{
    return m_rowOfPath.value(QDir::cleanPath(filePath), -1);
}

QString PhotosLibraryModel::filesKey() const
{
    return (m_filter == Files) ? m_filesKey : QString();
}

QVariantList PhotosLibraryModel::devices() const
{
    return m_devices;
}

QString PhotosLibraryModel::title() const
{
    switch (m_filter)
    {
        case Favorites:
            return i18n("Favorites");

        case Videos:
            return i18n("Videos");

        case Files:
            return m_filesTitle;

        case Folder:
        {
            // "2026 › 10" inside a library folder, its name for the library folder itself.

            const QString root     = CollectionManager::instance()->albumRootPath(m_folder);
            const QString relative = root.isEmpty() ? QString() : QDir(root).relativeFilePath(m_folder);

            if (!relative.isEmpty() && (relative != QLatin1String(".")))
            {
                return relative.split(QLatin1Char('/'), Qt::SkipEmptyParts).join(QString::fromUtf8(" \u203A "));
            }

            return QFileInfo(m_folder).fileName().isEmpty() ? m_folder : QFileInfo(m_folder).fileName();
        }

        case Camera:
            return photosPrettyDevice(m_cameraMake, m_cameraModel);

        case DeviceTag:
        {
            for (const QVariant& device : std::as_const(m_devices))
            {
                const QVariantMap map = device.toMap();

                if (map.value(QLatin1String("tagId")).toInt() == m_albumTagId)
                {
                    return map.value(QLatin1String("name")).toString();
                }
            }

            return i18n("Device");
        }

        case Screenshots:
            return i18n("Screenshots");

        case Raw:
            return i18n("RAW");

        case Panoramas:
            return i18n("Panoramas");

        case Selfies:
            return i18n("Selfies");

        case Hidden:
            return i18n("Hidden");

        case Trash:
            return i18n("Recently Deleted");

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

    Query query;
    query.filter      = m_filter;
    query.tagId       = m_albumTagId;
    query.make        = m_cameraMake;
    query.model       = m_cameraModel;
    query.files       = m_files;
    query.folder      = m_folder;
    query.hiddenTagId = TagsCache::instance()->tagForPath(hiddenTagName());

    m_watcher.setFuture(QtConcurrent::run(&PhotosLibraryModel::queryEntries, query));
}

PhotosLibraryModel::QueryResult PhotosLibraryModel::queryEntries(const Query& query)
{
    if (query.filter == Trash)
    {
        return queryTrash();
    }

    QString join;
    QString where;
    QList<QVariant> bound;
    const int filter = query.filter;

    // Screenshots: names given by phones and desktops, or PNG files without camera.

    static const char* const screenshot =
        "(Images.name LIKE '%screenshot%' OR Images.name LIKE 'Screen Shot%' OR "
        " Images.name LIKE 'Bildschirmfoto%' OR Images.name LIKE 'Capture d%cran%' OR "
        " Images.name LIKE 'Schermata%' OR Images.name LIKE 'Captura de pantalla%' OR "
        " (ImageInformation.format = 'PNG' AND COALESCE(ImageMetadata.make, '') = ''))";

    if ((filter == Camera) || (filter == Screenshots) || (filter == Panoramas) || (filter == Selfies))
    {
        join += QLatin1String(" LEFT JOIN ImageMetadata ON ImageMetadata.imageid = Images.id ");
    }

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
        case DeviceTag:
        {
            join += QLatin1String(" INNER JOIN ImageTags ON ImageTags.imageid = Images.id ");
            where = QLatin1String(" AND ImageTags.tagid = ?");
            bound << query.tagId;
            break;
        }

        case Hidden:
        {
            join += QLatin1String(" INNER JOIN ImageTags ON ImageTags.imageid = Images.id ");
            where = QLatin1String(" AND ImageTags.tagid = ?");
            bound << query.hiddenTagId;
            break;
        }

        case Camera:
        {
            where = QLatin1String(" AND COALESCE(ImageMetadata.make, '') = ? AND COALESCE(ImageMetadata.model, '') = ?");
            bound << query.make << query.model;
            break;
        }

        case Screenshots:
        {
            where = QLatin1String(" AND ") + QLatin1String(screenshot);
            break;
        }

        case Raw:
        {
            where = QLatin1String(" AND ImageInformation.format LIKE 'RAW%'");
            break;
        }

        case Panoramas:
        {
            // Wide (or tall) photos taken with a camera: tall screenshots are not panoramas.

            where = QLatin1String(" AND ImageInformation.width > 0 AND ImageInformation.height > 0 "
                                  " AND (ImageInformation.width >= 2 * ImageInformation.height OR "
                                  "      ImageInformation.height >= 2 * ImageInformation.width) "
                                  " AND COALESCE(ImageMetadata.make, '') <> '' AND NOT ") + QLatin1String(screenshot);
            break;
        }

        case Selfies:
        {
            // Phones name the lens: "iPhone 15 Pro front camera 2.69mm f/1.9".

            where = QLatin1String(" AND ImageMetadata.lens LIKE '%front%'");
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
        "       ImageInformation.creationDate, ImageInformation.rating, Images.album "
        "FROM Images "
        "INNER JOIN Albums ON Albums.id = Images.album "
        "LEFT JOIN ImageInformation ON ImageInformation.imageid = Images.id "
        "%1"
        "WHERE Images.status = %2 "
        "  AND (Images.category = %3 OR Images.category = %4) "
        "  AND Images.id NOT IN (SELECT subject FROM ImageRelations WHERE type = %5) "
        "%6 %7 "
        "ORDER BY ImageInformation.creationDate DESC, Images.id DESC;")
        .arg(join)
        .arg(int(DatabaseItem::Visible))
        .arg(int(DatabaseItem::Image))
        .arg(int(DatabaseItem::Video))
        .arg(int(DatabaseRelation::Grouped))
        .arg(where)
        .arg(((query.hiddenTagId > 0) && (filter != Hidden) && (filter != Files))
             ? QString::fromLatin1(" AND Images.id NOT IN (SELECT imageid FROM ImageTags WHERE tagid = %1) ")
                   .arg(query.hiddenTagId)
             : QString());

    QList<QVariant> values;

    {
        CoreDbAccess access;
        access.backend()->execSql(sql, bound, &values);
    }

    QHash<int, QString> rootPaths;
    QueryResult         result;
    QList<PhotosEntry>& entries = result.entries;
    entries.reserve(values.size() / 8);

    // Live Photos (iPhone) and motion photos saved as two files: a photo and a
    // short video with the same name in the same folder. Paired from the file
    // names, as other devices of a synced library see them: the video is
    // played from the photo, not listed on its own.

    QSet<QString>           photoKeys;
    QHash<QString, QString> videoNames;     // key -> video file name

    {
        QList<QVariant> names;

        {
            CoreDbAccess access;
            access.backend()->execSql(QString::fromLatin1("SELECT album, name, category FROM Images "
                                                          "WHERE status = %1 AND (category = %2 OR category = %3);")
                                          .arg(int(DatabaseItem::Visible))
                                          .arg(int(DatabaseItem::Image))
                                          .arg(int(DatabaseItem::Video)), &names);
        }

        QSet<QString> videoKeys;

        for (int i = 0 ; (i + 2) < names.size() ; i += 3)
        {
            const QString name = names.at(i + 1).toString();
            const QString key  = names.at(i).toString() + QLatin1Char('/') + name.section(QLatin1Char('.'), 0, -2).toLower();

            if (names.at(i + 2).toInt() == DatabaseItem::Video)
            {
                videoNames.insert(key, name);
            }
            else
            {
                photoKeys.insert(key);
            }
        }
    }

    for (int i = 0 ; (i + 7) < values.size() ; i += 8)
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

        if ((filter == Files) && !query.files.contains(entry.filePath))
        {
            continue;
        }

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        const Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif

        if ((filter == Folder) && !entry.filePath.startsWith(query.folder + QLatin1Char('/'), pathCase))
        {
            continue;
        }

        const QString name = values.at(i + 1).toString();
        const QString key  = values.at(i + 7).toString() + QLatin1Char('/') + name.section(QLatin1Char('.'), 0, -2).toLower();

        if (entry.isVideo)
        {
            if (photoKeys.contains(key) && (filter != Files))
            {
                continue;       // the motion of a Live Photo
            }
        }
        else
        {
            const auto video = videoNames.constFind(key);

            if (video != videoNames.constEnd())
            {
                entry.livePath = QFileInfo(entry.filePath).path() + QLatin1Char('/') + video.value();
            }
        }

        entries << entry;
    }

    return result;
}

PhotosLibraryModel::QueryResult PhotosLibraryModel::queryTrash()
{
    // Each trashed file has a JSON record with its database id and the deletion time.

    QueryResult result;

    const QStringList roots = CollectionManager::instance()->allAvailableAlbumRootPaths();

    for (const QString& root : roots)
    {
        const QDir filesDir(root + QLatin1Char('/') + DTrash::TRASH_FOLDER +
                            QLatin1Char('/') + DTrash::FILES_FOLDER);

        const auto files = filesDir.entryInfoList(QDir::Files);

        // Sidecars are trashed as entries of their own (same image id): keep
        // them with their photo, restored or deleted together.

        QList<DTrashItemInfo>          photos;
        QHash<QString, DTrashItemInfo> sidecars;    // by original path

        for (const QFileInfo& file : files)
        {
            DTrashItemInfo info;
            info.trashPath = file.filePath();
            DTrash::extractJsonForItem(root, file.baseName(), info);

            if (file.suffix().compare(QLatin1String("xmp"), Qt::CaseInsensitive) == 0)
            {
                sidecars.insert(info.collectionPath, info);
            }
            else if (info.imageId > 0)
            {
                photos << info;
            }
        }

        for (const DTrashItemInfo& info : std::as_const(photos))
        {
            // Sidecars first: restored before their photo, which is scanned
            // as soon as it is back (as digiKam moves and copies them first).

            DTrashItemInfoList items;

            const QFileInfo original(info.collectionPath);
            const QString candidates[] =
            {
                info.collectionPath + QLatin1String(".xmp"),
                info.collectionPath + QLatin1String(".XMP"),
                original.path() + QLatin1Char('/') + original.completeBaseName() + QLatin1String(".xmp"),
                original.path() + QLatin1Char('/') + original.completeBaseName() + QLatin1String(".XMP")
            };

            for (const QString& candidate : candidates)
            {
                if (sidecars.contains(candidate))
                {
                    items << sidecars.take(candidate);
                }
            }

            items << info;

            PhotosEntry entry;
            entry.id       = info.imageId;
            entry.filePath = info.trashPath;
            entry.dateTime = info.deletionTimestamp;

            result.entries << entry;
            result.trash.insert(info.imageId, items);
        }
    }

    if (!result.entries.isEmpty())
    {
        QStringList ids;

        for (const PhotosEntry& entry : std::as_const(result.entries))
        {
            ids << QString::number(entry.id);
        }

        QList<QVariant> values;

        {
            CoreDbAccess access;
            access.backend()->execSql(QString::fromLatin1("SELECT id, category FROM Images WHERE id IN (%1);")
                                          .arg(ids.join(QLatin1Char(','))), &values);
        }

        QSet<qlonglong> videos;

        for (int i = 0 ; (i + 1) < values.size() ; i += 2)
        {
            if (values.at(i + 1).toInt() == DatabaseItem::Video)
            {
                videos.insert(values.at(i).toLongLong());
            }
        }

        for (PhotosEntry& entry : result.entries)
        {
            entry.isVideo = videos.contains(entry.id);
        }
    }

    std::sort(result.entries.begin(), result.entries.end(),
              [] (const PhotosEntry& a, const PhotosEntry& b)
        {
            return (a.dateTime > b.dateTime);
        }
    );

    return result;
}

void PhotosLibraryModel::slotLoaded()
{
    Q_EMIT aboutToReload();

    beginResetModel();

    const QueryResult result = m_watcher.result();
    m_entries                = result.entries;
    m_trashInfos             = result.trash;

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

QUrl PhotosLibraryModel::liveUrlAt(int row) const
{
    if ((row < 0) || (row >= m_entries.size()) || m_entries.at(row).livePath.isEmpty())
    {
        return QUrl();
    }

    return QUrl::fromLocalFile(m_entries.at(row).livePath);
}

QUrl PhotosLibraryModel::fileUrlAt(int row) const
{
    const QString path = filePathAt(row);

    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
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

    // Trashed files: by path only, the database id points to the original location.

    return QLatin1String("image://dkthumb/") + QString::number((m_filter == Trash) ? 0 : entry.id) +
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

QVariantMap PhotosLibraryModel::infoAt(int row) const
{
    QVariantMap map;

    if ((row < 0) || (row >= m_entries.size()))
    {
        return map;
    }

    const PhotosEntry& entry = m_entries.at(row);
    const ItemInfo info(entry.id);
    const QLocale locale;
    const QFileInfo file(entry.filePath);

    map.insert(QLatin1String("dateText"), dateTextAt(row));
    map.insert(QLatin1String("fileName"), (m_filter == Trash) ? info.name() : file.fileName());
    map.insert(QLatin1String("folder"),   (m_filter == Trash) ? QString() : QDir::toNativeSeparators(file.path()));
    map.insert(QLatin1String("size"),     locale.formattedDataSize(file.size(), 1, QLocale::DataSizeTraditionalFormat));
    map.insert(QLatin1String("caption"),  info.comment());
    map.insert(QLatin1String("isVideo"),  entry.isVideo);

    const QSize size = info.dimensions();

    if (!size.isEmpty())
    {
        map.insert(QLatin1String("dimensions"), QString::fromUtf8("%1 \u00D7 %2").arg(size.width()).arg(size.height()));
        const double mp = double(size.width()) * size.height() / 1.0e6;

        if (mp >= 0.5)
        {
            map.insert(QLatin1String("megapixels"), i18n("%1 MP", locale.toString(mp, 'f', (mp < 10.0) ? 1 : 0)));
        }
    }

    map.insert(QLatin1String("format"), info.format().remove(QLatin1String("RAW-")));

    if (entry.isVideo)
    {
        const VideoInfoContainer video = info.videoInfoContainer();
        bool ok                        = false;
        const int ms                   = video.duration.toInt(&ok);

        if (ok && (ms > 0))
        {
            const int secs = ms / 1000;
            map.insert(QLatin1String("duration"), (secs >= 3600) ? QString::asprintf("%d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60)
                                                                 : QString::asprintf("%d:%02d", secs / 60, secs % 60));
        }

        map.insert(QLatin1String("videoCodec"), video.videoCodec);
        map.insert(QLatin1String("frameRate"),  video.frameRate.isEmpty() ? QString() : i18n("%1 fps", video.frameRate));
    }

    const PhotoInfoContainer photo = info.photoInfoContainer();
    map.insert(QLatin1String("camera"),   photosPrettyDevice(photo.make, photo.model));
    map.insert(QLatin1String("lens"),     photo.lens);
    map.insert(QLatin1String("aperture"), photo.aperture);
    map.insert(QLatin1String("exposure"), photo.exposureTime);
    map.insert(QLatin1String("iso"),      photo.sensitivity);
    map.insert(QLatin1String("focal"),    photo.focalLength);

    const ItemPosition position = info.imagePosition();

    if (!position.isEmpty() && position.hasCoordinates())
    {
        const double lat = position.latitudeNumber();
        const double lon = position.longitudeNumber();

        map.insert(QLatin1String("latitude"),  lat);
        map.insert(QLatin1String("longitude"), lon);
        map.insert(QLatin1String("location"),  QString::fromUtf8("%1\u00B0 %2, %3\u00B0 %4")
                                                   .arg(locale.toString(qAbs(lat), 'f', 4)).arg((lat >= 0) ? QLatin1Char('N') : QLatin1Char('S'))
                                                   .arg(locale.toString(qAbs(lon), 'f', 4)).arg((lon >= 0) ? QLatin1Char('E') : QLatin1Char('W')));
    }

    // Albums, device and place names: tags under "Albums/", "Devices/", "Places/"...

    QStringList albums;
    QStringList devices;
    QStringList places;
    TagsCache* const tags = TagsCache::instance();

    for (const int tagId : info.tagIds())
    {
        if (tags->isInternalTag(tagId))
        {
            continue;
        }

        const QString path = tags->tagPath(tagId, TagsCache::NoLeadingSlash);

        if      (path.startsWith(albumsRootTagName() + QLatin1Char('/')))
        {
            albums << tags->tagName(tagId);
        }
        else if (path.startsWith(QLatin1String("Devices/")))
        {
            devices << tags->tagName(tagId);
        }
        else if (path.startsWith(QLatin1String("Places/")))
        {
            places << path.mid(7).split(QLatin1Char('/')).join(QLatin1String(", "));
        }
    }

    map.insert(QLatin1String("albums"),  albums);
    map.insert(QLatin1String("devices"), devices);
    map.insert(QLatin1String("places"),  places);

    return map;
}

void PhotosLibraryModel::setCaptionAt(int row, const QString& caption)
{
    if ((row < 0) || (row >= m_entries.size()))
    {
        return;
    }

    const ItemInfo info(m_entries.at(row).id);

    if (info.comment() == caption.trimmed())
    {
        return;
    }

    CaptionValues value;
    value.caption = caption.trimmed();
    value.date    = QDateTime::currentDateTime();

    CaptionsMap captions;

    if (!value.caption.isEmpty())
    {
        captions.insert(QLatin1String("x-default"), value);
    }

    // The same path as the caption editor of the classic interface: database,
    // and files or sidecars according to the metadata settings.

    DisjointMetadata hub;
    hub.load(info);
    hub.setComments(captions);

    FileActionMngr::instance()->applyMetadata(QList<ItemInfo>() << info, hub);
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

    slotReloadDevices();
}

/**
 * Named devices (tags given at import), then the cameras of the EXIF data,
 * most photos first.
 */
static QVariantList queryDevices()
{
    QVariantList devices;
    const int rootTag = TagsCache::instance()->tagForPath(QLatin1String("Devices"));

    QList<QVariant> values;

    if (rootTag > 0)
    {
        {
            CoreDbAccess access;
            access.backend()->execSql(QString::fromLatin1(
                "SELECT Tags.id, Tags.name, COUNT(Images.id) FROM Tags "
                "INNER JOIN ImageTags ON ImageTags.tagid = Tags.id "
                "INNER JOIN Images ON Images.id = ImageTags.imageid "
                "WHERE Tags.pid = ? AND Images.status = %1 "
                "  AND Images.id NOT IN (SELECT subject FROM ImageRelations WHERE type = %2) "
                "GROUP BY Tags.id, Tags.name ORDER BY COUNT(Images.id) DESC;")
                .arg(int(DatabaseItem::Visible)).arg(int(DatabaseRelation::Grouped)), rootTag, &values);
        }

        for (int i = 0 ; (i + 2) < values.size() ; i += 3)
        {
            QVariantMap map;
            map.insert(QLatin1String("kind"),  QLatin1String("tag"));
            map.insert(QLatin1String("tagId"), values.at(i).toInt());
            map.insert(QLatin1String("name"),  values.at(i + 1).toString());
            map.insert(QLatin1String("count"), values.at(i + 2).toInt());
            devices << map;
        }
    }

    values.clear();

    {
        CoreDbAccess access;
        access.backend()->execSql(QString::fromLatin1(
            "SELECT COALESCE(ImageMetadata.make, ''), COALESCE(ImageMetadata.model, ''), COUNT(*) "
            "FROM ImageMetadata INNER JOIN Images ON Images.id = ImageMetadata.imageid "
            "WHERE Images.status = %1 "
            "  AND Images.id NOT IN (SELECT subject FROM ImageRelations WHERE type = %2) "
            "GROUP BY COALESCE(ImageMetadata.make, ''), COALESCE(ImageMetadata.model, '') "
            "ORDER BY COUNT(*) DESC;").arg(int(DatabaseItem::Visible)).arg(int(DatabaseRelation::Grouped)), &values);
    }

    for (int i = 0 ; (i + 2) < values.size() ; i += 3)
    {
        const QString make  = values.at(i).toString();
        const QString model = values.at(i + 1).toString();

        if (make.isEmpty() && model.isEmpty())
        {
            continue;
        }

        QVariantMap map;
        map.insert(QLatin1String("kind"),  QLatin1String("camera"));
        map.insert(QLatin1String("make"),  make);
        map.insert(QLatin1String("model"), model);
        map.insert(QLatin1String("name"),  photosPrettyDevice(make, model));
        map.insert(QLatin1String("count"), values.at(i + 2).toInt());
        devices << map;
    }

    return devices;
}

void PhotosLibraryModel::slotReloadDevices()
{
    if (m_devicesWatcher.isRunning())
    {
        m_albumsTimer->start();

        return;
    }

    m_devicesWatcher.setFuture(QtConcurrent::run(&queryDevices));
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
    m_albumsTimer->start();
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
    const bool removedAll = (changeset.operation() == ImageTagChangeset::RemovedAll);

    if (
        ((m_filter == Album) || (m_filter == DeviceTag)) &&
        (changeset.containsTag(m_albumTagId) || removedAll)
       )
    {
        scheduleReload();
    }

    // Hiding or showing photos changes every view.

    const int hiddenTag = TagsCache::instance()->tagForPath(hiddenTagName());

    if ((hiddenTag > 0) && (changeset.containsTag(hiddenTag) || removedAll))
    {
        scheduleReload();
    }

    // Device counts.

    m_albumsTimer->start();
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
        const ItemInfo info(id);

        if (info.isNull())
        {
            continue;
        }

        infos << info;
        m_trashPending.insert(id);
        m_undoIds.insert(id);
        m_undoRoots.insert(CollectionManager::instance()->albumRootPath(info.filePath()));
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

void PhotosLibraryModel::trashImageIds(const QList<qlonglong>& ids)
{
    trashIds(ids);
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

// --- Hidden photos --------------------------------------------------------------

bool PhotosLibraryModel::isHiddenAt(int row) const
{
    // Hidden photos only appear in the Hidden view (and in an import).

    return (m_filter == Hidden) && (row >= 0) && (row < m_entries.size());
}

void PhotosLibraryModel::setHiddenIds(const QList<qlonglong>& ids, bool hidden)
{
    if (ids.isEmpty())
    {
        return;
    }

    QList<ItemInfo> infos;

    for (const qlonglong id : ids)
    {
        infos << ItemInfo(id);
    }

    // A regular tag: written to the sidecars, hence synced, and visible in
    // stock digiKam as the "Hidden" tag.

    if (hidden)
    {
        const int tagId = TagsCache::instance()->getOrCreateTag(hiddenTagName());

        if (tagId > 0)
        {
            FileActionMngr::instance()->assignTag(infos, tagId);
        }
    }
    else
    {
        const int tagId = TagsCache::instance()->tagForPath(hiddenTagName());

        if (tagId > 0)
        {
            FileActionMngr::instance()->removeTag(infos, tagId);
        }
    }

    // Leave the current view at once.

    Q_EMIT aboutToReload();

    beginResetModel();

    const QSet<qlonglong> gone(ids.constBegin(), ids.constEnd());

    m_entries.removeIf([&gone] (const PhotosEntry& entry)
        {
            return gone.contains(entry.id);
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

    m_selection.subtract(gone);

    ++m_revision;

    Q_EMIT revisionChanged();
    Q_EMIT countChanged();
    Q_EMIT reloaded();

    emitSelectionChanged();
}

void PhotosLibraryModel::setHiddenAt(int row, bool hidden)
{
    if ((row >= 0) && (row < m_entries.size()))
    {
        setHiddenIds(QList<qlonglong>() << m_entries.at(row).id, hidden);
    }
}

void PhotosLibraryModel::setHiddenForSelection(bool hidden)
{
    QList<qlonglong> ids;

    for (const int row : selectedRows())
    {
        ids << m_entries.at(row).id;
    }

    setHiddenIds(ids, hidden);
}

// --- Recently Deleted -------------------------------------------------------------

QString PhotosLibraryModel::deletedTextAt(int row) const
{
    if ((m_filter != Trash) || (row < 0) || (row >= m_entries.size()))
    {
        return QString();
    }

    const DTrashItemInfoList infos = m_trashInfos.value(m_entries.at(row).id);

    if (infos.isEmpty())
    {
        return QString();
    }

    const DTrashItemInfo& info = infos.constLast();     // the photo, after its sidecars
    const QLocale locale;

    return i18n("Deleted %1 \u00B7 was %2",
                locale.toString(info.deletionTimestamp, QLocale::ShortFormat),
                info.collectionRelativePath);
}

void PhotosLibraryModel::trashActionIds(const QList<qlonglong>& ids, bool restore)
{
    DTrashItemInfoList items;

    for (const qlonglong id : ids)
    {
        items << m_trashInfos.value(id);
    }

    if (items.isEmpty())
    {
        return;
    }

    if (restore)
    {
        DIO::restoreTrash(items);
    }
    else
    {
        DIO::emptyTrash(items);
    }

    // Leave the view at once; the jobs report nothing when done.

    Q_EMIT aboutToReload();

    beginResetModel();

    const QSet<qlonglong> gone(ids.constBegin(), ids.constEnd());

    m_entries.removeIf([&gone] (const PhotosEntry& entry)
        {
            return gone.contains(entry.id);
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

    m_selection.subtract(gone);

    ++m_revision;

    Q_EMIT revisionChanged();
    Q_EMIT countChanged();
    Q_EMIT reloaded();

    emitSelectionChanged();

    QTimer::singleShot(1500, this, &PhotosLibraryModel::reload);
}

void PhotosLibraryModel::restoreAt(int row)
{
    if ((row >= 0) && (row < m_entries.size()))
    {
        trashActionIds(QList<qlonglong>() << m_entries.at(row).id, true);
    }
}

void PhotosLibraryModel::restoreSelection()
{
    QList<qlonglong> ids;

    for (const int row : selectedRows())
    {
        ids << m_entries.at(row).id;
    }

    trashActionIds(ids, true);
}

void PhotosLibraryModel::deleteForeverAt(int row)
{
    if ((row >= 0) && (row < m_entries.size()))
    {
        trashActionIds(QList<qlonglong>() << m_entries.at(row).id, false);
    }
}

void PhotosLibraryModel::deleteSelectionForever()
{
    QList<qlonglong> ids;

    for (const int row : selectedRows())
    {
        ids << m_entries.at(row).id;
    }

    trashActionIds(ids, false);
}

void PhotosLibraryModel::emptyTrash()
{
    if (m_filter != Trash)
    {
        return;
    }

    trashActionIds(m_trashInfos.keys(), false);
}

} // namespace Digikam
