/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2019-04-02
 * Description : plugin to export image as wallpaper.
 *
 * SPDX-FileCopyrightText: 2019      by Igor Antropov <antropovi at yahoo dot com>
 * SPDX-FileCopyrightText: 2019-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "wallpaperplugin.h"

// Qt includes

#include <QPointer>
#include <QEventLoop>
#include <QMessageBox>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "drawdecoder.h"
#include "wallpaperplugindlg.h"

namespace DigikamGenericWallpaperPlugin
{

WallpaperPlugin::WallpaperPlugin(QObject* const parent)
    : DPluginGeneric(parent)
{
}

QString WallpaperPlugin::name() const
{
    return i18n("Export as wallpaper");
}

QString WallpaperPlugin::iid() const
{
    return QLatin1String(DPLUGIN_IID);
}

QIcon WallpaperPlugin::icon() const
{
    return QIcon::fromTheme(QLatin1String("preferences-desktop-wallpaper"));
}

QString WallpaperPlugin::description() const
{
    return i18n("A tool to set image as wallpaper");
}

QString WallpaperPlugin::details() const
{
    return i18n("<p>This tool changes background wallpaper to selected image for all desktops.</p>"
                "<p>If many images are selected, the first one will be used.</p>"
                "<p>If no image is selected, the first one from current album will be used.</p>");
}

QString WallpaperPlugin::handbookSection() const
{
    return QLatin1String("post_processing");
}

QString WallpaperPlugin::handbookChapter() const
{
    return QLatin1String("wall_paper");
}

QList<DPluginAuthor> WallpaperPlugin::authors() const
{
    return QList<DPluginAuthor>()
            << DPluginAuthor(QString::fromUtf8("Igor Antropov"),
                             QString::fromUtf8("antropovi at yahoo dot com"),
                             QString::fromUtf8("(C) 2019"))
            << DPluginAuthor(QString::fromUtf8("Gilles Caulier"),
                             QString::fromUtf8("caulier dot gilles at gmail dot com"),
                             QString::fromUtf8("(C) 2019-2026"),
                             i18n("Author and Maintainer"));
}

void WallpaperPlugin::setup(QObject* const parent)
{
    DPluginAction* const ac = new DPluginAction(parent);
    ac->setIcon(icon());
    ac->setText(i18nc("@action", "Set as wallpaper"));
    ac->setObjectName(QLatin1String("Wallpaper"));
    ac->setActionCategory(DPluginAction::GenericTool);

    connect(ac, SIGNAL(triggered(bool)),
            this, SLOT(slotWallpaper()));

    addAction(ac);
}

void WallpaperPlugin::slotWallpaper()
{
    const DInfoInterface* const iface = infoIface(sender());
    QList<QUrl> images                = iface->currentSelectedItems();

    if (images.isEmpty())
    {
        images = iface->currentAlbumItems();
    }

    if (!images.isEmpty())
    {
        QString wallpaperPath = images[0].toLocalFile();

        // See bug #525230: special case to handle RAW preview to share with the desktop.

        if (DRawDecoder::isRawFile(images[0]))
        {
            QImage image;

            bool ret = DRawDecoder::loadEmbeddedPreview(image, wallpaperPath);

            if (ret)
            {
                wallpaperPath = QString::fromUtf8("%1-wallpaper.jpeg").arg(wallpaperPath);
                ret = image.save(wallpaperPath, "JPG");

                if (!ret)
                {
                    QMessageBox::critical(nullptr, qApp->applicationName(),
                                          i18n("Cannot export RAW image preview as JPEG as WallPaper\n%1",
                                               images[0].toLocalFile()));
                    return;
                }
            }
            else
            {
                    QMessageBox::critical(nullptr, qApp->applicationName(),
                                          i18n("Cannot extract RAW image preview for the WallPaper\n%1",
                                               images[0].toLocalFile()));
                    return;
            }
        }

#ifndef Q_OS_MACOS

        QPointer<WallpaperPluginDlg> dlg = new WallpaperPluginDlg(this);
        dlg->show();

        QEventLoop loop;

        connect(dlg, &QDialog::finished,
                this, [this, &loop, dlg, wallpaperPath](int result)
            {
                if (result == QDialog::Accepted)
                {
                    setWallpaper(wallpaperPath, dlg->wallpaperLayout());
                }

                loop.quit();
                delete dlg;
            }
        );

        loop.exec();

#else

        setWallpaper(wallpaperPath);

#endif

    }
}

} // namespace DigikamGenericWallpaperPlugin

#include "moc_wallpaperplugin.cpp"
