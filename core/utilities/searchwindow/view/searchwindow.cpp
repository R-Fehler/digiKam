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

#include "searchwindow_p.h"

namespace Digikam
{

SearchWindow::SearchWindow(QWidget* const parent)
    : QWidget(parent),
      d      (new Private)
{
    setWindowFlags(Qt::Window);
    QVBoxLayout* const layout   = new QVBoxLayout;

    QWidget* const nlPanel      = new QWidget(this);
    QVBoxLayout* const nlLayout = new QVBoxLayout(nlPanel);

#ifdef HAVE_LLAMACPP

    // Ready state: the describe box, shown when the model is present.

    d->nlReadyPanel               = new QWidget(nlPanel);
    QVBoxLayout* const readyOuter = new QVBoxLayout(d->nlReadyPanel);
    readyOuter->setContentsMargins(0, 0, 0, 0);

    QGroupBox* const nlGroup      = new QGroupBox(i18n("Natural Language Search"), d->nlReadyPanel);
    QVBoxLayout* const readyLay   = new QVBoxLayout(nlGroup);

    d->describeEdit               = new QLineEdit(nlGroup);
    d->describeEdit->setPlaceholderText(i18n("e.g. landscape photos with red labels taken near Paris"));
    d->describeEdit->setClearButtonEnabled(true);

    d->translateCheck             = new QCheckBox(i18n("Translate my query to English first"), nlGroup);
    d->translateCheck->setChecked(d->nlTranslate);
    d->translateCheck->setToolTip(i18n("When enabled, your query is translated to English before "
                                        "being interpreted, so you can type in your own language. "
                                        "Requires an internet connection."));

    d->nlSpinnerLabel             = new QLabel(nlGroup);
    d->nlSpinnerPix               = new DWorkingPixmap(this);
    d->nlSpinnerTimer             = new QTimer(this);

    connect(d->nlSpinnerTimer, &QTimer::timeout, this, [this]()
        {
            if (d->nlSpinnerPix->frameCount() > 0)
            {
                d->nlSpinnerIndex = (d->nlSpinnerIndex + 1) % d->nlSpinnerPix->frameCount();
                d->nlSpinnerLabel->setPixmap(d->nlSpinnerPix->frameAt(d->nlSpinnerIndex));
            }
        }
    );

    d->nlStatusLabel              = new QLabel(nlGroup);
    d->nlStatusLabel->setWordWrap(true);

    d->nlCancelButton             = new QToolButton(nlGroup);
    d->nlCancelButton->setIcon(QIcon::fromTheme(QLatin1String("dialog-cancel")));
    d->nlCancelButton->setToolTip(i18n("Cancel the current interpretation"));
    d->nlCancelButton->setAutoRaise(true);
    d->nlCancelButton->setEnabled(false);

    connect(d->nlCancelButton, &QToolButton::clicked, this, [this]()
        {
            if (d->nlEngine)
            {
                d->nlEngine->slotCancel();
            }
        }
    );

    QToolButton* const nlHelpButton = new QToolButton(nlGroup);
    nlHelpButton->setIcon(QIcon::fromTheme(QLatin1String("help-contents")));
    nlHelpButton->setToolTip(i18n("Open the Natural Language Search handbook"));
    nlHelpButton->setAutoRaise(true);

    connect(nlHelpButton, &QToolButton::clicked, this, []()
        {
            QDesktopServices::openUrl(QUrl(QLatin1String(
                "https://docs.digikam.org/en/left_sidebar/search_view.html")));
        }
    );

    QHBoxLayout* const nlStatusRow = new QHBoxLayout();
    nlStatusRow->setContentsMargins(0, 0, 0, 0);
    nlStatusRow->addWidget(d->translateCheck, 1);
    nlStatusRow->addWidget(d->nlSpinnerLabel);
    nlStatusRow->addWidget(d->nlStatusLabel, 1);
    nlStatusRow->addWidget(d->nlCancelButton);
    nlStatusRow->addWidget(nlHelpButton);

    readyLay->addWidget(d->describeEdit);
    readyLay->addLayout(nlStatusRow);

    readyOuter->addWidget(nlGroup);

    // Download state: shown when llama.cpp is built but the model is missing.

    d->nlDownloadPanel          = new QWidget(nlPanel);
    QVBoxLayout* const dlLay    = new QVBoxLayout(d->nlDownloadPanel);
    dlLay->setContentsMargins(0, 0, 0, 0);

    QLabel* const dlTitle       = new QLabel(i18n("Natural Language Search lets you find images by "
                                                 "describing them in your own words, for example "
                                                 "\"photos with red labels taken near Paris\". It "
                                                 "uses a language model that runs locally on your "
                                                 "computer, with no internet connection required "
                                                 "once downloaded."), d->nlDownloadPanel);
    dlTitle->setWordWrap(true);

    QLabel* const dlInfo        = new QLabel(i18n("The natural-language search model has not been "
                                                 "downloaded yet."), d->nlDownloadPanel);
    dlInfo->setWordWrap(true);

    QPushButton* const dlButton = new QPushButton(i18n("Download Model..."), d->nlDownloadPanel);
    dlButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    dlLay->addWidget(dlTitle);
    dlLay->addWidget(dlInfo);
    dlLay->addWidget(dlButton);

    connect(dlButton, &QPushButton::clicked,
            this, &SearchWindow::slotDownloadNlModel);

    nlLayout->addWidget(d->nlReadyPanel);
    nlLayout->addWidget(d->nlDownloadPanel);

    updateNlPanelState();

#else  // HAVE_LLAMACPP

    Q_UNUSED(nlLayout);
    nlPanel->hide();

#endif // HAVE_LLAMACPP

    layout->addWidget(nlPanel);

    d->scrollArea             = new QScrollArea(this);
    d->scrollArea->setWidgetResizable(true);
    d->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    d->searchView             = new SearchView;
    d->searchView->setup();

    d->bottomBar              = new SearchViewBottomBar(d->searchView);
    d->searchView->setBottomBar(d->bottomBar);

    d->scrollArea->setWidget(d->searchView);
    d->scrollArea->setFrameStyle(QFrame::NoFrame);

    layout->addWidget(d->scrollArea);
    layout->addWidget(d->bottomBar);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    setLayout(layout);

    setVisible(false);
    setWindowTitle(i18nc("@title:window", "Advanced Search"));

    QScreen* screen = qApp->primaryScreen();

    if (QWidget* const widget = qApp->activeWindow())
    {
        if (QWindow* const window = widget->windowHandle())
        {
            screen = window->screen();
        }
    }

    QRect srect = screen->availableGeometry();
    QSize wsize = QSize((1024 <= srect.width())  ? 1024 : srect.width(),
                        (800  <= srect.height()) ?  800 : srect.height());

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group      = config->group(QLatin1String("AdvancedSearch Widget"));

    if (group.exists())
    {
        QSize confSize = group.readEntry(QLatin1String("Widget Size"), wsize);
        resize(confSize);
    }
    else
    {
        resize(wsize);
    }

    connect(d->searchView, SIGNAL(searchOk()),
            this, SLOT(searchOk()));

    connect(d->searchView, SIGNAL(searchCancel()),
            this, SLOT(searchCancel()));

    connect(d->searchView, SIGNAL(searchTryout()),
            this, SLOT(searchTryout()));

    connect(d->translateCheck, &QCheckBox::toggled,
            this, [this](bool on)
        {
            d->nlTranslate          = on;
            KSharedConfigPtr config = KSharedConfig::openConfig();
            KConfigGroup group      = config->group(QLatin1String("NL Search Settings"));
            group.writeEntry(QLatin1String("Translate Queries"), on);
            group.sync();
        }
    );

    setupNlSearch();

    connect(d->describeEdit, &QLineEdit::returnPressed,
            this, &SearchWindow::slotDescribeSearchSubmitted);
}

SearchWindow::~SearchWindow()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group      = config->group(QLatin1String("AdvancedSearch Widget"));
    group.writeEntry(QLatin1String("Widget Size"), size());

    delete d->nlResolver;

    delete d;
}

void SearchWindow::readSearch(int id, const QString& xml)
{
    d->currentId     = id;
    d->hasTouchedXml = false;
    d->oldXml        = xml;
    d->searchView->read(xml);
}

void SearchWindow::reset()
{
    d->currentId     = -1;
    d->hasTouchedXml = false;
    d->oldXml.clear();
    d->nlCommittedXml.clear();
    d->searchView->read(QString());
}

QString SearchWindow::search() const
{
    return d->searchView->write();
}

void SearchWindow::searchOk()
{
    if (!isVisible() )
    {
        return;
    }

    if (d->describeEdit)
    {
        const QString describe = d->describeEdit->text().trimmed();

        if (!describe.isEmpty() && d->nlCommittedXml.isEmpty())
        {
            d->nlFromDialog            = true;
            d->pendingOkAfterInterpret = true;
            startInterpretation(describe);

            return;
        }
    }

    QString xml = (d->nlCommittedXml.isEmpty() ? search() : d->nlCommittedXml);

    if (
        xml.contains(QLatin1String("<group/>")) ||
        xml.contains(QLatin1String("<group></group>"))
       )
    {
        hide();
        return;
    }

    d->hasTouchedXml = true;

    Q_EMIT searchEdited(d->currentId, xml);

    hide();
}

void SearchWindow::searchCancel()
{
    qCDebug(DIGIKAM_GENERAL_LOG) << "SearchWindow: search cancelled";

    // Abort any running natural-language interpretation when the dialog closes.

    if (d->nlEngine)
    {
        d->nlEngine->slotCancel();
    }

    if (d->hasTouchedXml)
    {
        Q_EMIT searchEdited(d->currentId, d->oldXml);

        d->hasTouchedXml = false;
    }

    hide();
}

void SearchWindow::searchTryout()
{
    if (!isVisible())
    {
        return;
    }

    // If the user has typed a natural-language description, interpret that
    // instead of running the (possibly empty) structured form.

    if (d->describeEdit)
    {
        const QString describe = d->describeEdit->text().trimmed();

        if (!describe.isEmpty())
        {
            d->nlFromDialog = true;
            startInterpretation(describe);

            return;
        }
    }

    const QString xml = search();

    if (
        xml.contains(QLatin1String("<group/>")) ||
        xml.contains(QLatin1String("<group></group>"))
       )
    {
        return;   // not to let an empty tryout overwrite committed results
    }

    qCDebug(DIGIKAM_GENERAL_LOG) << "SearchWindow: search tryout";
    d->hasTouchedXml = true;

    Q_EMIT searchEdited(d->currentId, xml);
}

void SearchWindow::keyPressEvent(QKeyEvent* e)
{
    if (
        d->describeEdit && d->describeEdit->hasFocus() &&
        ((e->key() == Qt::Key_Return) || (e->key() == Qt::Key_Enter))
       )
    {
        QWidget::keyPressEvent(e);
        return;
    }

    if (!e->modifiers() || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter)))
    {
        switch (e->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Select:
            {
                searchOk();
                break;
            }

            case Qt::Key_F4:
            case Qt::Key_Escape:
            case Qt::Key_Back:
            {
                searchCancel();
                break;
            }

            default:
            {
                break;
            }
        }
    }
}

} // namespace Digikam

#include "moc_searchwindow.cpp"
