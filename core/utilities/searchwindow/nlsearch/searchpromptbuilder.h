/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Builds the constrained prompt sent to the model.
 *               Embeds the JSON schema, the list of supported fields,
 *               and optional collection-aware hints (known tags,
 *               albums, people).
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

class DIGIKAM_GUI_EXPORT SearchPromptBuilder
{
public:

    SearchPromptBuilder();

    /**
     * @brief Build the full prompt for one user query. Known tags/albums/people
     * are truncated internally (s_maxHintItems) to keep the prompt small
     * for tiny-model context windows.
     */
    QString buildPrompt(const QString& userQuery,
                        const QStringList& knownTags   = {},
                        const QStringList& knownAlbums = {},
                        const QStringList& knownPeople = {}) const;

    /// @brief JSON schema description embedded in every prompt.
    QString schemaDescription()                              const;

    /// @brief Static instruction block.
    QString systemPrompt()                                   const;

private:

    void initSystemPrompt();

private:

    QString          m_systemPrompt;

    /**
     * @brief Cap on collection-specific hint items (tag, album, and people
     * names) injected into the prompt. Limiting this keeps the prompt compact
     * so inference stays fast and within the context window, even for large
     * collections with thousands of tags.
     */
    static const int s_maxHintItems = 50;
};

} // namespace Digikam
