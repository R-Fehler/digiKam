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

#pragma once

// Local includes

#include "dbinaryiface.h"

namespace Digikam
{

class AppleMailBinary : public DBinaryIface
{
    Q_OBJECT

public:

    explicit AppleMailBinary(QObject* const parent = nullptr);
    ~AppleMailBinary() override = default;
};

} // namespace Digikam
