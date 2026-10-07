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

#pragma once

// Local includes

#include "digikam_export.h"
#include "dnnmodelbase.h"
#include "dnnmodelmanager.h"

namespace Digikam
{

/**
 * @brief Download-only model wrapper for local GGUF LLMs.
 *
 * The natural-language search feature loads its model with llama.cpp, not
 * OpenCV. This class exists only so the model participates in digiKam's
 * standard model registration and download flow (DNNModelManager +
 * FilesDownloader): it carries the file metadata and verifies the download,
 * but performs no OpenCV loading. SearchLlamaBackend resolves the path via
 * getModelPath() and loads the GGUF itself.
 */
class DIGIKAM_EXPORT DNNModelNaturalLanguage : public DNNModelBase
{
public:

    explicit DNNModelNaturalLanguage(const DNNModelInfoContainer& _info);
    ~DNNModelNaturalLanguage() override = default;

private:

    DNNModelNaturalLanguage() = delete;

    bool loadModel() override;
};

} // namespace Digikam