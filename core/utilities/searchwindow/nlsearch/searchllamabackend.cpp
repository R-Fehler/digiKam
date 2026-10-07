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
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchllamabackend.h"

// C++ includes

#include <cstdint>
#include <vector>

// Qt includes

#include <QFileInfo>
#include <QElapsedTimer>

// Local includes

#include "digikam_debug.h"

namespace Digikam
{

SearchLlamaBackend::SearchLlamaBackend(QObject* const parent)
    : SearchLanguageBackend(parent)
{
    m_worker = new SearchLlamaWorker;
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    connect(this, &SearchLlamaBackend::signalRequestLoad,
            m_worker, &SearchLlamaWorker::slotDoLoad);

    connect(this, &SearchLlamaBackend::signalRequestInference,
            m_worker, &SearchLlamaWorker::slotDoInference);

    connect(m_worker, &SearchLlamaWorker::signalLoaded,
            this, [this](bool ok)
        {
            m_modelLoaded = ok;

            Q_EMIT signalModelLoaded(ok);
        }
    );

    connect(m_worker, &SearchLlamaWorker::signalOutputReady,
            this, &SearchLlamaBackend::signalRawOutputReady);

    connect(m_worker, &SearchLlamaWorker::signalError,
            this, &SearchLlamaBackend::signalInferenceError);

    connect(m_worker, &SearchLlamaWorker::signalProgress,
            this, &SearchLlamaBackend::signalInferenceProgress);

    connect(m_worker, &SearchLlamaWorker::signalLoadProgress,
            this, &SearchLlamaBackend::signalModelLoadProgress);

    connect(m_worker, &SearchLlamaWorker::signalCancelled,
            this, &SearchLlamaBackend::signalInferenceCancelled);

    m_workerThread.start();
}

SearchLlamaBackend::~SearchLlamaBackend()
{
    m_worker->requestCancel();
    m_workerThread.quit();
    m_workerThread.wait();
}

bool SearchLlamaBackend::loadModel(const QString& modelPath)
{
    if (!QFileInfo::exists(modelPath))
    {
        qCWarning(DIGIKAM_NLSEARCH_LOG) << "NL search: model file not found:" << modelPath;

        return false;
    }

    m_modelPath = modelPath;

    Q_EMIT signalRequestLoad(modelPath);

    return true;
}

void SearchLlamaBackend::unloadModel()
{
    QMetaObject::invokeMethod(m_worker, "slotDoUnload", Qt::QueuedConnection);
    m_modelLoaded = false;
}

bool SearchLlamaBackend::isModelLoaded() const
{
    return m_modelLoaded;
}

QString SearchLlamaBackend::modelPath() const
{
    return m_modelPath;
}

QString SearchLlamaBackend::backendName() const
{
    return QLatin1String("llama.cpp");
}

void SearchLlamaBackend::setMaxTokens(int maxTokens)
{
    m_maxTokens = maxTokens;
}

void SearchLlamaBackend::setTemperature(float temperature)
{
    m_temperature = temperature;
}

void SearchLlamaBackend::slotRunInference(const QString& prompt)
{
    if (!m_modelLoaded)
    {
        Q_EMIT signalInferenceError(QLatin1String("Model is not loaded."));

        return;
    }

    Q_EMIT signalRequestInference(prompt, m_maxTokens, m_temperature);
}

void SearchLlamaBackend::slotCancel()
{
    m_worker->requestCancel();
}

} // namespace Digikam

#include "moc_searchllamabackend.cpp"
