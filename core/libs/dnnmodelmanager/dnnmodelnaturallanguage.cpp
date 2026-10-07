/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-25
 * Description : digiKam DNNModelNaturalLanguage class
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "dnnmodelnaturallanguage.h"

// Local includes

#include "digikam_debug.h"

namespace Digikam
{

DNNModelNaturalLanguage::DNNModelNaturalLanguage(const DNNModelInfoContainer& _info)
    : DNNModelBase(_info)
{
}

bool DNNModelNaturalLanguage::loadModel()
{
    // No OpenCV net for a GGUF model: llama.cpp performs the actual load
    // in SearchLlamaBackend. Here we only confirm the downloaded file is
    // present and the expected size (checkFilename() compares to info.fileSize).

    if (checkFilename())
    {
        return (modelLoaded = true);
    }

    return false;
}

} // namespace Digikam