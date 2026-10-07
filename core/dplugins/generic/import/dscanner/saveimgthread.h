/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2009-10-11
 * Description : save image thread for scanned data
 *
 * SPDX-FileCopyrightText: 2009-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QThread>
#include <QString>
#include <QUrl>
#include <QByteArray>
#include <QImage>

// KDE includes

namespace DigikamGenericDScannerPlugin
{

class SaveImgThread : public QThread
{
    Q_OBJECT

public:

    explicit SaveImgThread(QObject* const parent);
    ~SaveImgThread()    override;

    void setTargetFile(const QUrl& url, const QString& format);
    void setScannerModel(const QString& make, const QString& model);
    void setImageData(const QImage& imageData);

Q_SIGNALS:

    void signalProgress(const QUrl&, int);
    void signalComplete(const QUrl&, bool);

private:

    void run()          override;

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace DigikamGenericDScannerPlugin
