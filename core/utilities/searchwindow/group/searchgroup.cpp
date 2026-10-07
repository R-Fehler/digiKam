/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-20
 * Description : User interface for searches
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 * SPDX-FileCopyrightText: 2011-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchgroup_p.h"

namespace Digikam
{

SearchGroup::SearchGroup(SearchView* const parent)
    : AbstractSearchGroupContainer(parent),
      m_view                      (parent)
{
}

SearchGroup::Type SearchGroup::groupType() const
{
    return m_groupType;
}

QList<QRect> SearchGroup::startupAnimationArea() const
{
    QList<QRect> rects;

    // from subgroups;

    rects += startupAnimationAreaOfGroups();

    // field groups

    for (const SearchFieldGroup* const fieldGroup : std::as_const(m_fieldGroups))
    {
        // cppcheck-suppress useStlAlgorithm
        rects += fieldGroup->areaOfMarkedFields();
    }

    // adjust position relative to parent

    for (QList<QRect>::iterator it = rects.begin() ; it != rects.end() ; ++it)
    {
        (*it).translate(pos());
    }

    return rects;
}

} // namespace Digikam

#include "moc_searchgroup.cpp"
