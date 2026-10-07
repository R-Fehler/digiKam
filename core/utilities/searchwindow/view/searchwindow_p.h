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

#include "searchwindow.h"

// Qt includes

#include <QApplication>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScreen>
#include <QWindow>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QToolButton>
#include <QIcon>
#include <QDesktopServices>
#include <QUrl>
#include <QPointer>
#include <QTimer>

// KDE includes

#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>

// Local includes

#include "digikam_config.h"
#include "digikam_debug.h"
#include "searchview.h"
#include "coredbsearchxml.h"
#include "thememanager.h"
#include "searchqueryengine.h"
#include "searchlanguagebackend.h"
#include "searchmockbackend.h"
#include "searchllamabackend.h"
#include "searchnlmodelmanager.h"
#include "searchpromptbuilder.h"
#include "searchintentparser.h"
#include "searchcapabilitydictionary.h"
#include "searchintentresolver.h"
#include "searchquerycache.h"
#include "donlinetranslator.h"
#include "tagscache.h"
#include "colorlabelwidget.h"
#include "filesdownloader.h"
#include "coredbaccess.h"
#include "coredb.h"
#include "dworkingpixmap.h"

namespace Digikam
{

class Q_DECL_HIDDEN SearchWindow::Private
{
public:

    Private() = default;

public:

    QScrollArea*                scrollArea              = nullptr;
    SearchView*                 searchView              = nullptr;
    SearchViewBottomBar*        bottomBar               = nullptr;
    int                         currentId               = -1;
    bool                        hasTouchedXml           = false;
    QString                     oldXml;

    QLineEdit*                  describeEdit            = nullptr;
    QLabel*                     nlStatusLabel           = nullptr;
    QLabel*                     nlSpinnerLabel          = nullptr;
    DWorkingPixmap*             nlSpinnerPix            = nullptr;
    QTimer*                     nlSpinnerTimer          = nullptr;
    int                         nlSpinnerIndex          = 0;
    QCheckBox*                  translateCheck          = nullptr;
    QToolButton*                nlCancelButton          = nullptr;
    QWidget*  nlReadyPanel                              = nullptr;
    QWidget*  nlDownloadPanel                           = nullptr;

    SearchQueryEngine*          nlEngine                = nullptr;
    SearchLanguageBackend*      nlBackend               = nullptr;
    SearchPromptBuilder         nlPromptBuilder;
    SearchIntentParser          nlParser;
    SearchCapabilityDictionary  nlDictionary;
    SearchIntentResolver*       nlResolver              = nullptr;
    SearchQueryCache            nlCache;
    DOnlineTranslator*          nlTranslator            = nullptr;

    QString                     pendingNlText;
    bool                        nlTranslate             = false;
    bool                        nlFromDialog            = false;
    bool                        pendingOkAfterInterpret = false;

    SearchQueryIntent           nlLastIntent;
    QString                     nlCommittedXml;
};


} // namespace Digikam
