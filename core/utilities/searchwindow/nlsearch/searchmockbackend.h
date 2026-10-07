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
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QMap>

// Local includes

#include "searchlanguagebackend.h"
#include "digikam_export.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchMockBackend : public SearchLanguageBackend
{
    Q_OBJECT

public:

    explicit SearchMockBackend(QObject* const parent = nullptr);
    ~SearchMockBackend()                         override = default;

    bool loadModel(const QString& modelPath)     override;
    void unloadModel()                           override;
    bool isModelLoaded()                   const override;
    QString modelPath()                    const override;
    QString backendName()                  const override;

    /**
     * @brief Register a canned response: any prompt *containing* the given
     * query substring returns the given JSON payload.
     */
    void addCannedResponse(const QString& querySubstring, const QString& jsonOutput);

public Q_SLOTS:

    void slotRunInference(const QString& prompt) override;

private:

    bool                    m_loaded = false;
    QString                 m_path;
    QMap<QString, QString>  m_canned;           ///< query substring -> JSON output
};

} // namespace Digikam
