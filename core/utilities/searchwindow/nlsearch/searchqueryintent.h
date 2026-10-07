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

#pragma once

// Qt includes

#include <QString>
#include <QStringList>
#include <QList>
#include <QMetaType>

// Local includes

#include "searchcapabilitydictionary.h"

namespace Digikam
{

/**
 * @brief A single search constraint extracted from the user's query.
 * @example { field = "tag", op = "contains", value = "landscape" }
 *
 * The set of valid fields/operators is defined and validated by
 * SearchIntentParser and SearchCapabilityDictionary, never trusted
 * directly from the model output.
 */
class SearchQueryConstraint
{
public:

    SearchQueryConstraint() = default;

public:

    bool isValid()      const;

public:

    QString field;   ///< Canonical field name (e.g. "tag", "picklabel", "daterange", "place")
    QString op;      ///< Operator (e.g. "eq", "contains", "gte", "between")
    QString value;   ///< Raw or resolved value (e.g. "landscape", "2025-06-01..2025-08-31")
};

/**
 * @brief The full structured intent for one natural-language query.
 * Produced by SearchIntentParser from raw model output, consumed by
 * SearchIntentResolver, and (after resolution) applied to the
 * Advanced Search UI via SearchWindow programmatic setters.
 */
class SearchQueryIntent
{
public:

    SearchQueryIntent() = default;

public:

    bool isAmbiguous()  const;
    bool isEmpty()      const;

public:

    QList<SearchQueryConstraint> constraints;

    /**
     * @note Populated when the model flags an ambiguous term
     */
    QString                      clarificationMessage;
    QStringList                  clarificationChoices;
    QList<AmbiguityChoice>       clarificationOptions;  ///< Each carries field+value to apply on pick.

    /**
     * @note Book-keeping for transparency / SAlbum storage.
     */
    QString                      originalQuery;         ///< Exactly what the user typed.
    QString                      normalizedQuery;       ///< After optional DOnlineTranslator pass.
    QString                      rawModelOutput;        ///< Raw JSON from the backend (debugging).

    bool                         requiresClarification = false;
    bool                         parseSucceeded        = false;
};

} // namespace Digikam

Q_DECLARE_METATYPE(Digikam::SearchQueryConstraint)
Q_DECLARE_METATYPE(Digikam::SearchQueryIntent)
