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

void SearchWindow::slotDownloadNlModel()
{

#ifdef HAVE_LLAMACPP

    QPointer<FilesDownloader> floader = new FilesDownloader(this);
    floader->startDownload();
    delete floader;

    if (SearchNlModelManager::isModelAvailable() && d->nlEngine)
    {
        SearchLlamaBackend* const llamaBackend = new SearchLlamaBackend(this);
        llamaBackend->loadModel(SearchNlModelManager::defaultModelPath());
        d->nlBackend                           = llamaBackend;
        d->nlEngine->setBackend(d->nlBackend);

        qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search: llama.cpp backend loaded after download";
    }

    updateNlPanelState();

#endif

}

void SearchWindow::slotDescribeSearchSubmitted()
{
    const QString text = d->describeEdit->text().trimmed();

    if (text.isEmpty())
    {
        return;
    }

    d->nlFromDialog = true;
    startInterpretation(text);
}

void SearchWindow::slotTranslationFinished()
{
    QString normalized;

    if (
        d->nlTranslator &&
        (d->nlTranslator->error() == DOnlineTranslator::NoError)
       )
    {
        const QString translated = d->nlTranslator->translation();

        // Only use the translation if it actually produced something and
        // differs from the source; otherwise fall back to the original.

        if (!translated.isEmpty())
        {
            normalized = translated;
        }
    }

    // normalized may be empty here (translation disabled/failed/no-op),
    // in which case the engine reuses the original pendingNlText.

    d->nlEngine->slotInterpretQuery(d->pendingNlText, normalized);
}

void SearchWindow::slotNlIntentReady(const SearchQueryIntent& intent)
{
    d->nlSpinnerTimer->stop();
    d->nlCancelButton->setEnabled(false);
    d->nlSpinnerLabel->hide();

    d->describeEdit->setEnabled(true);
    d->nlStatusLabel->setAutoFillBackground(false);
    d->nlStatusLabel->setPalette(QPalette());
    d->nlStatusLabel->hide();

    if (d->nlFromDialog)
    {
        applyResolvedIntent(intent);
        d->nlFromDialog = false;

        if (!d->nlCommittedXml.isEmpty())
        {
            d->hasTouchedXml = true;

            Q_EMIT searchEdited(d->currentId, d->nlCommittedXml);
        }

        if (d->pendingOkAfterInterpret)
        {
            d->pendingOkAfterInterpret = false;
            hide();
        }
    }
}

void SearchWindow::slotNlClarificationRequired(const SearchQueryIntent& intent)
{
    d->nlSpinnerTimer->stop();
    d->nlCancelButton->setEnabled(false);
    d->nlSpinnerLabel->hide();
    d->describeEdit->setEnabled(true);

    QMenu menu(this);

    if (!intent.clarificationMessage.isEmpty())
    {
        QAction* const header = menu.addAction(intent.clarificationMessage);
        header->setEnabled(false);
        menu.addSeparator();
    }

    // Build one action per option, stashing its index in the action data.

    for (int i = 0 ; i < intent.clarificationOptions.size() ; ++i)
    {
        QAction* const a = menu.addAction(intent.clarificationOptions.at(i).displayText);
        a->setData(i);
    }

    QAction* const picked = menu.exec(d->describeEdit->mapToGlobal(
                                      QPoint(0, d->describeEdit->height())));

    if (picked && picked->isEnabled() && picked->data().isValid())
    {
        const int idx = picked->data().toInt();

        if ((idx >= 0) && (idx < intent.clarificationOptions.size()))
        {
            const AmbiguityChoice& choice = intent.clarificationOptions.at(idx);

            // Apply the chosen meaning directly - no second inference pass,
            // so the ambiguous word can't re-trigger clarification.

            SearchQueryIntent resolved;
            resolved.parseSucceeded = true;
            resolved.originalQuery  = intent.originalQuery;

            SearchQueryConstraint c;
            c.field = choice.field;
            c.value = choice.value;
            c.op    = (choice.field == QLatin1String("rating")) ? QLatin1String("gte")
                                                                : QLatin1String("eq");
            resolved.constraints << c;

            applyResolvedIntent(resolved);
        }
    }
}

void SearchWindow::slotNlCancelled()
{
    d->nlSpinnerTimer->stop();
    d->nlSpinnerLabel->hide();
    d->nlCancelButton->setEnabled(false);
    d->describeEdit->setEnabled(true);

    // Neutral message, no error highlight.

    d->nlStatusLabel->setAutoFillBackground(false);
    d->nlStatusLabel->setPalette(QPalette());
    d->nlStatusLabel->setText(i18n("Interpretation cancelled."));
    d->nlStatusLabel->show();
}

void SearchWindow::slotNlError(const QString& message)
{
    d->nlSpinnerTimer->stop();
    d->nlCancelButton->setEnabled(false);
    d->nlSpinnerLabel->setPixmap(QPixmap());

    d->describeEdit->setEnabled(true);
    d->nlStatusLabel->setText(message);

    QPalette pal = d->nlStatusLabel->palette();
    pal.setColor(QPalette::Active, QPalette::Window, QColor(220, 140, 140));
    pal.setColor(QPalette::Active, QPalette::WindowText, Qt::black);
    d->nlStatusLabel->setPalette(pal);
    d->nlStatusLabel->setAutoFillBackground(true);

    d->nlStatusLabel->show();
}

void SearchWindow::slotNlStatus(const QString& message)
{
    d->nlStatusLabel->setText(message);
    d->nlStatusLabel->show();
}

} // namespace Digikam
