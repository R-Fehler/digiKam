/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - file names written by sync tools:
 *               temporary files and conflict copies. Header only,
 *               unit tested by streamline/tests/syncnames_test.cpp.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace Digikam
{

/**
 * A file (path relative to a watched folder) which is not a finished photo:
 * hidden files and folders (.stversions, .thumbnails, ._resource forks,
 * Syncthing's .syncthing.NAME.tmp), partial downloads, conflict copies.
 */
inline bool photosIsTemporaryName(const QString& relativePath)
{
    const QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    for (const QString& part : parts)
    {
        if (part.startsWith(QLatin1Char('.')) || part.startsWith(QLatin1Char('~')))
        {
            return true;
        }
    }

    const QString name = parts.isEmpty() ? relativePath : parts.constLast();

    static const char* const suffixes[] =
    {
        ".tmp", ".temp", ".part", ".partial", ".crdownload", ".download", ".!sync", ".filepart"
    };

    for (const char* const suffix : suffixes)
    {
        if (name.endsWith(QLatin1String(suffix), Qt::CaseInsensitive))
        {
            return true;
        }
    }

    return (
            name.contains(QLatin1String(".syncthing."), Qt::CaseInsensitive) ||
            name.contains(QLatin1String("sync-conflict"), Qt::CaseInsensitive) ||
            name.contains(QLatin1String("conflicted copy"), Qt::CaseInsensitive)
           );
}

/**
 * For a conflict copy of a sidecar, the name of the original sidecar; empty
 * for other files (a conflict copy of a photo is a photo of its own):
 *
 *  - Syncthing:          NAME.sync-conflict-YYYYMMDD-HHMMSS-DEVICEID.EXT
 *  - Dropbox:            NAME (Anna's conflicted copy YYYY-MM-DD).EXT
 *  - Nextcloud/ownCloud: NAME (conflicted copy YYYY-MM-DD HHMMSS).EXT,
 *                        NAME_conflict-YYYYMMDD-HHMMSS.EXT
 */
inline QString photosConflictOriginal(const QString& fileName)
{
    static const QRegularExpression syncthing(QLatin1String("^(.+)\\.sync-conflict-\\d{8}-\\d{6}-[A-Z0-9]+(\\.[^.]+)$"));
    static const QRegularExpression copy(QLatin1String("^(.+) \\([^)]*conflicted copy[^)]*\\)(\\.[^.]+)$"));
    static const QRegularExpression owncloud(QLatin1String("^(.+)_conflict-\\d{8}-\\d{6}(\\.[^.]+)$"));

    for (const QRegularExpression* const re : { &syncthing, &copy, &owncloud })
    {
        const QRegularExpressionMatch match = re->match(fileName);

        if (match.hasMatch())
        {
            const QString original = match.captured(1) + match.captured(2);

            return original.endsWith(QLatin1String(".xmp"), Qt::CaseInsensitive) ? original : QString();
        }
    }

    return QString();
}

} // namespace Digikam
