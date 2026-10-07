/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-20
 * Description : User interface for searches - choice
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 * SPDX-FileCopyrightText: 2011-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchfields_p.h"

namespace Digikam
{

SearchFieldChoice::SearchFieldChoice(QObject* const parent)
    : SearchField(parent)
{
    m_model   = new ChoiceSearchModel(this);
    m_anyText = i18nc("@option: default kind of search options combined", "Any");
}

void SearchFieldChoice::setupValueWidgets(QGridLayout* layout, int row, int column)
{
    m_comboBox = new ChoiceSearchComboBox;
    m_comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    connect(m_model, SIGNAL(checkStateChanged(QVariant,bool)),
            this, SLOT(checkStateChanged()));

    m_comboBox->setSearchModel(m_model);

    // set object name for style sheet

    m_comboBox->setObjectName(QLatin1String("SearchFieldChoice_ComboBox"));

    // label is created only after setting the model

    m_comboBox->label()->setObjectName(QLatin1String("SearchFieldChoice_ClickLabel"));
    updateComboText();

    layout->addWidget(m_comboBox, row, column, 1, 3);
}

void SearchFieldChoice::setChoice(const QMap<int, QString>& map)
{
    m_type = QMetaType::Int;

    m_model->setChoice(map);

}

void SearchFieldChoice::setChoice(const QStringList& choice)
{
    m_type = QMetaType::QString;

    m_model->setChoice(choice);
}

void SearchFieldChoice::setAnyText(const QString& anyText)
{
    m_anyText = anyText;
}

void SearchFieldChoice::checkStateChanged()
{
    updateComboText();
}

void SearchFieldChoice::updateComboText()
{
    QStringList checkedChoices = m_model->checkedDisplayTexts();

    if      (checkedChoices.isEmpty())
    {
        m_comboBox->setLabelText(m_anyText);
        setValidValueState(false);
    }
    else if (checkedChoices.count() == 1)
    {
        m_comboBox->setLabelText(checkedChoices.first());
        setValidValueState(true);
    }
    else
    {
        m_comboBox->setLabelText(i18nc("@label: search field choice combo-box description",
                                       "Any of: %1", checkedChoices.join(QLatin1String(", "))));
        setValidValueState(true);
    }
}

void SearchFieldChoice::read(SearchXmlCachingReader& reader)
{
    SearchXml::Relation relation = reader.fieldRelation();

    if (relation == SearchXml::OneOf)
    {
        if      (m_type == QMetaType::Int)
        {
            m_model->setChecked<int>(reader.valueToIntList());
        }
        else if (m_type == QMetaType::QString)
        {
            m_model->setChecked<QString>(reader.valueToStringList());
        }
    }
    else
    {
        if      (m_type == QMetaType::Int)
        {
            m_model->setChecked<int>(reader.valueToInt(), relation);
        }
        else if (m_type == QMetaType::QString)
        {
            // The testRelation magic only really makes sense for integers. "Like" is not implemented.
/*
            m_model->setChecked<QString>(reader.value(), relation);
*/
            m_model->setChecked<QString>(reader.value());
        }
    }
}

void SearchFieldChoice::write(SearchXmlWriter& writer)
{
    if      (m_type == QMetaType::Int)
    {
        QList<int> v = m_model->checkedKeys<int>();

        if (!v.isEmpty())
        {
            if (v.size() == 1)
            {
                writer.writeField(m_name, SearchXml::Equal);
                writer.writeValue(v.first());
                writer.finishField();
            }
            else
            {
                writer.writeField(m_name, SearchXml::OneOf);
                writer.writeValue(v);
                writer.finishField();
            }
        }
    }
    else if (m_type == QMetaType::QString)
    {
        QList<QString> v = m_model->checkedKeys<QString>();

        if (!v.isEmpty())
        {
            if (v.size() == 1)
            {
                // For choice string fields, we have the possibility to specify the wildcard
                // position with the position of *.

                if (v.first().contains(QLatin1Char('*')))
                {
                    writer.writeField(m_name, SearchXml::Like);
                }
                else
                {
                    writer.writeField(m_name, SearchXml::Equal);
                }

                writer.writeValue(v.first());
                writer.finishField();
            }
            else
            {
                // OneOf handles wildcards automatically

                writer.writeField(m_name, SearchXml::OneOf);
                writer.writeValue(v);
                writer.finishField();
            }
        }
    }
}

void SearchFieldChoice::reset()
{
    m_model->resetChecked();
}

void SearchFieldChoice::setValueWidgetsVisible(bool visible)
{
    m_comboBox->setVisible(visible);
}

QList<QRect> SearchFieldChoice::valueWidgetRects() const
{
    QList<QRect> rects;
    rects << m_comboBox->geometry();

    return rects;
}

} // namespace Digikam
