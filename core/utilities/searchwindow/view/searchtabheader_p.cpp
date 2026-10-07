/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-02-26
 * Description : Upper widget in the search sidebar
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchtabheader_p.h"

namespace Digikam
{

KeywordLineEdit::KeywordLineEdit(QWidget* const parent)
    : QLineEdit(parent)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group        = config->group(QLatin1String("KeywordSearchEdit Settings"));
    m_autoSearch              = group.readEntry(QLatin1String("Autostart Search"), false);
}

void KeywordLineEdit::showAdvancedSearch(bool hasAdvanced)
{
    if (m_hasAdvanced == hasAdvanced)
    {
        return;
    }

    m_hasAdvanced = hasAdvanced;
    adjustStatus(m_hasAdvanced);
}

void KeywordLineEdit::focusInEvent(QFocusEvent* e)
{
    if (m_hasAdvanced)
    {
        adjustStatus(false);
    }

    QLineEdit::focusInEvent(e);
}

void KeywordLineEdit::focusOutEvent(QFocusEvent* e)
{
    QLineEdit::focusOutEvent(e);

    if (m_hasAdvanced)
    {
        adjustStatus(true);
    }
}

void KeywordLineEdit::contextMenuEvent(QContextMenuEvent* e)
{
    QAction* const action = new QAction(i18nc("@action:inmenu",
                                              "Autostart Search"), this);
    action->setCheckable(true);
    action->setChecked(m_autoSearch);

    connect(action, &QAction::triggered,
            this, &KeywordLineEdit::toggleAutoSearch);

    QMenu* const menu = createStandardContextMenu();
    menu->addSeparator();
    menu->addAction(action);
    menu->exec(e->globalPos());
    delete menu;
}

bool KeywordLineEdit::autoSearchEnabled() const
{
    return m_autoSearch;
}

void KeywordLineEdit::adjustStatus(bool adv)
{
    if (adv)
    {
        QPalette p = palette();
        p.setColor(QPalette::Text, p.color(QPalette::Disabled, QPalette::Text));
        setPalette(p);
    }
    else
    {
        setPalette(QPalette());
    }
}

QString KeywordLineEdit::getText() const
{
    return text();
}

void KeywordLineEdit::toggleAutoSearch()
{
    m_autoSearch              = !m_autoSearch;

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup group        = config->group(QLatin1String("KeywordSearchEdit Settings"));
    group.writeEntry(QLatin1String("Autostart Search"), m_autoSearch);
}

} // namespace Digikam
