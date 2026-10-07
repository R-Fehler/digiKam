/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Parses and validates raw model output into a
 *               SearchQueryIntent. Rejects anything that is not a
 *               well-formed JSON object matching the schema. The
 *               model output is NEVER trusted directly.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QByteArray>

// Local includes

#include "searchqueryintent.h"
#include "digikam_export.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchIntentParser
{
public:

    SearchIntentParser() = default;

public:

    /**
     * @brief Parse raw model output. Tolerates leading/trailing junk by
     * extracting the first balanced top-level JSON object, then
     * validates fields and operators against the supported sets.
     * On failure returns an intent with parseSucceeded == false.
     */
    SearchQueryIntent parse(const QByteArray& rawJson,
                            const QString& originalQuery,
                            const QString& normalizedQuery)        const;

    bool validate(const SearchQueryIntent& intent)                 const;
    QStringList validationErrors(const SearchQueryIntent& intent)  const;

private:

    bool validateField(const QString& field)                       const;
    bool validateOperator(const QString& field, const QString& op) const;

    QByteArray extractJsonObject(const QByteArray& raw)            const;
};

} // namespace Digikam
