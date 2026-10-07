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
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchquerycache.h"

namespace Digikam
{

QString SearchQueryCache::normalizeKey(const QString& query) const
{
    return (query.trimmed().toLower().simplified());
}

bool SearchQueryCache::lookup(const QString& query, SearchQueryIntent* const outIntent) const
{
    const auto it = m_entries.constFind(normalizeKey(query));

    if (it == m_entries.constEnd())
    {
        return false;
    }

    if (outIntent)
    {
        *outIntent = it.value();
    }

    return true;
}

void SearchQueryCache::store(const QString& query, const SearchQueryIntent& intent)
{
    // Only cache fully successful, non-ambiguous interpretations.

    if (!intent.parseSucceeded || intent.requiresClarification)
    {
        return;
    }

    if (m_entries.size() >= s_maxEntries)
    {
        m_entries.erase(m_entries.constBegin());
    }

    m_entries.insert(normalizeKey(query), intent);
}

void SearchQueryCache::clear()
{
    m_entries.clear();
}

void SearchQueryCache::invalidateSchemaVersion(int version)
{
    if (version != m_schemaVersion)
    {
        clear();
        m_schemaVersion = version;
    }
}

int SearchQueryCache::size() const
{
    return m_entries.size();
}

} // namespace Digikam
