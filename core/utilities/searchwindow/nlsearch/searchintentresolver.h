/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Resolves a parsed SearchQueryIntent into concrete
 *               criteria the Advanced Search UI can apply. Decides
 *               what applies directly, what stays unresolved (shown
 *               to the user, never silently invented), and whether
 *               clarification is needed.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Local includes

#include "searchqueryintent.h"
#include "digikam_export.h"

namespace Digikam
{

class SearchCapabilityDictionary;

class ResolvedSearchCriteria
{
public:

    ResolvedSearchCriteria() = default;

public:

    QList<SearchQueryConstraint> resolvedConstraints;
    QStringList                  unresolvedTerms;
    bool                         canApplyDirectly = false;
};

// -----------------------------------------------------------------------------

class DIGIKAM_GUI_EXPORT SearchIntentResolver
{
public:

    explicit SearchIntentResolver(SearchCapabilityDictionary* const dictionary);

public:

    ResolvedSearchCriteria resolve(const SearchQueryIntent& intent)  const;
    bool canApplyDirectly(const SearchQueryIntent& intent)           const;

    /**
     * @brief Dictionary-driven ambiguity: sets clarification fields on the intent if
     * any constraint value is a known ambiguous term the model did not flag.
     */
    void applyDictionaryAmbiguity(SearchQueryIntent* const intent)   const;

private:

    bool resolveConstraint(const SearchQueryConstraint& in,
                           SearchQueryConstraint* const out)         const;

private:

    SearchCapabilityDictionary* m_dictionary = nullptr;
};

} // namespace Digikam
