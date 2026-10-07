/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Cache of successful natural-language interpretations
 *               so repeated queries skip model inference entirely.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QHash>

// Local includes

#include "searchqueryintent.h"
#include "digikam_export.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchQueryCache
{
public:

    SearchQueryCache() = default;

public:

    /**
     * @brief Lookup by normalized query text (case-insensitive, trimmed).
     */
    bool lookup(const QString& query, SearchQueryIntent* const outIntent) const;

    void store(const QString& query, const SearchQueryIntent& intent);
    void clear();

    /**
     * @brief Drop cached entries when the prompt/JSON schema changes.
     */
    void invalidateSchemaVersion(int version);

    int size() const;

private:

    QString normalizeKey(const QString& query) const;

private:

    /**
     * @note In-memory only. Persistence to a small JSON file under
     * the digiKam config dir is planned for later.
     */
    QHash<QString, SearchQueryIntent> m_entries;

    /**
     * @note Current version of the cached-intent schema. invalidateSchemaVersion()
     * clears the cache when a newer version is supplied, so entries written
     * under an older SearchQueryIntent layout are dropped rather than reused.
     */
    int                               m_schemaVersion = 1;

    /**
    * @brief Maximum number of cached query results, evicted least-recently-used.
    * Repeated identical queries return instantly without re-running inference;
    * 256 entries is a negligible memory footprint.
    */
    static const int                  s_maxEntries    = 256;
};

} // namespace Digikam
