/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2007-09-09
 * Description : scanner dialog
 *
 * SPDX-FileCopyrightText: 2007-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QCloseEvent>
#include <QWidget>
#include <QImage>

// KDE include

#include <ksanewidget.h>

// Local includes

#include "dplugindialog.h"

using namespace Digikam;
using namespace KSaneIface;

namespace DigikamGenericDScannerPlugin
{

class ScanDialog : public DPluginDialog
{
    Q_OBJECT

public:

    explicit ScanDialog(KSaneWidget* const saneWdg, QWidget* const parent = nullptr);
    ~ScanDialog()                   override;

    void setTargetDir(const QString& targetDir);

protected:

    void closeEvent(QCloseEvent*)   override;

Q_SIGNALS:

    void signalImportedImage(const QUrl&);

private Q_SLOTS:

    void slotSaveImage(const QImage&);
    void slotThreadProgress(const QUrl&, int);
    void slotThreadDone(const QUrl&, bool);
    void slotDialogFinished();

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace DigikamGenericDScannerPlugin
