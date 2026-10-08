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
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

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

    KConfig::setMainConfigName(configFileName());
}

bool PhotosMode::isEnabled()
{
    return s_enabled;
}

void PhotosMode::addCommandLineOptions(QCommandLineParser& parser)
{
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("photos"),
                                        i18n("Start digiKam with the streamlined Photos interface")));
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
            }
        }
    );
}

} // namespace Digikam
