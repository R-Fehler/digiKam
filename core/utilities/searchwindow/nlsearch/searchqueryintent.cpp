/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Structured intent extracted from a natural-language
 *               search query. Intermediate representation between the
 *               LLM output and digiKam Advanced Search criteria.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchqueryintent.h"

namespace Digikam
{

bool SearchQueryConstraint::isValid() const
{
    return (!field.isEmpty() && !op.isEmpty());
}

// --------------------------------------------

bool SearchQueryIntent::isAmbiguous() const
{
    return requiresClarification;
}

bool SearchQueryIntent::isEmpty() const
{
    return constraints.isEmpty();
}

} // namespace Digikam
