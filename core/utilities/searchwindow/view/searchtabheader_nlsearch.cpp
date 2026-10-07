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

void SearchTabHeader::stopNlSpinner()
{
    d->nlSpinnerTimer->stop();

    d->nlSpinnerLabel->setPixmap(QPixmap());

    d->nlCancelButton->setEnabled(false);
}

void SearchTabHeader::slotNlInterpretClicked()
{
    const QString text = d->keywordEdit->getText().trimmed();

    if (text.isEmpty())
    {
        return;
    }

#ifdef HAVE_LLAMACPP

    // If the model has not been downloaded yet, open the Advanced Search
    // dialog, which presents the download option, instead of failing silently.

    if (!SearchNlModelManager::isModelAvailable())
    {
        SearchWindow* const window = searchWindow();
        window->show();
        window->raise();

        return;
    }

#endif

    // Claim this text so the keyword edit's editingFinished/timer paths
    // treat it as already-handled and don't fire a racing keyword search.

    d->oldKeywordContent = d->keywordEdit->getText();
    d->keywordEditTimer->stop();

    // searchWindow() lazily creates the dialog (and with it the one
    // shared engine  model). Connections are made once, on first use.

    SearchWindow* const window = searchWindow();

    if (!d->nlConnected)
    {
        connect(window->queryEngine(), &SearchQueryEngine::signalIntentReady,
                this, &SearchTabHeader::slotNlIntentReady);

        connect(window->queryEngine(), &SearchQueryEngine::signalClarificationRequired,
                this, &SearchTabHeader::slotNlClarificationRequired);

        connect(window->queryEngine(), &SearchQueryEngine::signalErrorOccurred,
                this, &SearchTabHeader::slotNlError);

        connect(window->queryEngine(), &SearchQueryEngine::signalCancelled,
                this, &SearchTabHeader::slotNlCancelled);

        d->nlConnected = true;
    }

    d->nlInterpretButton->setEnabled(false);
    d->nlCancelButton->setEnabled(true);
    d->nlSpinnerIndex = 0;
    d->nlSpinnerTimer->start(100);
    window->startInterpretation(text);
}

void SearchTabHeader::slotNlIntentReady(const SearchQueryIntent& intent)
{
    d->nlInterpretButton->setEnabled(true);
    stopNlSpinner();
    d->nlPendingIntent = intent;

    d->nlOpenAdvancedButton->setVisible(true);
    d->nlRunNowButton->setVisible(true);

    QStringList lines;

    for (const SearchQueryConstraint& c : intent.constraints)
    {
        lines << QString::fromLatin1("%1: %2").arg(c.field, c.value);
    }

    d->nlInterpretedLabel->setText(i18n("Interpreted as:\n%1", lines.join(QLatin1Char('\n'))));
    d->nlInterpretedPanel->show();
}

void SearchTabHeader::slotNlClarificationRequired(const SearchQueryIntent& intent)
{
    Q_UNUSED(intent);

    d->nlInterpretButton->setEnabled(true);
    stopNlSpinner();

    // Ambiguity needs the full dialog: its clarification UI takes over.
    // The dialog's own describe field re-runs the query there.

    SearchWindow* const window = searchWindow();
    window->show();
    window->raise();
    window->startInterpretation(d->keywordEdit->getText().trimmed());
}

void SearchTabHeader::slotNlError(const QString& message)
{
    d->nlInterpretButton->setEnabled(true);
    stopNlSpinner();
    d->nlInterpretedLabel->setText(message);
    d->nlOpenAdvancedButton->setVisible(false);
    d->nlRunNowButton->setVisible(false);
    d->nlInterpretedPanel->show();
}

void SearchTabHeader::slotNlCancelled()
{
    d->nlInterpretButton->setEnabled(true);
    stopNlSpinner();
    d->nlInterpretedLabel->setText(i18n("Interpretation cancelled."));
    d->nlOpenAdvancedButton->setVisible(false);
    d->nlRunNowButton->setVisible(false);
    d->nlInterpretedPanel->show();
}

void SearchTabHeader::slotNlRunNow()
{
    if (d->nlPendingIntent.isEmpty())
    {
        return;
    }

    const QString xml = searchWindow()->intentToXml(d->nlPendingIntent);
    searchWindow()->hide();
    setCurrentSearch(DatabaseSearch::AdvancedSearch, xml);
    d->nlInterpretedPanel->hide();
}

void SearchTabHeader::slotNlOpenAdvanced()
{
    if (d->nlPendingIntent.isEmpty())
    {
        return;
    }

    SearchWindow* const window = searchWindow();
    window->reset();
    window->applyResolvedIntent(d->nlPendingIntent);
    window->show();
    window->raise();
    d->nlInterpretedPanel->hide();
}

} // namespace Digikam
