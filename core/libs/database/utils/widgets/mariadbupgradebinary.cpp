/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2023-10-21
 * Description : Autodetects MariaDB upgrade program and version
 *
 * SPDX-FileCopyrightText: 2016-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "mariadbupgradebinary.h"

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "dbengineparameters.h"

namespace Digikam
{

MariaDBUpgradeBinary::MariaDBUpgradeBinary()
    : DBinaryIface(DbEngineParameters::defaultMariaDBUpgradeCmd(),
                   QLatin1String("MariaDB"),
                   QLatin1String("https://mariadb.org/download/"),
                   QString(),
                   QStringList(QLatin1String("--help")),
                   i18n("This binary file is used to upgrade the database to current server version."))
{
    setup();
}

} // namespace Digikam

#include "moc_mariadbupgradebinary.cpp"
