/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2017-05-25
 * Description : a widget to play audio track.
 *
 * SPDX-FileCopyrightText: 2017-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt include

#include <QWidget>
#include <QString>
#include <QMediaPlayer>

// Local includes

#include "digikam_export.h"

namespace Digikam
{

class DIGIKAM_EXPORT AudPlayerWdg : public QWidget
{
    Q_OBJECT

public:

    explicit AudPlayerWdg(QWidget* const parent = nullptr);
    ~AudPlayerWdg() override;

    void setAudioFile(const QString& afile);

public Q_SLOTS:

    void slotPlay();
    void slotStop();

private Q_SLOTS:

    void slotTimeUpdaterTimeout(qint64);
    void slotSetVolume(int);

    void slotPlayerStateChanged(QMediaPlayer::PlaybackState);
    void slotPlayerError(QMediaPlayer::Error, const QString&);

private:

    void setZeroTime();

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
