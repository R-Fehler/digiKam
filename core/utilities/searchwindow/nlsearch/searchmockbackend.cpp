/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Mock inference backend. Returns canned JSON for known
 *               prompts so the whole pipeline (prompt -> parse ->
 *               resolve -> XML) can be developed and unit-tested
 *               before any real model is wired in.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchmockbackend.h"

// Qt includes

#include <QTimer>

namespace Digikam
{

SearchMockBackend::SearchMockBackend(QObject* const parent)
    : SearchLanguageBackend(parent)
{
    addCannedResponse(QLatin1String("photos from Paris in 2023"),
        QLatin1String(R"({
            "constraints": [
                { "field": "place",     "op": "contains", "value": "Paris" },
                { "field": "daterange", "op": "between",  "value": "2023-01-01..2023-12-31" }
            ],
            "clarification": null
        })")
    );

    addCannedResponse(QLatin1String("sunset photos with red labels"),
        QLatin1String(R"({
            "constraints": [
                { "field": "tag",        "op": "contains", "value": "sunset" },
                { "field": "colorlabel", "op": "eq",       "value": "red" }
            ],
            "clarification": null
        })")
    );

    addCannedResponse(QLatin1String("best photos from last summer"),
        QLatin1String(R"({
            "constraints": [
                { "field": "daterange", "op": "between", "value": "2025-06-01..2025-08-31" }
            ],
            "clarification": {
                "message": "What does 'best' mean for you?",
                "choices": [ "Pick Label: Accepted", "Rating >= 4" ]
            }
        })")
    );
}

bool SearchMockBackend::loadModel(const QString& modelPath)
{
    m_path   = modelPath;
    m_loaded = true;

    Q_EMIT signalModelLoaded(true);

    return true;
}

void SearchMockBackend::unloadModel()
{
    m_loaded = false;
}

bool SearchMockBackend::isModelLoaded() const
{
    return m_loaded;
}

QString SearchMockBackend::modelPath() const
{
    return m_path;
}

QString SearchMockBackend::backendName() const
{
    return QLatin1String("mock");
}

void SearchMockBackend::addCannedResponse(const QString& querySubstring,
                                          const QString& jsonOutput)
{
    m_canned.insert(querySubstring, jsonOutput);
}

void SearchMockBackend::slotRunInference(const QString& prompt)
{
    // Deliver asynchronously to mimic real backend behaviour and keep
    // signal/slot ordering identical to the llama.cpp backend.

    QTimer::singleShot(0, this, [this, prompt]()
        {
            for (auto it = m_canned.constBegin() ; it != m_canned.constEnd() ; ++it)
            {
                if (prompt.contains(it.key(), Qt::CaseInsensitive))
                {
                    Q_EMIT signalRawOutputReady(it.value());

                    return;
                }
            }

            // Unknown query: empty but valid schema (nothing understood).

            Q_EMIT signalRawOutputReady(QLatin1String(R"({ "constraints": [], "clarification": null })"));
        }
    );
}

} // namespace Digikam

#include "moc_searchmockbackend.cpp"
