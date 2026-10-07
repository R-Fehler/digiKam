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

#pragma once

// Qt includes

#include <QLabel>
#include <QString>
#include <QWidget>

namespace Digikam
{

class DClickLabel;

class SearchFieldGroupLabel : public QWidget
{
    Q_OBJECT

public:

    explicit SearchFieldGroupLabel(QWidget* const parent);

public:

    void setTitle(const QString& title);

public Q_SLOTS:

    void displayExpanded();
    void displayFolded();

Q_SIGNALS:

    void clicked();

protected:

    QString      m_title;
    DClickLabel* m_titleLabel   = nullptr;
    QLabel*      m_expandLabel  = nullptr;
};

} // namespace Digikam
