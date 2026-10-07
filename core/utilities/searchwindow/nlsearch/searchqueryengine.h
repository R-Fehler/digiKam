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
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QStringList>

// Local includes

#include "searchqueryintent.h"
#include "digikam_export.h"

namespace Digikam
{

class SearchPromptBuilder;
class SearchIntentParser;
class SearchCapabilityDictionary;
class SearchIntentResolver;
class SearchLanguageBackend;
class SearchQueryCache;

class DIGIKAM_GUI_EXPORT SearchQueryEngine : public QObject
{
    Q_OBJECT

public:

    explicit SearchQueryEngine(QObject* const parent = nullptr);
    ~SearchQueryEngine() override;

    Q_DISABLE_COPY(SearchQueryEngine)

public:

    /**
     * @brief Dependency injection: keeps the engine unit-testable with the
     * mock backend and lets SearchWindow own component lifetimes.
     */
    void setBackend(SearchLanguageBackend* const backend);
    void setPromptBuilder(SearchPromptBuilder* const builder);
    void setParser(SearchIntentParser* const parser);
    void setResolver(SearchIntentResolver* const resolver);
    void setCache(SearchQueryCache* const cache);

    /**
     * @brief Collection-aware hints injected into prompts.
     */
    void setKnownTags(const QStringList& tags);
    void setKnownAlbums(const QStringList& albums);
    void setKnownPeople(const QStringList& people);

    bool isReady() const;

public Q_SLOTS:

    /**
     * @brief Entry point used by SearchWindow / SearchTabHeader.
     * originalText: what the user typed.
     * normalizedText: post-DOnlineTranslator text, or empty to reuse
     * the original (translation disabled / failed -> fallback).
     */
    void slotInterpretQuery(const QString& originalText,
                        const QString& normalizedText);

    void slotCancel();

Q_SIGNALS:

    /// @brief Resolved, unambiguous: UI can populate Advanced Search fields.
    void signalIntentReady(const SearchQueryIntent& intent);

    /// @brief Ambiguous: UI must show clarification choices, never guess.
    void signalClarificationRequired(const SearchQueryIntent& intent);

    void signalErrorOccurred(const QString& message);
    void signalStatusMessage(const QString& message);
    void signalCancelled();

private Q_SLOTS:

    void slotRawOutputReady(const QString& output);
    void slotBackendError(const QString& error);
    void slotBackendCancelled();

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
