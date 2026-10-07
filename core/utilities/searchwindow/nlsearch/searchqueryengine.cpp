/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Central coordinator of the NL search subsystem.
 *               Flow: query -> cache check -> prompt -> backend ->
 *               parse -> resolve -> signalIntentReady() or
 *               signalClarificationRequired() back to the UI.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchqueryengine.h"

// Qt includes

#include <QTimer>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "searchlanguagebackend.h"
#include "searchpromptbuilder.h"
#include "searchintentparser.h"
#include "searchintentresolver.h"
#include "searchquerycache.h"
#include "digikam_debug.h"
#include "searchnlmodelmanager.h"

namespace Digikam
{

class Q_DECL_HIDDEN SearchQueryEngine::Private
{
public:

    Private() = default;

public:

    SearchLanguageBackend*  backend       = nullptr;
    SearchPromptBuilder*    promptBuilder = nullptr;
    SearchIntentParser*     parser        = nullptr;
    SearchIntentResolver*   resolver      = nullptr;
    SearchQueryCache*       cache         = nullptr;

    QStringList             knownTags;
    QStringList             knownAlbums;
    QStringList             knownPeople;

    QString                 originalQuery;
    QString                 normalizedQuery;
    bool                    busy          = false;
};

SearchQueryEngine::SearchQueryEngine(QObject* const parent)
    : QObject(parent),
      d      (new Private)
{
    qRegisterMetaType<SearchQueryConstraint>("SearchQueryConstraint");
    qRegisterMetaType<SearchQueryIntent>("SearchQueryIntent");
}

SearchQueryEngine::~SearchQueryEngine()
{
    delete d;
}

void SearchQueryEngine::setBackend(SearchLanguageBackend* const backend)
{
    if (d->backend)
    {
        disconnect(d->backend, nullptr, this, nullptr);
    }

    d->backend = backend;

    if (d->backend)
    {
        connect(d->backend, &SearchLanguageBackend::signalRawOutputReady,
                this, &SearchQueryEngine::slotRawOutputReady);

        connect(d->backend, &SearchLanguageBackend::signalInferenceError,
                this, &SearchQueryEngine::slotBackendError);

        connect(d->backend, &SearchLanguageBackend::signalInferenceCancelled,
                this, &SearchQueryEngine::slotBackendCancelled);
    }
}

void SearchQueryEngine::setPromptBuilder(SearchPromptBuilder* const builder)
{
    d->promptBuilder = builder;
}

void SearchQueryEngine::setParser(SearchIntentParser* const parser)
{
    d->parser = parser;
}

void SearchQueryEngine::setResolver(SearchIntentResolver* const resolver)
{
    d->resolver = resolver;
}

void SearchQueryEngine::setCache(SearchQueryCache* const cache)
{
    d->cache = cache;
}

void SearchQueryEngine::setKnownTags(const QStringList& tags)
{
    d->knownTags = tags;
}

void SearchQueryEngine::setKnownAlbums(const QStringList& albums)
{
    d->knownAlbums = albums;
}

void SearchQueryEngine::setKnownPeople(const QStringList& people)
{
    d->knownPeople = people;
}

bool SearchQueryEngine::isReady() const
{
    return (
            d->backend                  &&
            d->backend->isModelLoaded() &&
            d->promptBuilder            &&
            d->parser                   &&
            d->resolver
           );
}

void SearchQueryEngine::slotInterpretQuery(const QString& originalText,
                                       const QString& normalizedText)
{
    if (d->busy)
    {
        Q_EMIT signalStatusMessage(i18nc("@info", "A query is already being interpreted."));

        return;
    }

    d->originalQuery   = originalText;
    d->normalizedQuery = (normalizedText.isEmpty() ? originalText
                                                   : normalizedText);

    if (d->cache)
    {
        SearchQueryIntent cached;

        if (d->cache->lookup(d->normalizedQuery, &cached))
        {
            QTimer::singleShot(0, this, [this, cached = std::move(cached)]()
                {
                    Q_EMIT signalIntentReady(cached);
                }
            );

            return;
        }
    }

    if (!isReady())
    {
        if (!SearchNlModelManager::isModelAvailable())
        {
            Q_EMIT signalErrorOccurred(i18nc("@info",
                "Natural language search needs a language model that is not "
                "installed yet. Enable \"Natural Language Search\" in the model "
                "download dialog to install it, then restart digiKam."));
        }
        else
        {
            Q_EMIT signalErrorOccurred(i18nc("@info",
                "The natural language search model is still loading. "
                "Please try your search again in a few seconds."));
        }

        return;
    }

    const QString prompt = d->promptBuilder->buildPrompt(d->normalizedQuery,
                                                         d->knownTags,
                                                         d->knownAlbums,
                                                         d->knownPeople);

    d->busy = true;

    Q_EMIT signalStatusMessage(i18nc("@info", "Interpreting your search..."));

    d->backend->slotRunInference(prompt);
}

void SearchQueryEngine::slotCancel()
{
    if (d->backend)
    {
        d->backend->slotCancel();
    }

    d->busy = false;
}

void SearchQueryEngine::slotRawOutputReady(const QString& output)
{
    d->busy = false;

    SearchQueryIntent intent = d->parser->parse(output.toUtf8(),
                                                d->originalQuery,
                                                d->normalizedQuery);

    if (!intent.parseSucceeded)
    {
        Q_EMIT signalErrorOccurred(i18nc("@info", "Could not interpret the model output. "
                                          "Please refine the description or create rules manually."));

        return;
    }


    const ResolvedSearchCriteria criteria = d->resolver->resolve(intent);

    d->resolver->applyDictionaryAmbiguity(&intent);

    if (intent.requiresClarification)
    {
        Q_EMIT signalClarificationRequired(intent);

        return;
    }

    if (criteria.resolvedConstraints.isEmpty())
    {
        // Nothing understood: explicit message, leave user in normal

        Q_EMIT signalErrorOccurred(i18nc("@info", "Could not map '%1' to tags, album names, captions, "
                                         "location, or date. Please refine the description "
                                         "or create rules manually.", d->originalQuery));

        return;
    }

    intent.constraints = criteria.resolvedConstraints;

    if (!criteria.unresolvedTerms.isEmpty())
    {
        Q_EMIT signalStatusMessage(i18nc("@info", "Some terms could not be mapped and were skipped: %1",
                                   criteria.unresolvedTerms.join(QLatin1String(", "))));
    }

    if (d->cache)
    {
        d->cache->store(d->normalizedQuery, intent);
    }

    Q_EMIT signalIntentReady(intent);
}

void SearchQueryEngine::slotBackendCancelled()
{
    Q_EMIT signalCancelled();
}

void SearchQueryEngine::slotBackendError(const QString& error)
{
    d->busy = false;

    Q_EMIT signalErrorOccurred(error);
}

} // namespace Digikam

#include "moc_searchqueryengine.cpp"
