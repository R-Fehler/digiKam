/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-20
 * Description : User interface for searches
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 * SPDX-FileCopyrightText: 2012-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchfieldgrouplabel.h"

// Qt includes

#include <QGridLayout>
#include <QFrame>

// Local includes

#include "dexpanderbox.h"

namespace Digikam
{

SearchFieldGroupLabel::SearchFieldGroupLabel(QWidget* const parent)
    : QWidget(parent)
{
    QGridLayout* const layout = new QGridLayout;

    m_titleLabel              = new DClickLabel;
    m_titleLabel->setObjectName(QLatin1String("SearchFieldGroupLabel_Label"));
    m_expandLabel             = new QLabel;
    QFrame* const hline       = new QFrame;
    hline->setFrameStyle(QFrame::HLine | QFrame::Raised);

    layout->addWidget(m_titleLabel,  0, 0);
    layout->addWidget(m_expandLabel, 0, 1);
    layout->addWidget(hline,         1, 0, 1, 3);
    layout->setColumnStretch(2, 1);
    layout->setSpacing(2);
    setLayout(layout);

    connect(m_titleLabel, SIGNAL(activated()),
            this, SIGNAL(clicked()));
}

void SearchFieldGroupLabel::setTitle(const QString& title)
{
    m_title = title;
    m_titleLabel->setText(title);
}

void SearchFieldGroupLabel::displayExpanded()
{
}

void SearchFieldGroupLabel::displayFolded()
{
}

} // namespace Digikam

#include "moc_searchfieldgrouplabel.cpp"
