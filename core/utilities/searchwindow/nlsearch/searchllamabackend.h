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

#include <QThread>

// Local includes

#include "searchllamaworker.h"
#include "searchlanguagebackend.h"
#include "digikam_export.h"
#include "digikam_config.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchLlamaBackend : public SearchLanguageBackend
{
    Q_OBJECT

public:

    explicit SearchLlamaBackend(QObject* const parent = nullptr);
    ~SearchLlamaBackend()                        override;

    Q_DISABLE_COPY(SearchLlamaBackend)

public:

    bool loadModel(const QString& modelPath)     override;
    void unloadModel()                           override;
    bool isModelLoaded()                   const override;
    QString modelPath()                    const override;
    QString backendName()                  const override;

    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);

public Q_SLOTS:

    void slotRunInference(const QString& prompt) override;
    void slotCancel()                            override;

Q_SIGNALS:

    void signalRequestLoad(const QString& path);
    void signalRequestInference(const QString& prompt, int maxTokens, float temperature);

private:

    QThread             m_workerThread;
    SearchLlamaWorker*  m_worker      = nullptr;
    QString             m_modelPath;
    bool                m_modelLoaded = false;

    /**
    * @brief Maximum number of tokens to generate. The model only needs to emit
    * a small JSON object (a handful of search constraints), so 128 gives ample
    * headroom while capping runaway generation and keeping latency low.
    */
    int                 m_maxTokens   = 128;

    /**
     * @brief Sampling temperature for token generation. 0.0 = greedy/deterministic
     * decoding which always pick the most-probable token, which is what we want:
     * the model must emit structured, parseable JSON, so reproducible output
     * matters more than creative variation. Higher values would add randomness.
     */
    float               m_temperature = 0.0F;
};

} // namespace Digikam
