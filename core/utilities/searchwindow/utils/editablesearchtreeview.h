/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-14
 * Description : Basic search tree view with editing functionality
 *
 * SPDX-FileCopyrightText: 2008-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2009-2010 by Johannes Wienke <languitar at semipol dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Local includes

#include "searchtreeview.h"
#include "searchmodificationhelper.h"

namespace Digikam
{

/**
 * @brief This tree view for searches adds basic editing functionality via the context
 * menu. This is in detail deleting and renaming existing searches.
 */
class EditableSearchTreeView: public SearchTreeView
{
    Q_OBJECT

public:

    /**
     * @brief Constructor.
     *
     * @param parent qt parent
     * @param searchModel the model this view should act on
     * @param searchModificationHelper the modification helper object used to
     *                                 perform operations on the displayed
     *                                 searches
     */
    EditableSearchTreeView(QWidget* const parent, SearchModel* const searchModel,
                           SearchModificationHelper* const searchModificationHelper);

    /**
     * @brief Destructor.
     */
    ~EditableSearchTreeView()                                                                     override;

protected:

    /**
     * @brief implemented hook methods for context menus.
     */
    QString contextMenuTitle()                                                              const override;

    /**
     * @brief Adds actions to delete or rename existing searches.
     */
    void addCustomContextMenuActions(ContextMenuHelper& cmh, Album* album)                        override;

    /**
     * @brief Handles deletion and renaming actions.
     */
    void handleCustomContextMenuAction(QAction* action, const AlbumPointer<Album>& album)         override;

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
