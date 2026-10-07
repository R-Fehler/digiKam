/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Lifecycle management of the NL search model file.
 *               Thin bridge to digiKam's existing infrastructure:
 *               DNNModelManager (core/libs/dnnmodelmanager) for
 *               registration/path lookup and FilesDownloader
 *               (core/utilities/setup/downloader) for KDE-hosted
 *               on-demand retrieval. No parallel download mechanism.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchnlmodelmanager.h"

// Qt includes

#include <QFileInfo>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "dnnmodelmanager.h"
#include "dnnmodelbase.h"
#include "dnnmodeldefinitions.h"

namespace Digikam
{

namespace
{
    // @note DNNModelManager::getModel() lower-cases the lookup internally.

    const QLatin1String s_nlModelName("Qwen2.5-1.5B-Instruct");

} // namespace

SearchNlModelManager::SearchNlModelManager(QObject* const parent)
    : QObject(parent)
{
}

SearchNlModelManager::~SearchNlModelManager() = default;

QString SearchNlModelManager::defaultModelPath()
{
    const DNNModelBase* const model = DNNModelManager::instance()->getModel(
                                                                            s_nlModelName,
                                                                            DNNModelUsage::DNNUsageNaturalLanguageSearch
                                                                           );

    return (model ? model->getModelPath() : QString());
}

bool SearchNlModelManager::isModelAvailable()
{
    const QString path = defaultModelPath();

    return (!path.isEmpty() && QFileInfo::exists(path));
}

QString SearchNlModelManager::currentModelPath() const
{
    return (m_currentModelPath.isEmpty() ? defaultModelPath()
                                         : m_currentModelPath);
}

QString SearchNlModelManager::currentModelVersion() const
{
    return m_currentVersion;
}

void SearchNlModelManager::ensureModelAvailable()
{
    if (isModelAvailable())
    {
        m_currentModelPath = defaultModelPath();

        Q_EMIT signalModelReady(m_currentModelPath);

        qCInfo(DIGIKAM_NLSEARCH_LOG) << "isModelAvailable:" << isModelAvailable()
                                     << "path:" << defaultModelPath();

        return;
    }

    qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search model not present; download required via FilesDownloader:"
                                 << defaultModelPath();

    Q_EMIT signalModelError(i18nc("@info",
                                  "The natural-language search model is not installed. "
                                  "Please download it from the digiKam setup downloader."));
}

void SearchNlModelManager::cleanupObsoleteModels()
{
    // No-op: obsolete model files are cleaned up centrally by
    // FilesDownloader::deleteUnusedFiles(). Kept for interface stability.
}

} // namespace Digikam

#include "moc_searchnlmodelmanager.cpp"
