/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2019-07-06
 * Description : Autodetect Apple Mail binary program
 *
 * SPDX-FileCopyrightText: 2020-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "applemailbinary.h"

// KDE includes

#include <klocalizedstring.h>

namespace Digikam
{

AppleMailBinary::AppleMailBinary(QObject* const)
    : DBinaryIface(
                   QLatin1String("Mail"),
                   QLatin1String("Apple Mail"),
                   QLatin1String("https://en.wikipedia.org/wiki/Apple_Mail"),
                   QLatin1String("SendByMail"),
                   QStringList(),
                   i18n("Apple Mail Client.")
                  )
{
    setup();
}

} // namespace Digikam

#include "moc_applemailbinary.cpp"
