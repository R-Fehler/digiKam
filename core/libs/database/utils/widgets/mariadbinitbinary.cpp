/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2016-06-20
 * Description : Autodetects MariaDB initializer binary program and version
 *
 * SPDX-FileCopyrightText: 2016-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "mariadbinitbinary.h"

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "dbengineparameters.h"

namespace Digikam
{

MariaDBInitBinary::MariaDBInitBinary()
    : DBinaryIface(DbEngineParameters::defaultMariaDBInitCmd(),
                   QLatin1String("MariaDB"),
                   QLatin1String("https://mariadb.org/download/"),
                   QString(),
                   QStringList(QLatin1String("--help")),
                   i18n("This binary file is used to initialize the MariaDB data files for the database."))
{
    setup();
}

} // namespace Digikam

#include "moc_mariadbinitbinary.cpp"
