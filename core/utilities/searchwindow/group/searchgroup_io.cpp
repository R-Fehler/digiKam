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

void SearchGroup::read(SearchXmlCachingReader& reader)
{
    reset();

    m_label->setGroupOperator(reader.groupOperator());
    m_label->setDefaultFieldOperator(reader.defaultFieldOperator());

    startReadingGroups(reader);

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isEndElement())
        {
            break;
        }

        // subgroup

        if (reader.isGroupElement())
        {
            readGroup(reader);
        }

        if (reader.isFieldElement())
        {
            QString name                 = reader.fieldName();
            SearchField* field           = nullptr;
            SearchFieldGroup* fieldGroup = nullptr;

            for (SearchFieldGroup* const fg : std::as_const(m_fieldGroups))
            {
                fieldGroup = fg;

                if (fieldGroup)
                {
                    if ((field = fieldGroup->fieldForName(name)))
                    {
                        // cppcheck-suppress useStlAlgorithm
                        break;
                    }
                }
            }

            if (field)
            {
                field->read(reader);
                fieldGroup->markField(field);
                fieldGroup->setFieldsVisible(true);
            }
            else
            {
                qCWarning(DIGIKAM_GENERAL_LOG) << "Unhandled search field in XML with field name" << name;
                reader.readToEndOfElement();
            }
        }
    }

    finishReadingGroups();
}

void SearchGroup::write(SearchXmlWriter& writer)
{
    writer.writeGroup();
    writer.setGroupOperator(m_label->groupOperator());
    writer.setDefaultFieldOperator(m_label->defaultFieldOperator());

    for (SearchFieldGroup* const fieldGroup : std::as_const(m_fieldGroups))
    {
        fieldGroup->write(writer);
    }

    // take care for subgroups

    writeGroups(writer);

    writer.finishGroup();
}

} // namespace Digikam
