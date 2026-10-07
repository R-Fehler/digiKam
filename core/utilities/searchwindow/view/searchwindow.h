/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-20
 * Description : User interface for searches
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QWidget>

// Local includes

#include "searchqueryintent.h"

namespace Digikam
{
    class SearchQueryEngine;
    class SearchXmlWriter;

class SearchWindow : public QWidget
{
    Q_OBJECT

public:

    /**
     * Create a new SearchWindow with an empty advanced search
     */
    explicit SearchWindow(QWidget* const parent);

    ~SearchWindow()                                                   override;

    /**
     * Read the given search into the search widgets.
     * The id will be emitted with the searchEdited signal.
     */
    void readSearch(int id, const QString& query);

    /**
     * Reset the search widget to an empty search.
     * Current id is -1.
     */
    void reset();

    /**
     * Returns the currently produced search string
     */
    QString search()                                            const;

    void applyResolvedIntent(const SearchQueryIntent& intent);
    QString intentToXml(const SearchQueryIntent& intent)        const;
    SearchQueryEngine* queryEngine()                            const;
    void startInterpretation(const QString& text);

Q_SIGNALS:

    /**
     * Signals that the user has finished editing the search.
     * The given query is the same as search().
     */
    void searchEdited(int id, const QString& query);

protected Q_SLOTS:

    void searchOk();
    void searchCancel();
    void searchTryout();

private Q_SLOTS:

    void slotDescribeSearchSubmitted();
    void slotNlIntentReady(const SearchQueryIntent& intent);
    void slotNlClarificationRequired(const SearchQueryIntent& intent);
    void slotNlError(const QString& message);
    void slotNlCancelled();
    void slotNlStatus(const QString& message);
    void slotTranslationFinished();
    void updateNlPanelState();
    void slotDownloadNlModel();

protected:

    void keyPressEvent(QKeyEvent*) override;

private:

    void setupNlSearch();
    void writeConstraintToXml(SearchXmlWriter& writer,
                              const SearchQueryConstraint& c)   const;

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
