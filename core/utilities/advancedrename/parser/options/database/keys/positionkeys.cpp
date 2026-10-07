/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2010-05-23
 * Description : position information keys
 *
 * SPDX-FileCopyrightText: 2009-2012 by Andi Clemens <andi dot clemens at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "positionkeys.h"

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "coredbinfocontainers.h"
#include "iteminfo.h"
#include "itemposition.h"

namespace
{
    inline const QString& KEY_LATITUDE()
    {
        static const QString key = QLatin1String("Latitude");
        return key;
    }

    inline const QString& KEY_LONGITUDE()
    {
        static const QString key = QLatin1String("Longitude");
        return key;
    }

    inline const QString& KEY_LATTITUDENUMBER()
    {
        static const QString key = QLatin1String("LatitudeNumber");
        return key;
    }

    inline const QString& KEY_LONGITUDENUMBER()
    {
        static const QString key = QLatin1String("LongitudeNumber");
        return key;
    }

    inline const QString& KEY_LATITUDEFORMATTED()
    {
        static const QString key = QLatin1String("LatitudeFormatted");
        return key;
    }

    inline const QString& KEY_LONGITUDEFORMATTED()
    {
        static const QString key = QLatin1String("LongitudeFormatted");
        return key;
    }

    inline const QString& KEY_ALTITUDE()
    {
        static const QString key = QLatin1String("Altitude");
        return key;
    }

    inline const QString& KEY_ALTITUDEFORMATTED()
    {
        static const QString key = QLatin1String("AltitudeFormatted");
        return key;
    }

    inline const QString& KEY_ORIENTATION()
    {
        static const QString key = QLatin1String("Orientation");
        return key;
    }

    inline const QString& KEY_ROLL()
    {
        static const QString key = QLatin1String("Roll");
        return key;
    }

    inline const QString& KEY_TILT()
    {
        static const QString key = QLatin1String("Tilt");
        return key;
    }

    inline const QString& KEY_ACCURACY()
    {
        static const QString key = QLatin1String("Accuracy");
        return key;
    }

    inline const QString& KEY_DESCRIPTION()
    {
        static const QString key = QLatin1String("Description");
        return key;
    }

} // namespace

namespace Digikam
{

PositionKeys::PositionKeys()
    : DbKeysCollection(i18n("Position Information (GPS)"))
{
    addId(KEY_LATITUDE(),           i18n("Latitude in the format as described by the XMP specification"));
    addId(KEY_LONGITUDE(),          i18n("Longitude in the format as described by the XMP specification"));
    addId(KEY_LATTITUDENUMBER(),    i18n("Latitude as double value"));
    addId(KEY_LONGITUDENUMBER(),    i18n("Longitude as double value"));
    addId(KEY_LATITUDEFORMATTED(),  i18n("Latitude in a human readable form"));
    addId(KEY_LONGITUDEFORMATTED(), i18n("Longitude in a human readable form"));
    addId(KEY_ALTITUDE(),           i18n("Altitude in meters"));
    addId(KEY_ALTITUDEFORMATTED(),  i18n("Altitude in a human readable form"));
    addId(KEY_ORIENTATION(),        i18n("GPS direction of the camera in degrees (0-360°, where 0°=North, 90°=East, 180°=South, 270°=West)"));
    addId(KEY_ROLL(),               i18n("Camera roll angle in degrees (rotation around the X-axis)"));
    addId(KEY_TILT(),               i18n("Camera tilt angle in degrees (rotation around the Y-axis)"));
    addId(KEY_ACCURACY(),           i18n("GPS accuracy in meters (estimated horizontal position error)"));
    addId(KEY_DESCRIPTION(),        i18n("User-provided description of the GPS location or landmark"));
}

QString PositionKeys::getDbValue(const QString& key, ParseSettings& settings)
{
    ItemInfo info         = ItemInfo::fromUrl(settings.fileUrl);
    ItemPosition position = info.imagePosition();

    QString result;

    if      (key == KEY_LATITUDE())
    {
        result = position.latitude().simplified();
    }
    else if (key == KEY_LONGITUDE())
    {
        result = position.longitude().simplified();
    }
    else if (key == KEY_LATTITUDENUMBER())
    {
        result = QString::number(position.latitudeNumber());
    }
    else if (key == KEY_LONGITUDENUMBER())
    {
        result = QString::number(position.longitudeNumber());
    }
    else if (key == KEY_LATITUDEFORMATTED())
    {
        result = position.latitudeFormatted().simplified();
    }
    else if (key == KEY_LONGITUDEFORMATTED())
    {
        result = position.longitudeFormatted().simplified();
    }
    else if (key == KEY_ALTITUDE())
    {
        result = QString::number(position.altitude());
    }
    else if (key == KEY_ALTITUDEFORMATTED())
    {
        result = position.altitudeFormatted().simplified();
    }
    else if (key == KEY_ORIENTATION())
    {
        result = QString::number(position.orientation());
    }
    else if (key == KEY_ROLL())
    {
        result = QString::number(position.roll());
    }
    else if (key == KEY_TILT())
    {
        result = QString::number(position.tilt());
    }
    else if (key == KEY_ACCURACY())
    {
        result = QString::number(position.accuracy());
    }
    else if (key == KEY_DESCRIPTION())
    {
        result = position.description().simplified();
    }

    return result;
}

} // namespace Digikam
