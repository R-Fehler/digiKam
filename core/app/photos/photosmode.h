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

#pragma once

// Qt includes

#include <QString>
#include <QStringList>

// Local includes

#include "digikam_export.h"

class QCommandLineParser;
class QWidget;

namespace Digikam
{

class DigikamApp;
class ItemIconView;

/**
 * Entry points used by the stock digiKam startup code to enable Photos mode.
 *
 * Photos mode never changes what digiKam stores: it uses the same databases,
 * the same collections and the same metadata code paths. It only uses its own
 * configuration file (digikam-photosrc) so that window and view settings do not
 * leak into the classic digiKam configuration (digikamrc).
 */
class DIGIKAM_GUI_EXPORT PhotosMode
{
public:

    /**
     * Must be called first in main(), before any KConfig access.
     * Detects "--photos" and switches the main configuration file.
     */
    static void preInitialize(int argc, char** argv);

    static bool isEnabled();

    static void addCommandLineOptions(QCommandLineParser& parser);

    /**
     * "digikam --photos <folder or photo>...", like "code <folder>": when
     * Photos mode already runs for this user, hands the paths over to it
     * (the window comes to the front) and returns true: this process then
     * exits before opening the database. Otherwise keeps the paths for the
     * window about to open and returns false.
     */
    static bool forwardToRunningInstance(const QCommandLineParser& parser);

    /// Paths given on the command line of this process (taken once).
    static QStringList takeStartupPaths();

    /// Name of the local socket of the running instance.
    static QString instanceServerName();

    /**
     * Wraps the classic view in a container showing the Photos UI by default.
     */
    static QWidget* createCentralWidget(DigikamApp* const app, ItemIconView* const classicView);

    /**
     * Called once the main window GUI has been created (menus, tool bars...).
     */
    static void finalizeMainWindow(DigikamApp* const app);

    static QString configFileName();

private:

    PhotosMode()  = delete;
    ~PhotosMode() = delete;
};

} // namespace Digikam
