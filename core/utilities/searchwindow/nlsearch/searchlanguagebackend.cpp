/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Abstract inference interface for the natural-language
 *               search feature. Hides the actual model runtime
 *               (llama.cpp / ONNX / mock) behind a Qt async API so the
 *               rest of the pipeline stays backend-independent.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchlanguagebackend.h"

namespace Digikam
{

SearchLanguageBackend::SearchLanguageBackend(QObject* const parent)
    : QObject(parent)
{
}

void SearchLanguageBackend::slotCancel()
{
    // Default: no-op. Backends that support cancellation override this.
}

} // namespace Digikam

#include "moc_searchlanguagebackend.cpp"
