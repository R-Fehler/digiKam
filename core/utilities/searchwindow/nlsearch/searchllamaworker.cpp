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

#include "searchllamaworker.h"

// C++ includes

#include <cstdint>
#include <vector>

// Qt includes

#include <QThread>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"

#ifdef HAVE_LLAMACPP
#   include <llama.h>
#endif

namespace Digikam
{

#ifdef HAVE_LLAMACPP

namespace
{

/**
 * @brief Route llama.cpp's internal logging through digiKam's NL-search log
 * category instead of letting it print directly to stderr.
 */
void s_llamaLogCallback(ggml_log_level level, const char* text, void* userData)
{
    Q_UNUSED(level);
    Q_UNUSED(userData);

    if (text)
    {
        const QString msg = QString::fromUtf8(text).trimmed();

        if (!msg.isEmpty())
        {
            qCDebug(DIGIKAM_NLSEARCH_LOG).noquote() << msg;
        }
    }
}

/**
 * @brief Abort callback for llama.cpp. Returns true to stop an in-progress
 * decode. The void* carries the worker instance, so a cancellation requested
 * from the GUI interrupts computation mid-decode, not only between tokens.
 */
bool s_llamaAbortCallback(void* userData)       // cppcheck-suppress constParameterCallback
{
    if (userData)
    {
        const SearchLlamaWorker* const worker = static_cast<const SearchLlamaWorker*>(userData);

        return worker->abortRequested();
    }

    return false;
}

/**
 * @brief Model-load progress callback for llama.cpp. Forwards the 0.0-1.0
 * load progress to the worker, which relays it to the GUI. Returns true to
 * continue loading.
 */
bool s_llamaProgressCallback(float progress, void* userData)
{
    if (userData)
    {
        SearchLlamaWorker* const worker = static_cast<SearchLlamaWorker*>(userData);
        worker->reportLoadProgress(progress);
    }

    return true;
}

} // anonymous namespace

#endif // HAVE_LLAMACPP

SearchLlamaWorker::SearchLlamaWorker(QObject* const parent)
    : QObject(parent)
{
}

SearchLlamaWorker::~SearchLlamaWorker()
{
    slotDoUnload();
}

void SearchLlamaWorker::requestCancel()
{
    m_cancelRequested.storeRelaxed(1);
}

void SearchLlamaWorker::slotDoLoad(const QString& modelPath)
{

#ifdef HAVE_LLAMACPP

    // clear any previously loaded model

    slotDoUnload();

    // Initialize the llama.cpp backend

    llama_backend_init();

    // Route llama.cpp's internal logging through digiKam's log category.

    llama_log_set(s_llamaLogCallback, nullptr);

    // Model parameters: CPU-only

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers       = 0;

    // Report model-load progress (0.0-1.0) to the GUI while the ~1 GB model loads.

    mparams.progress_callback           = s_llamaProgressCallback;
    mparams.progress_callback_user_data = this;

    const QByteArray pathUtf8  = modelPath.toUtf8();
    llama_model* const model   = llama_model_load_from_file(pathUtf8.constData(), mparams);

    if (!model)
    {
        qCWarning(DIGIKAM_NLSEARCH_LOG) << "NL search: failed to load model:" << modelPath;

        Q_EMIT signalError(i18n("Failed to load the language model."));
        Q_EMIT signalLoaded(false);

        return;
    }

    // Context window large enough for the prompt and the structured output, and thread counts sized to the machine.

    llama_context_params cparams = llama_context_default_params();

    // Context window: the maximum combined prompt + generated tokens the model
    // holds at once. 4096 comfortably fits the system prompt, the field schema,
    // collection hints, and the user query, with headroom to spare.

    cparams.n_ctx                = 4096;
    cparams.n_threads            = QThread::idealThreadCount();
    cparams.n_threads_batch      = QThread::idealThreadCount();

    llama_context* const ctx     = llama_init_from_model(model, cparams);

    if (!ctx)
    {
        qCWarning(DIGIKAM_NLSEARCH_LOG) << "NL search: failed to create llama context";

        llama_model_free(model);

        Q_EMIT signalError(i18n("Failed to initialize the model context."));
        Q_EMIT signalLoaded(false);

        return;
    }

    m_model   = model;
    m_context = ctx;

    // Allow the GUI to abort a long decode mid-computation (checked by
    // llama.cpp during llama_decode), in addition to the between-token
    // check in slotDoInference.

    llama_set_abort_callback(ctx, s_llamaAbortCallback, this);

    Q_EMIT signalLoaded(true);

#else

    Q_UNUSED(modelPath);
    Q_EMIT signalError(i18n("digiKam was built without llama.cpp support."));
    Q_EMIT signalLoaded(false);

#endif

}

void SearchLlamaWorker::slotDoInference(const QString& prompt, int maxTokens, float temperature)
{
    m_cancelRequested.storeRelaxed(0);

#ifdef HAVE_LLAMACPP

    Q_UNUSED(temperature);  // greedy decoding, determinism preferred for structured output

    if (!m_model || !m_context)
    {
        Q_EMIT signalError(i18n("Model is not loaded."));

        return;
    }

    const llama_model* const model = static_cast<llama_model*>(m_model);
    llama_context*     const ctx   = static_cast<llama_context*>(m_context);
    const llama_vocab* const vocab = llama_model_get_vocab(model);

    llama_memory_clear(llama_get_memory(ctx), true);

    // 1. Tokenize the prompt

    const QByteArray promptUtf8 = prompt.toUtf8();

    const int32_t tokenized     = llama_tokenize(vocab, promptUtf8.constData(),
                                                 promptUtf8.size(), nullptr, 0, true, true);
    const int n_prompt          = (tokenized == INT32_MIN) ? 0 : -tokenized;

    if (n_prompt <= 0)
    {
        Q_EMIT signalError(i18n("Failed to tokenize the prompt."));

        return;
    }

    std::vector<llama_token> tokens(n_prompt);

    if (llama_tokenize(vocab, promptUtf8.constData(), promptUtf8.size(),
                       tokens.data(), tokens.size(), true, true) < 0)
    {
        Q_EMIT signalError(i18n("Failed to tokenize the prompt."));

        return;
    }

    // 2. Greedy sampler (deterministic)

    llama_sampler* const smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    // 3. Decode the prompt, then generate token by token

    QString     result;
    llama_batch batch     = llama_batch_get_one(tokens.data(), tokens.size());
    int         generated = 0;

    while (generated < maxTokens)
    {
        if (m_cancelRequested.loadRelaxed())
        {
            llama_sampler_free(smpl);

            Q_EMIT signalCancelled();

            return;
        }

        if (llama_decode(ctx, batch) != 0)
        {
            llama_sampler_free(smpl);

            if (m_cancelRequested.loadRelaxed())
            {
                Q_EMIT signalCancelled();
            }
            else
            {
                Q_EMIT signalError(i18n("Model decode failed."));
            }

            return;
        }

        const llama_token newToken = llama_sampler_sample(smpl, ctx, -1);

        if (llama_vocab_is_eog(vocab, newToken))
        {
            break;
        }

        char      piece[256] = { 0 };
        const int n          = llama_token_to_piece(vocab, newToken, piece, sizeof(piece), 0, true);

        if (n > 0)
        {
            result += QString::fromUtf8(piece, n);
        }

        Q_EMIT signalProgress(++generated);

        const QString trimmed = result.trimmed();
        int depth             = 0;
        bool sawOpen          = false;
        bool balanced         = false;

        for (const QChar& ch : trimmed)
        {
            if      (ch == QLatin1Char('{'))
            {
                ++depth;
                sawOpen = true;
            }
            else if (ch == QLatin1Char('}'))
            {
                --depth;
            }
        }

        balanced = (sawOpen && depth == 0);

        if (balanced)
        {
            break;
        }

        m_singleToken = newToken;
        batch         = llama_batch_get_one(&m_singleToken, 1);
    }

    llama_sampler_free(smpl);

    Q_EMIT signalOutputReady(result);

#else

    Q_UNUSED(prompt);
    Q_UNUSED(maxTokens);
    Q_UNUSED(temperature);
    Q_EMIT signalError(i18n("digiKam was built without llama.cpp support."));

#endif

}

void SearchLlamaWorker::slotDoUnload()
{

#ifdef HAVE_LLAMACPP

    if (m_context)
    {
        llama_free(static_cast<llama_context*>(m_context));
        m_context = nullptr;
    }

    if (m_model)
    {
        llama_model_free(static_cast<llama_model*>(m_model));
        m_model = nullptr;
    }

#endif

}

bool SearchLlamaWorker::abortRequested() const
{
    return (m_cancelRequested.loadRelaxed() != 0);
}

void SearchLlamaWorker::reportLoadProgress(float progress)
{
    Q_EMIT signalLoadProgress(static_cast<int>(progress * 100.0F));
}

} // namespace Digikam

#include "moc_searchllamaworker.cpp"
