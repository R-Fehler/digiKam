/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Maintainable mapping layer between natural-language
 *               terms and digiKam Advanced Search capabilities.
 *               Extending search support means editing this table,
 *               not retraining the model.
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

// Local includes

#include "digikam_export.h"

namespace Digikam
{

struct AmbiguityChoice
{
    QString displayText;   ///< Shown in the menu, e.g. "Oriented horizontally".
    QString field;         ///< Canonical field, e.g. "orientation".
    QString value;         ///< Concrete value, e.g. "landscape".
};

class DIGIKAM_GUI_EXPORT SearchCapabilityDictionary
{
public:

    SearchCapabilityDictionary();
    ~SearchCapabilityDictionary();

    Q_DISABLE_COPY(SearchCapabilityDictionary)

public:

    void initializeDefaults();

    /// @brief "color label" -> "colorlabel". Returns false if unknown.
    bool resolveFieldAlias(const QString& alias, QString& canonicalField)                       const;

    /// @brief ("picklabel", "best") -> "accepted". Returns false if unknown.
    bool resolveValue(const QString& field, const QString& rawValue, QString& resolvedValue)    const;

    QStringList supportedFields()                                                               const;

    /// @brief Clarification choices for an ambiguous term ("best" -> Pick Label / Rating).
    QList<AmbiguityChoice> choicesForAmbiguousWord(const QString& word)                         const;

    bool isKnownTag(const QString& tag)                                                         const;
    void setKnownTags(const QStringList& tags);

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
