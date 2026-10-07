/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : llama.cpp-based inference backend (GGUF models).
 *               First concrete backend.
 *               TinyLlama 1.1B Q4 as baseline, Qwen2.5-1.5B-Instruct
 *               as primary candidate. Inference runs on a worker
 *               thread; results are delivered via queued signals.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QAtomicInteger>

// Local includes

#include "digikam_config.h"

namespace Digikam
{

/**
 * @brief Worker object living on the worker thread. Owns the llama.cpp model
 * and context. All llama_* calls happen here, never on the GUI thread.
 */
class SearchLlamaWorker : public QObject
{
    Q_OBJECT

public:

    explicit SearchLlamaWorker(QObject* const parent = nullptr);
    ~SearchLlamaWorker() override;

    Q_DISABLE_COPY(SearchLlamaWorker)

public:

    void requestCancel();

   /**
    * @brief Thread-safe check used by the llama.cpp abort callback to stop
    * an in-progress decode when cancellation has been requested.
    */
    bool abortRequested() const;

    /**
     * @brief Relay model-load progress (0.0-1.0) from the llama.cpp progress
     * callback to the GUI as a percentage.
     */
    void reportLoadProgress(float progress);

public Q_SLOTS:

    void slotDoLoad(const QString& modelPath);
    void slotDoInference(const QString& prompt, int maxTokens, float temperature);
    void slotDoUnload();

Q_SIGNALS:

    void signalLoaded(bool success);
    void signalOutputReady(const QString& output);
    void signalError(const QString& message);
    void signalProgress(int step);
    void signalLoadProgress(int percent);
    void signalCancelled();

private:

#ifdef HAVE_LLAMACPP

    void*                  m_model       = nullptr;
    void*                  m_context     = nullptr;
    int32_t                m_singleToken = 0;

#endif

    QAtomicInteger<int>    m_cancelRequested;
};

} // namespace Digikam
