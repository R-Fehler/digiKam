/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - "Open in Photos" in the context menu of
 *               file managers, for the current user.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QString>

namespace Digikam
{

/**
 * "Open in Photos" on folders, photos and videos, in the file manager's
 * context menu. Installed for the current user only, no administrator
 * rights needed, and removed the same way:
 *
 *  - Windows: Explorer verbs under HKEY_CURRENT_USER\Software\Classes
 *    (folders, the background of a folder, images, videos). Windows 11
 *    lists them under "Show more options".
 *  - Linux: a Dolphin service menu, Nemo action, Nautilus and Caja
 *    scripts, and an "Open With" entry (all file managers) in
 *    ~/.local/share.
 *  - macOS: nothing to install. Finder lists digiKam under "Open With"
 *    (document types of the application bundle).
 *
 * The command is "<digikam> --photos <path>": a running digiKam gets the
 * path (see PhotosMode::forwardToRunningInstance()). The path of the
 * program is the AppImage itself when run from one.
 */
namespace PhotosShellIntegration
{

/// Something can be installed on this platform.
bool    isSupported();

/// Installed (by this or another copy of digiKam).
bool    isInstalled();

/// Installs it, or updates the program path. Returns false on failure.
bool    install();

void    uninstall();

/// The program started by the menu entries.
QString programPath();

} // namespace PhotosShellIntegration

} // namespace Digikam
