/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2010-05-22
 * Description : metadata information keys
 *
 * SPDX-FileCopyrightText: 2009-2012 by Andi Clemens <andi dot clemens at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "metadatakeys.h"

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "coredbinfocontainers.h"
#include "iteminfo.h"

namespace
{
    inline const QString& KEY_MAKE()
    {
        static const QString key = QLatin1String("CameraMake");
        return key;
    }

    inline const QString& KEY_MODEL()
    {
        static const QString key = QLatin1String("CameraModel");
        return key;
    }

    inline const QString& KEY_LENS()
    {
        static const QString key = QLatin1String("CameraLens");
        return key;
    }

    inline const QString& KEY_APERTURE()
    {
        static const QString key = QLatin1String("Aperture");
        return key;
    }

    inline const QString& KEY_FOCALLENGTH()
    {
        static const QString key = QLatin1String("FocalLength");
        return key;
    }

    inline const QString& KEY_FOCALLENGTH35()
    {
        static const QString key = QLatin1String("FocalLength35");
        return key;
    }

    inline const QString& KEY_EXPOSURETIME()
    {
        static const QString key = QLatin1String("ExposureTime");
        return key;
    }

    inline const QString& KEY_EXPOSUREPROGRAM()
    {
        static const QString key = QLatin1String("ExposureProgram");
        return key;
    }

    inline const QString& KEY_EXPOSUREMODE()
    {
        static const QString key = QLatin1String("ExposureMode");
        return key;
    }

    inline const QString& KEY_SENSITIVITY()
    {
        static const QString key = QLatin1String("Sensitivity");
        return key;
    }

    inline const QString& KEY_FLASHMODE()
    {
        static const QString key = QLatin1String("FlashMode");
        return key;
    }

    inline const QString& KEY_WHITEBALANCE()
    {
        static const QString key = QLatin1String("WhiteBalance");
        return key;
    }

    inline const QString& KEY_WHITEBALANCECOLORTEMPERATURE()
    {
        static const QString key = QLatin1String("WhiteBalanceColorTemp");
        return key;
    }

    inline const QString& KEY_METERINGMODE()
    {
        static const QString key = QLatin1String("MeteringMode");
        return key;
    }

    inline const QString& KEY_SUBJECTDISTANCE()
    {
        static const QString key = QLatin1String("SubjectDistance");
        return key;
    }

    inline const QString& KEY_SUBJECTDISTANCECATEGORY()
    {
        static const QString key = QLatin1String("SubjectDistanceCategory");
        return key;
    }

    inline const QString& KEY_ASPECTRATIO()
    {
        static const QString key = QLatin1String("AspectRatio");
        return key;
    }

    inline const QString& KEY_AUDIOBITRATE()
    {
        static const QString key = QLatin1String("AudioBitRate");
        return key;
    }

    inline const QString& KEY_AUDIOCHANNELTYPE()
    {
        static const QString key = QLatin1String("AudioChannelType");
        return key;
    }

    inline const QString& KEY_AUDIOCODEC()
    {
        static const QString key = QLatin1String("AudioCodec");
        return key;
    }

    inline const QString& KEY_DURATION()
    {
        static const QString key = QLatin1String("Duration");
        return key;
    }

    inline const QString& KEY_FRAMERATE()
    {
        static const QString key = QLatin1String("FrameRate");
        return key;
    }

    inline const QString& KEY_VIDEOCODEC()
    {
        static const QString key = QLatin1String("VideoCodec");
        return key;
    }
} // namespace

namespace Digikam
{

MetadataKeys::MetadataKeys()
    : DbKeysCollection(i18n("Metadata Information"))
{
    addId(KEY_MAKE(),                         i18n("Make of the camera"));
    addId(KEY_MODEL(),                        i18n("Model of the camera"));
    addId(KEY_LENS(),                         i18n("Lens of the camera"));
    addId(KEY_APERTURE(),                     i18n("Aperture"));
    addId(KEY_FOCALLENGTH(),                  i18n("Focal length"));
    addId(KEY_FOCALLENGTH35(),                i18n("Focal length (35mm equivalent)"));
    addId(KEY_EXPOSURETIME(),                 i18n("Exposure time"));
    addId(KEY_EXPOSUREPROGRAM(),              i18n("Exposure program"));
    addId(KEY_EXPOSUREMODE(),                 i18n("Exposure mode"));
    addId(KEY_SENSITIVITY(),                  i18n("Sensitivity"));
    addId(KEY_FLASHMODE(),                    i18n("Flash mode"));
    addId(KEY_WHITEBALANCE(),                 i18n("White balance"));
    addId(KEY_WHITEBALANCECOLORTEMPERATURE(), i18n("White balance (color temperature)"));
    addId(KEY_METERINGMODE(),                 i18n("Metering mode"));
    addId(KEY_SUBJECTDISTANCE(),              i18n("Subject distance"));
    addId(KEY_SUBJECTDISTANCECATEGORY(),      i18n("Subject distance (category)"));
    addId(KEY_ASPECTRATIO(),                  i18n("Display Aspect Ratio"));
    addId(KEY_AUDIOBITRATE(),                 i18n("Audio Bit Rate"));
    addId(KEY_AUDIOCHANNELTYPE(),             i18n("Audio Channel Type"));
    addId(KEY_AUDIOCODEC(),                   i18n("Audio Codec (Audio Codec)"));
    addId(KEY_DURATION(),                     i18n("Duration of File"));
    addId(KEY_FRAMERATE(),                    i18n("Frame Rate of Video"));
    addId(KEY_VIDEOCODEC(),                   i18n("Video Codec"));
}

QString MetadataKeys::getDbValue(const QString& key, ParseSettings& settings)
{
    ItemInfo info                         = ItemInfo::fromUrl(settings.fileUrl);
    ImageMetadataContainer container      = info.imageMetadataContainer();
    VideoMetadataContainer videoContainer = info.videoMetadataContainer();
    QString result;

    if      (key == KEY_MAKE())
    {
        result = container.make;
    }
    else if (key == KEY_MODEL())
    {
        result = container.model;
    }
    else if (key == KEY_LENS())
    {
        result = container.lens;
    }
    else if (key == KEY_APERTURE())
    {
        result = container.aperture;
    }
    else if (key == KEY_FOCALLENGTH())
    {
        result = container.focalLength;
    }
    else if (key == KEY_FOCALLENGTH35())
    {
        result = container.focalLength35;
    }
    else if (key == KEY_EXPOSURETIME())
    {
        result = container.exposureTime;
    }
    else if (key == KEY_EXPOSUREPROGRAM())
    {
        result = container.exposureProgram;
    }
    else if (key == KEY_EXPOSUREMODE())
    {
        result = container.exposureMode;
    }
    else if (key == KEY_SENSITIVITY())
    {
        result = container.sensitivity;
    }
    else if (key == KEY_FLASHMODE())
    {
        result = container.flashMode;
    }
    else if (key == KEY_WHITEBALANCE())
    {
        result = container.whiteBalance;
    }
    else if (key == KEY_WHITEBALANCECOLORTEMPERATURE())
    {
        result = container.whiteBalanceColorTemperature;
    }
    else if (key == KEY_METERINGMODE())
    {
        result = container.meteringMode;
    }
    else if (key == KEY_SUBJECTDISTANCE())
    {
        result = container.subjectDistance;
    }
    else if (key == KEY_SUBJECTDISTANCECATEGORY())
    {
        result = container.subjectDistanceCategory;
    }
    else if (key == KEY_ASPECTRATIO())
    {
        result = videoContainer.aspectRatio;
    }
    else if (key == KEY_AUDIOBITRATE())
    {
        result = videoContainer.audioBitRate;
    }
    else if (key == KEY_AUDIOCHANNELTYPE())
    {
        result = videoContainer.audioChannelType;
    }
    else if (key == KEY_AUDIOCODEC())
    {
        result = videoContainer.audioCodec;
    }
    else if (key == KEY_DURATION())
    {
        result = videoContainer.duration;
    }
    else if (key == KEY_FRAMERATE())
    {
        result = videoContainer.frameRate;
    }
    else if (key == KEY_VIDEOCODEC())
    {
        result = videoContainer.videoCodec;
    }

    result.replace(QLatin1Char('/'), QLatin1Char('_'));

    return result;
}

} // namespace Digikam
