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
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QString>

// Local includes

#include "digikam_export.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchNlModelManager : public QObject
{
    Q_OBJECT

public:

    explicit SearchNlModelManager(QObject* const parent = nullptr);
    ~SearchNlModelManager()              override;

public:

    QString currentModelPath()    const;
    QString currentModelVersion() const;

    /**
     * @brief Ensure the model file exists locally. If missing, triggers a
     * download through FilesDownloader (KDE-hosted asset). Emits
     * signalModelReady() or signalModelError().
     */
    void ensureModelAvailable();

    // @brief Remove superseded model versions from the storage directory.
    void cleanupObsoleteModels();

public:

    static QString defaultModelPath();
    static bool    isModelAvailable();

Q_SIGNALS:

    void signalDownloadProgress(qint64 received, qint64 total);
    void signalModelReady(const QString& localPath);
    void signalModelError(const QString& message);

private:

    QString m_currentModelPath;
    QString m_currentVersion = QLatin1String("v1");
};

} // namespace Digikam
