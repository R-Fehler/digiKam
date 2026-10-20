/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - a streamlined, consumer oriented
 *               front-end started with "digikam --photos".
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosmode.h"

// C++ includes

#include <cstring>

// Qt includes

#include <QAction>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLocalSocket>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

// KDE includes

#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>

// Local includes

#include "digikam_debug.h"
#include "digikamapp.h"
#include "photoscontainer.h"
#include "photosmetadata.h"
#include "thumbnailsize.h"

namespace Digikam
{

namespace
{

bool                      s_enabled = false;
QPointer<PhotosContainer> s_container;
QStringList               s_startupPaths;

} // namespace

QString PhotosMode::configFileName()
{
    return QLatin1String("digikam-photosrc");
}

void PhotosMode::preInitialize(int argc, char** argv)
{
    for (int i = 1 ; i < argc ; ++i)
    {
        if (argv[i] && (std::strcmp(argv[i], "--photos") == 0))
        {
            s_enabled = true;
            break;
        }
    }

    if (!s_enabled)
    {
        return;
    }

    // Seed our own configuration from the classic one on first start, so that
    // both front-ends open the same collections and databases.
    // Afterwards the two files evolve independently.

    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString ownConfig = configDir + QLatin1Char('/') + configFileName();
    const QString dkConfig  = configDir + QLatin1String("/digikamrc");

    if (!QFile::exists(ownConfig) && QFile::exists(dkConfig))
    {
        QDir().mkpath(configDir);

        if (QFile::copy(dkConfig, ownConfig))
        {
            QFile::setPermissions(ownConfig, QFile::ReadOwner | QFile::WriteOwner);
        }
    }

    // Photos mode defaults, applied once (the user may change them later):
    // watch the library folders so that changes made by other applications
    // show up, and use large thumbnails for crisp centre-cropped tiles.
    // Only once a configuration exists: an empty "Album Settings" group would
    // skip digiKam's first-run assistant.

    if (QFile::exists(ownConfig))
    {
        KConfig config(ownConfig, KConfig::SimpleConfig);
        KConfigGroup photos = config.group(QLatin1String("Photos Mode"));

        if (config.hasGroup(QLatin1String("Album Settings")) &&
            !photos.readEntry(QLatin1String("Defaults Applied"), false))
        {
            KConfigGroup album = config.group(QLatin1String("Album Settings"));
            album.writeEntry(QLatin1String("Album Monitoring"), true);
            album.writeEntry(QLatin1String("Use Large Thumbs"), true);
            photos.writeEntry(QLatin1String("Defaults Applied"), true);
            config.sync();
        }

        // Favorites, albums, captions and people go to XMP sidecars next to
        // the photos: the library stays readable by other applications and
        // syncs between devices with the folders (see photosmetadata.h).

        if (config.hasGroup(QLatin1String("Album Settings")) &&
            !photos.readEntry(QLatin1String("Sidecar Defaults Applied"), false))
        {
            KConfigGroup metadata = config.group(QLatin1String("Metadata Settings"));
            PhotosMetadata::writeSidecarDefaults(metadata);
            photos.writeEntry(QLatin1String("Sidecar Defaults Applied"), true);
            config.sync();
        }
    }

    // A database move asked for in the last session (see PhotosLibraries::
    // databaseCheck()): now, before the database is opened.

    if (QFile::exists(ownConfig))
    {
        moveDatabaseIfRequested(ownConfig, dkConfig);
    }

    KConfig::setMainConfigName(configFileName());
}

void PhotosMode::moveDatabaseIfRequested(const QString& ownConfig, const QString& classicConfig)
{
    KConfig config(ownConfig, KConfig::SimpleConfig);
    KConfigGroup photos   = config.group(QLatin1String("Photos Mode"));
    const QString target  = photos.readEntry(QLatin1String("Move Database To"), QString());

    if (target.isEmpty())
    {
        return;
    }

    photos.deleteEntry(QLatin1String("Move Database To"));
    config.sync();

    KConfigGroup database = config.group(QLatin1String("Database Settings"));

    if (database.readEntry(QLatin1String("Database Type"), QString()) != QLatin1String("QSQLITE"))
    {
        return;
    }

    const char* const keys[] =
    {
        "Database Name",
        "Database Name Thumbnails",
        "Database Name Face",
        "Database Name Similarity"
    };

    const char* const files[] =
    {
        "digikam4.db",
        "thumbnails-digikam.db",
        "recognition.db",
        "similarity.db"
    };

    QDir().mkpath(target);
    const QString targetDir = QDir(target).absolutePath() + QLatin1Char('/');
    bool ok                 = true;
    QStringList moved;

    for (int i = 0 ; ok && (i < 4) ; ++i)
    {
        const QString sourceDir = database.readEntry(keys[i], QString());

        if (sourceDir.isEmpty() || (QDir(sourceDir).absolutePath() == QDir(targetDir).absolutePath()))
        {
            continue;
        }

        // With the journal files of SQLite, if any.

        for (const QString& suffix : { QString(), QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal") })
        {
            const QString from = QDir(sourceDir).filePath(QLatin1String(files[i]) + suffix);
            const QString to   = targetDir + QLatin1String(files[i]) + suffix;

            if (!QFile::exists(from))
            {
                continue;
            }

            if (QFile::exists(to))
            {
                QFile::rename(to, to + QLatin1String(".old"));
            }

            // Rename on the same drive, else copy then remove.

            if (QFile::rename(from, to) || (QFile::copy(from, to) && QFile::remove(from)))
            {
                moved << to;
            }
            else
            {
                qCWarning(DIGIKAM_GENERAL_LOG) << "Photos mode: cannot move the database file" << from << "to" << to;
                ok = false;
                break;
            }
        }
    }

    if (!ok)
    {
        return;
    }

    // Point this configuration, and the classic one when it used the same
    // database, to the new place: both front-ends keep sharing it.

    const QString oldDir = database.readEntry(keys[0], QString());

    auto repoint = [&] (KConfig& cfg)
    {
        KConfigGroup group = cfg.group(QLatin1String("Database Settings"));

        if (QDir(group.readEntry(keys[0], QString())).absolutePath() != QDir(oldDir).absolutePath())
        {
            return;
        }

        for (const char* const key : keys)
        {
            group.writeEntry(key, targetDir);
        }

        cfg.sync();
    };

    repoint(config);

    if (QFile::exists(classicConfig))
    {
        KConfig classic(classicConfig, KConfig::SimpleConfig);
        repoint(classic);
    }

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode: library database moved to" << targetDir << moved;
}

bool PhotosMode::isEnabled()
{
    return s_enabled;
}

void PhotosMode::addCommandLineOptions(QCommandLineParser& parser)
{
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("photos"),
                                        i18n("Start digiKam with the streamlined Photos interface")));
    parser.addPositionalArgument(QLatin1String("path"),
                                 i18n("Photos mode: folder or photo to open (added to the library if needed)"),
                                 QLatin1String("[path...]"));
}

QString PhotosMode::instanceServerName()
{
    // One instance per user and configuration (the same database).

    QString user = qEnvironmentVariable("USER");

    if (user.isEmpty())
    {
        user = qEnvironmentVariable("USERNAME");
    }

    const QByteArray key = (user + QLatin1Char('@') + QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                            QLatin1Char('/') + configFileName()).toUtf8();

    return QLatin1String("digikam-photos-") + QString::number(qHash(key), 16);
}

bool PhotosMode::forwardToRunningInstance(const QCommandLineParser& parser)
{
    if (!s_enabled)
    {
        return false;
    }

    QStringList paths;

    for (const QString& argument : parser.positionalArguments())
    {
        // File managers may pass URLs ("file:///home/...").

        const QString path = argument.startsWith(QLatin1String("file:")) ? QUrl(argument).toLocalFile()
                                                                         : argument;

        if (!path.isEmpty())
        {
            paths << QDir::current().absoluteFilePath(path);
        }
    }

    QLocalSocket socket;
    socket.connectToServer(instanceServerName());

    if (!socket.waitForConnected(500))
    {
        s_startupPaths = paths;

        return false;
    }

    QJsonObject message;
    message.insert(QLatin1String("open"), QJsonArray::fromStringList(paths));

    socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();

    if (socket.state() != QLocalSocket::UnconnectedState)
    {
        socket.waitForDisconnected(1000);
    }

    qCDebug(DIGIKAM_GENERAL_LOG) << "Photos mode already running: handed over" << paths;

    return true;
}

QStringList PhotosMode::takeStartupPaths()
{
    const QStringList paths = s_startupPaths;
    s_startupPaths.clear();

    return paths;
}

QWidget* PhotosMode::createCentralWidget(DigikamApp* const app, ItemIconView* const classicView)
{
    // Square, centre-cropped tiles need thumbnails whose short side covers the
    // tile: use digiKam's large (512 px) thumbnails. Stored in our own config
    // only; stock digiKam reads larger stored thumbnails fine (it scales down).

    ThumbnailSize::setUseLargeThumbs(true);

    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String("Album Settings"));
    ThumbnailSize::saveSettings(group, true);

    s_container = new PhotosContainer(app, classicView);

    return s_container;
}

void PhotosMode::finalizeMainWindow(DigikamApp* const app)
{
    if (!s_container)
    {
        return;
    }

    QAction* const toggle = s_container->toggleAction();

    // Keep the shortcut working while the menu bar is hidden.

    app->addAction(toggle);

    // Offer the way back from the classic interface in its View menu.

    const auto menus = app->menuBar()->findChildren<QMenu*>();

    for (QMenu* const menu : menus)
    {
        if (menu->objectName() == QLatin1String("view"))
        {
            QAction* const first = menu->actions().isEmpty() ? nullptr : menu->actions().constFirst();
            menu->insertAction(first, toggle);
            menu->insertSeparator(first);
            break;
        }
    }

    // Window state restoring may show bars again: apply our chrome afterwards.

    QTimer::singleShot(0, s_container, [] ()
        {
            if (s_container)
            {
                s_container->setPhotosActive(true);
                s_container->openStartupPaths();
            }
        }
    );
}

} // namespace Digikam
