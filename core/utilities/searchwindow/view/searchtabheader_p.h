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

#include "searchtabheader.h"

// Qt includes

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QApplication>
#include <QStyle>
#include <QLineEdit>
#include <QInputDialog>
#include <QIcon>
#include <QMenu>
#include <QContextMenuEvent>

// KDE includes

#include <klocalizedstring.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

// Local includes

#include "digikam_config.h"
#include "digikam_debug.h"
#include "digikam_globals.h"
#include "album.h"
#include "albummanager.h"
#include "searchnlmodelmanager.h"
#include "searchfolderview.h"
#include "searchwindow.h"
#include "coredbsearchxml.h"
#include "dexpanderbox.h"
#include "searchqueryengine.h"
#include "dworkingpixmap.h"
#include "digikamapp.h"

namespace Digikam
{

class Q_DECL_HIDDEN KeywordLineEdit : public QLineEdit
{
    Q_OBJECT

public:

    explicit KeywordLineEdit(QWidget* const parent = nullptr);

    void showAdvancedSearch(bool hasAdvanced);
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    bool autoSearchEnabled() const;
    void adjustStatus(bool adv);
    QString getText() const;

public Q_SLOTS:

    void toggleAutoSearch();

protected:

    bool    m_hasAdvanced   = false;
    bool    m_autoSearch    = false;
};

// -------------------------------------------------------------------------

class Q_DECL_HIDDEN SearchTabHeader::Private
{
public:

    Private() = default;

public:

    QGroupBox*          newSearchWidget         = nullptr;
    QGroupBox*          saveAsWidget            = nullptr;
    QGroupBox*          editSimpleWidget        = nullptr;
    QGroupBox*          editAdvancedWidget      = nullptr;

    QStackedLayout*     lowerArea               = nullptr;

    KeywordLineEdit*    keywordEdit             = nullptr;
    QPushButton*        advancedNewSearch       = nullptr;
    QPushButton*        advancedEditSearch      = nullptr;

    QLineEdit*          saveNameEdit            = nullptr;
    QToolButton*        saveButton              = nullptr;

    DAdjustableLabel*   storedKeywordEditName   = nullptr;
    QLineEdit*          storedKeywordEdit       = nullptr;
    DAdjustableLabel*   storedAdvancedEditName  = nullptr;
    QPushButton*        storedAdvancedEditLabel = nullptr;

    QTimer*             keywordEditTimer        = nullptr;
    QTimer*             storedKeywordEditTimer  = nullptr;

    SearchWindow*       searchWindow            = nullptr;

    SAlbum*             currentAlbum            = nullptr;

    QString             oldKeywordContent;
    QString             oldStoredKeywordContent;

    QToolButton*        nlInterpretButton       = nullptr;
    QToolButton*        nlCancelButton          = nullptr;
    QLabel*             nlSpinnerLabel          = nullptr;
    DWorkingPixmap*     nlSpinnerPix            = nullptr;
    QTimer*             nlSpinnerTimer          = nullptr;
    int                 nlSpinnerIndex          = 0;
    QGroupBox*          nlInterpretedPanel      = nullptr;
    QLabel*             nlInterpretedLabel      = nullptr;
    QPushButton*        nlOpenAdvancedButton    = nullptr;
    QPushButton*        nlRunNowButton          = nullptr;

    SearchQueryIntent   nlPendingIntent;
    bool                nlConnected             = false;
};

} // namespace Digikam
