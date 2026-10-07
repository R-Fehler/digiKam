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
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchintentresolver.h"

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "searchcapabilitydictionary.h"

namespace Digikam
{

SearchIntentResolver::SearchIntentResolver(SearchCapabilityDictionary* const dictionary)
    : m_dictionary(dictionary)
{
}

ResolvedSearchCriteria SearchIntentResolver::resolve(const SearchQueryIntent& intent) const
{
    ResolvedSearchCriteria result;

    if (!intent.parseSucceeded || !m_dictionary)
    {
        return result;
    }

    for (const SearchQueryConstraint& in : std::as_const(intent.constraints))
    {
        SearchQueryConstraint out;

        if (resolveConstraint(in, &out))
        {
            result.resolvedConstraints << out;
        }
        else
        {
            // Never silently invent metadata: surface it instead.

            result.unresolvedTerms << QString::fromLatin1("%1: %2").arg(in.field, in.value);
        }
    }

    result.canApplyDirectly = (!result.resolvedConstraints.isEmpty() &&
                               !intent.requiresClarification);

    return result;
}

bool SearchIntentResolver::canApplyDirectly(const SearchQueryIntent& intent) const
{
    return resolve(intent).canApplyDirectly;
}

void SearchIntentResolver::applyDictionaryAmbiguity(SearchQueryIntent* const intent) const
{
    if (!intent || !m_dictionary || intent->requiresClarification)
    {
        return;
    }

    for (const SearchQueryConstraint& c : std::as_const(intent->constraints))
    {
        const QList<AmbiguityChoice> choices = m_dictionary->choicesForAmbiguousWord(c.value);

        if (!choices.isEmpty())
        {
            intent->requiresClarification = true;
            intent->clarificationMessage  = i18nc("@info", "\"%1\" could mean more than one thing. "
                                                           "Which did you mean?", c.value);
            intent->clarificationOptions  = choices;

            // keep the display-only list in sync for any code that still reads it

            intent->clarificationChoices.clear();

            for (const AmbiguityChoice& ch : choices)
            {
                intent->clarificationChoices << ch.displayText;
            }

            return;
        }
    }
}

bool SearchIntentResolver::resolveConstraint(const SearchQueryConstraint& in,
                                             SearchQueryConstraint* const out) const
{
    if (!in.isValid() || !out)
    {
        return false;
    }

    QString canonicalField;

    if (!m_dictionary->resolveFieldAlias(in.field, canonicalField))
    {
        return false;
    }

    QString resolvedValue;

    if (!m_dictionary->resolveValue(canonicalField, in.value, resolvedValue))
    {
        return false;
    }

    out->field = canonicalField;
    out->op    = in.op;
    out->value = resolvedValue;

    return true;
}

} // namespace Digikam
