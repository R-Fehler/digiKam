/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - the folders of the library (digiKam's
 *               collections): list, add, remove, and opening a
 *               folder or a photo given on the command line.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photoslibraries.h"

// Qt includes

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>

// KDE includes

#include <kconfiggroup.h>
#include <ksharedconfig.h>

#ifdef Q_OS_WIN
#   include <windows.h>
#endif
#include <QUrl>

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "collectionlocation.h"
#include "collectionmanager.h"
#include "coredbaccess.h"
#include "coredbbackend.h"
#include "coredbconstants.h"
#include "scancontroller.h"

namespace Digikam
{

namespace
{

QString cleanPath(const QString& path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();

    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

bool isInside(const QString& path, const QString& folder)
{
    const Qt::CaseSensitivity cs =
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
                                   Qt::CaseInsensitive;
#else
                                   Qt::CaseSensitive;
#endif

    return (path.compare(folder, cs) == 0) ||
           path.startsWith(folder.endsWith(QLatin1Char('/')) ? folder : folder + QLatin1Char('/'), cs);
}

} // namespace

PhotosLibraries::PhotosLibraries(QObject* const parent)
    : QObject(parent)
{
    connect(CollectionManager::instance(), &CollectionManager::locationStatusChanged,
            this, &PhotosLibraries::foldersChanged);

    connect(CollectionManager::instance(), &CollectionManager::locationPropertiesChanged,
            this, &PhotosLibraries::foldersChanged);
}

QVariantList PhotosLibraries::folders() const
{
    QVariantList folders;

    const QList<CollectionLocation> locations = CollectionManager::instance()->allLocations();

    for (const CollectionLocation& location : locations)
    {
        if (location.status() == CollectionLocation::LocationDeleted)
        {
            continue;
        }

        // The path of a location on a drive which is not connected is unknown:
        // show its label (by default the folder name given when it was added).

        const QString path = location.albumRootPath();

        QVariantMap folder;
        folder.insert(QLatin1String("id"),        location.id());
        folder.insert(QLatin1String("path"),      path);
        folder.insert(QLatin1String("name"),      !location.label().isEmpty() ? location.label()
                                                                              : QFileInfo(path).fileName());
        folder.insert(QLatin1String("available"), location.isAvailable());
        folder.insert(QLatin1String("removable"), location.type() == CollectionLocation::VolumeRemovable);
        folder.insert(QLatin1String("network"),   (location.type() == CollectionLocation::Network) ||
                                                  (location.isAvailable() && isNetworkPath(path)));
        folder.insert(QLatin1String("syncTool"),  location.isAvailable() ? syncToolOf(path) : QString());
        folders << folder;
    }

    return folders;
}

QVariantMap PhotosLibraries::checkPath(const QString& path) const
{
    QVariantMap result;
    QString target = path;

    if (target.startsWith(QLatin1String("file:")))
    {
        target = QUrl(target).toLocalFile();
    }

    const QFileInfo info(target);

    if (!info.exists())
    {
        result.insert(QLatin1String("message"), i18n("%1 does not exist.", QDir::toNativeSeparators(target)));

        return result;
    }

    const QString folder = cleanPath(info.isDir() ? info.absoluteFilePath() : info.absolutePath());

    result.insert(QLatin1String("path"), folder);
    result.insert(QLatin1String("name"), QFileInfo(folder).fileName().isEmpty() ? folder : QFileInfo(folder).fileName());

    if (!info.isDir())
    {
        result.insert(QLatin1String("file"), cleanPath(info.absoluteFilePath()));
    }

    for (const QString& root : CollectionManager::instance()->allAvailableAlbumRootPaths())
    {
        if (isInside(folder, cleanPath(root)))
        {
            result.insert(QLatin1String("inLibrary"), true);
            result.insert(QLatin1String("root"),      root);

            return result;
        }
    }

    // Not in the library yet: digiKam tells whether it can become a library
    // folder (e.g. not when it contains one already).

    QList<CollectionLocation> assumeDeleted;
    QString message;
    const int check = CollectionManager::instance()->checkLocation(QUrl::fromLocalFile(folder), assumeDeleted, &message);

    result.insert(QLatin1String("inLibrary"), false);
    result.insert(QLatin1String("canAdd"),    (check == CollectionManager::LocationAllRight) ||
                                              (check == CollectionManager::LocationHasProblems));
    result.insert(QLatin1String("message"),   message);

    return result;
}

QString PhotosLibraries::chooseFolder() const
{
    return QFileDialog::getExistingDirectory(QApplication::activeWindow(),
                                             i18n("Add a folder to the library"),
                                             QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
}

QString PhotosLibraries::addFolder(const QString& path)
{
    const QString folder = cleanPath(path);

    QList<CollectionLocation> assumeDeleted;
    QString message;
    const int check = CollectionManager::instance()->checkLocation(QUrl::fromLocalFile(folder), assumeDeleted, &message);

    if ((check != CollectionManager::LocationAllRight) && (check != CollectionManager::LocationHasProblems))
    {
        return message.isEmpty() ? i18n("%1 cannot be added to the library.", QDir::toNativeSeparators(folder))
                                 : message;
    }

    const CollectionLocation location = CollectionManager::instance()->addLocation(QUrl::fromLocalFile(folder),
                                                                                  QFileInfo(folder).fileName());

    if (location.isNull())
    {
        return i18n("%1 cannot be added to the library.", QDir::toNativeSeparators(folder));
    }

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode: library folder added" << folder;

    // Find the photos and videos (in the background, the library fills in).

    ScanController::instance()->scheduleCollectionScan(location.albumRootPath());

    Q_EMIT foldersChanged();

    return QString();
}

void PhotosLibraries::removeFolder(int id)
{
    const CollectionLocation location = CollectionManager::instance()->locationForAlbumRootId(id);

    if (location.isNull())
    {
        return;
    }

    // The files stay where they are; their database entries go (information
    // saved in sidecar files comes back if the folder is added again).

    CollectionManager::instance()->removeLocation(location);

    Q_EMIT foldersChanged();
}

int PhotosLibraries::photoCount(int id) const
{
    QList<QVariant> values;

    {
        CoreDbAccess access;
        access.backend()->execSql(QString::fromLatin1("SELECT COUNT(*) FROM Images "
                                                      "INNER JOIN Albums ON Albums.id = Images.album "
                                                      "WHERE Albums.albumRoot = ? AND Images.status = %1 "
                                                      "AND (Images.category = %2 OR Images.category = %3);")
                                      .arg(int(DatabaseItem::Visible))
                                      .arg(int(DatabaseItem::Image))
                                      .arg(int(DatabaseItem::Video)),
                                  id, &values);
    }

    return values.isEmpty() ? 0 : values.constFirst().toInt();
}

bool PhotosLibraries::isNetworkPath(const QString& path)
{
    if (path.startsWith(QLatin1String("//")) || path.startsWith(QLatin1String("\\\\")))
    {
        return true;    // UNC path
    }

#ifdef Q_OS_WIN

    const QString root        = QStorageInfo(path).rootPath();
    const std::wstring native = QDir::toNativeSeparators(root).toStdWString();

    if (!root.isEmpty() && (GetDriveTypeW(native.c_str()) == DRIVE_REMOTE))
    {
        return true;
    }

#endif

    static const QStringList networkTypes =
    {
        QLatin1String("cifs"),     QLatin1String("smb3"),       QLatin1String("smbfs"),
        QLatin1String("nfs"),      QLatin1String("nfs4"),       QLatin1String("afpfs"),
        QLatin1String("webdav"),   QLatin1String("davfs"),      QLatin1String("fuse.sshfs"),
        QLatin1String("sshfs"),    QLatin1String("fuse.rclone"), QLatin1String("fuse.gvfsd-fuse"),
        QLatin1String("9p"),       QLatin1String("ceph"),       QLatin1String("glusterfs")
    };

    return networkTypes.contains(QString::fromLatin1(QStorageInfo(path).fileSystemType()).toLower());
}

QString PhotosLibraries::syncToolOf(const QString& path)
{
    // Marker files of the sync tools, in the folder or a parent folder.

    QDir dir(path);

    for (int depth = 0 ; depth < 32 ; ++depth)
    {
        if      (dir.exists(QLatin1String(".stfolder")))
        {
            return QLatin1String("Syncthing");
        }
        else if (dir.exists(QLatin1String(".dropbox")) || dir.exists(QLatin1String(".dropbox.cache")))
        {
            return QLatin1String("Dropbox");
        }
        else if (!dir.entryList(QStringList() << QLatin1String(".sync_*.db") << QLatin1String("._sync_*.db")
                                              << QLatin1String(".owncloudsync.log") << QLatin1String(".nextcloudsync.log"),
                                QDir::Files | QDir::Hidden).isEmpty())
        {
            return QLatin1String("Nextcloud");
        }
        else if (dir.exists(QLatin1String(".sync")) && dir.exists(QLatin1String(".sync/ID")))
        {
            return QLatin1String("Resilio Sync");
        }

        const QString name = dir.dirName();

        if ((name == QLatin1String("My Drive")) || name.startsWith(QLatin1String("Google Drive")))
        {
            return QLatin1String("Google Drive");
        }

        if (name.startsWith(QLatin1String("OneDrive")))
        {
            return QLatin1String("OneDrive");
        }

        if ((name == QLatin1String("iCloud Drive")) || (name == QLatin1String("com~apple~CloudDocs")))
        {
            return QLatin1String("iCloud Drive");
        }

        if (!dir.cdUp())
        {
            break;
        }
    }

    return QString();
}

QVariantMap PhotosLibraries::databaseCheck() const
{
    QVariantMap result;

    const KConfigGroup database = KSharedConfig::openConfig()->group(QLatin1String("Database Settings"));
    const KConfigGroup photos   = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"));

    result.insert(QLatin1String("pending"), photos.readEntry(QLatin1String("Move Database To"), QString()));

    if (database.readEntry(QLatin1String("Database Type"), QString()) != QLatin1String("QSQLITE"))
    {
        return result;      // A database server (MariaDB): made for several clients.
    }

    const QString path = cleanPath(database.readEntry(QLatin1String("Database Name"), QString()));
    result.insert(QLatin1String("path"),      QDir::toNativeSeparators(path));
    result.insert(QLatin1String("suggested"), QDir::toNativeSeparators(
                  QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1String("/database")));

    QString problem;

    for (const QString& root : CollectionManager::instance()->allAvailableAlbumRootPaths())
    {
        if (isInside(path, cleanPath(root)))
        {
            problem = QLatin1String("library");
            break;
        }
    }

    const QString tool = syncToolOf(path);

    if (problem.isEmpty() && isNetworkPath(path))
    {
        problem = QLatin1String("network");
    }

    if (problem.isEmpty() && !tool.isEmpty())
    {
        problem = QLatin1String("synced");
    }

    result.insert(QLatin1String("problem"), problem);
    result.insert(QLatin1String("tool"),    tool);

    return result;
}

void PhotosLibraries::requestDatabaseMove(const QString& target)
{
    KConfigGroup photos = KSharedConfig::openConfig()->group(QLatin1String("Photos Mode"));

    if (target.isEmpty())
    {
        photos.deleteEntry(QLatin1String("Move Database To"));
    }
    else
    {
        photos.writeEntry(QLatin1String("Move Database To"), QDir::fromNativeSeparators(target));
    }

    photos.sync();
}

} // namespace Digikam
