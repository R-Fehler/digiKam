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

#pragma once

// Qt includes

#include <QWidget>

// Local includes

#include "coredbalbuminfo.h"
#include "searchqueryintent.h"

namespace Digikam
{

class Album;
class SAlbum;
class SearchWindow;

class SearchTabHeader : public QWidget
{
    Q_OBJECT

public:

    explicit SearchTabHeader(QWidget* const parent);
    ~SearchTabHeader()                                                override;

public Q_SLOTS:

    void selectedSearchChanged(Album* album);
    void copySearch(SAlbum* album);
    void editSearch(SAlbum* album);
    void newKeywordSearch();
    void newAdvancedSearch();

Q_SIGNALS:

    void searchShallBeSelected(const QList<Album*>& albums);

private Q_SLOTS:

    void slotKeywordChanged();
    void slotKeywordChangedTimer();
    void slotStoredKeywordChanged();
    void slotEditCurrentSearch();
    void slotSaveSearch();
    void slotEditStoredAdvancedSearch();
    void slotAdvancedSearchEdited(int id, const QString& query);

    void slotNlInterpretClicked();
    void slotNlIntentReady(const SearchQueryIntent& intent);
    void slotNlClarificationRequired(const SearchQueryIntent& intent);
    void slotNlError(const QString& message);
    void slotNlCancelled();
    void slotNlRunNow();
    void slotNlOpenAdvanced();

private:

    void          setCurrentSearch(DatabaseSearch::Type type,
                                   const QString& query,
                                   bool selectCurrentAlbum = true);

    QString       queryFromKeywords(const QString& keywords)    const;
    QString       keywordsFromQuery(const QString& query)       const;
    SearchWindow* searchWindow()                                const;
    void          stopNlSpinner();

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
