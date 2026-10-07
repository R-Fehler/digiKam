/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2006-04-04
 * Description : a tool to generate HTML image galleries
 *
 * SPDX-FileCopyrightText: 2006-2010 by Aurelien Gateau <aurelien dot gateau at free dot fr>
 * SPDX-FileCopyrightText: 2012-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "gallerytheme.h"

// Qt includes

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <QDir>

// KDE includes

#include <kconfiggroup.h>
#include <kdesktopfile.h>

// Local includes

#include "digikam_debug.h"
#include "colorthemeparameter.h"
#include "intthemeparameter.h"
#include "listthemeparameter.h"
#include "stringthemeparameter.h"
#include "captionthemeparameter.h"

namespace DigikamGenericHtmlGalleryPlugin
{

namespace
{

    inline const QLatin1String& AUTHOR_GROUP()
    {
        static const QLatin1String str("X-HTMLGallery Author");
        return str;
    }

    inline const QLatin1String& PARAMETER_GROUP_PREFIX()
    {
        static const QLatin1String str("X-HTMLGallery Parameter ");
        return str;
    }

    inline const QLatin1String& PARAMETER_TYPE_KEY()
    {
        static const QLatin1String str("Type");
        return str;
    }

    inline const QLatin1String& PREVIEW_GROUP()
    {
        static const QLatin1String str("X-HTMLGallery Preview");
        return str;
    }

    inline const QLatin1String& OPTIONS_GROUP()
    {
        static const QLatin1String str("X-HTMLGallery Options");
        return str;
    }

    inline const QLatin1String& CAPTION_PARAMETER_TYPE()
    {
        static const QLatin1String str("caption");
        return str;
    }

    inline const QLatin1String& STRING_PARAMETER_TYPE()
    {
        static const QLatin1String str("string");
        return str;
    }

    inline const QLatin1String& LIST_PARAMETER_TYPE()
    {
        static const QLatin1String str("list");
        return str;
    }

    inline const QLatin1String& COLOR_PARAMETER_TYPE()
    {
        static const QLatin1String str("color");
        return str;
    }

    inline const QLatin1String& INT_PARAMETER_TYPE()
    {
        static const QLatin1String str("int");
        return str;
    }

    inline GalleryTheme::List& THEMES_LIST()
    {
        static GalleryTheme::List list;
        return list;
    }

} // namespace

class Q_DECL_HIDDEN GalleryTheme::Private
{
public:

    Private() = default;

public:

    KDesktopFile* desktopFile = nullptr;
    QUrl          url;
    ParameterList parameterList;

public:

    /**
     * Return the list of parameters defined in the desktop file. We need to
     * parse the file ourselves to preserve parameter order.
     */
    QStringList readParameterNameList(const QString& desktopFileName)
    {
        QStringList list;
        QFile file(desktopFileName);

        if (!file.open(QIODevice::ReadOnly))
        {
            return QStringList();
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        QString prefix = QLatin1String("[") + QLatin1String(PARAMETER_GROUP_PREFIX());

        while (!stream.atEnd())
        {
            QString line = stream.readLine();
            line         = line.trimmed();

            if (!line.startsWith(prefix))
            {
                continue;
            }

            // Remove opening bracket and group prefix

            line         = line.mid(prefix.length());

            // Remove closing bracket

            line.truncate(line.length() - 1);

            list.append(line);
        }

        file.close();

        return list;
    }

    void init(const QString& desktopFileName)
    {
        delete desktopFile;

        desktopFile                   = new KDesktopFile(desktopFileName);
        url                           = QUrl::fromLocalFile(desktopFileName);
        QStringList parameterNameList = readParameterNameList(desktopFileName);

        readParameters(parameterNameList);
    }

    void readParameters(const QStringList& list)
    {
        QStringList::ConstIterator it  = list.constBegin();
        QStringList::ConstIterator end = list.constEnd();

        for ( ; it != end ; ++it)
        {
            QString groupName                 = QLatin1String(PARAMETER_GROUP_PREFIX()) + *it;
            QByteArray internalName           = it->toUtf8();
            KConfigGroup group                = desktopFile->group(groupName);
            QString type                      = group.readEntry(PARAMETER_TYPE_KEY());
            AbstractThemeParameter* parameter = nullptr;

            if      (type == QLatin1String(STRING_PARAMETER_TYPE()))
            {
                parameter = new StringThemeParameter();
            }
            else if (type == QLatin1String(CAPTION_PARAMETER_TYPE()))
            {
                parameter = new CaptionThemeParameter();
            }
            else if (type == QLatin1String(LIST_PARAMETER_TYPE()))
            {
                parameter = new ListThemeParameter();
            }
            else if (type == QLatin1String(COLOR_PARAMETER_TYPE()))
            {
                parameter = new ColorThemeParameter();
            }
            else if (type == QLatin1String(INT_PARAMETER_TYPE()))
            {
                parameter = new IntThemeParameter();
            }
            else
            {
                qCWarning(DIGIKAM_DPLUGIN_GENERIC_LOG) << "Parameter '" << internalName
                                                       << "' has unknown type '" << type
                                                       << "'. Falling back to string type\n";
                parameter = new StringThemeParameter();
            }

            parameter->init(internalName, &group);
            parameterList << parameter;
        }
    }
};

GalleryTheme::GalleryTheme()
   : d(new Private)
{
}

GalleryTheme::~GalleryTheme()
{
    delete d->desktopFile;
    delete d;
}

const GalleryTheme::List& GalleryTheme::getList()
{
    if (THEMES_LIST().isEmpty())
    {
        QStringList list;
        QStringList internalNameList;
        const QStringList filter     = QStringList() << QLatin1String("*.desktop");
        const QStringList themesDirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                                                 QLatin1String("digikam/themes"),
                                                                 QStandardPaths::LocateDirectory);

        for (const QString& themeDir : std::as_const(themesDirs))
        {
            const auto infs = QDir(themeDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

            for (const QFileInfo& themeInfo : infs)
            {
                const auto files = QDir(themeInfo.absoluteFilePath()).entryInfoList(filter);

                for (const QFileInfo& deskFile : files)
                {
                    list << deskFile.absoluteFilePath();
                }
            }
        }

        QStringList::ConstIterator it  = list.constBegin();
        QStringList::ConstIterator end = list.constEnd();

        for ( ; it != end ; ++it)
        {
            GalleryTheme::Ptr theme(new GalleryTheme);
            theme->d->init(*it);
            QString intName = theme->internalName();

            if (!internalNameList.contains(intName))
            {
                THEMES_LIST() << theme;
                internalNameList << intName;
            }
        }
    }

    qCDebug(DIGIKAM_DPLUGIN_GENERIC_LOG) << "HTML Gallery Themes found:" << THEMES_LIST().size();

    return THEMES_LIST();
}

GalleryTheme::Ptr GalleryTheme::findByInternalName(const QString& internalName)
{
    const GalleryTheme::List& lst         = getList();
    GalleryTheme::List::ConstIterator it  = lst.constBegin();
    GalleryTheme::List::ConstIterator end = lst.constEnd();

    for ( ; it != end ; ++it)
    {
        GalleryTheme::Ptr theme = *it;

        if (theme->internalName() == internalName)
        {
            return theme;
        }
    }

    return GalleryTheme::Ptr(nullptr);
}

QString GalleryTheme::internalName() const
{
    return d->url.fileName();
}

QString GalleryTheme::name() const
{
    return d->desktopFile->readName();
}

QString GalleryTheme::comment() const
{
    return d->desktopFile->readComment();
}

QString GalleryTheme::directory() const
{
    return d->url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile();
}

QString GalleryTheme::authorName() const
{
    return d->desktopFile->group(AUTHOR_GROUP()).readEntry("Name");
}

QString GalleryTheme::authorUrl() const
{
    return d->desktopFile->group(AUTHOR_GROUP()).readEntry("Url");
}

QString GalleryTheme::previewName() const
{
    return d->desktopFile->group(PREVIEW_GROUP()).readEntry("Name");
}

QString GalleryTheme::previewUrl() const
{
    return d->desktopFile->group(PREVIEW_GROUP()).readEntry("Url");
}

bool GalleryTheme::allowNonsquareThumbnails() const
{
    return d->desktopFile->group(OPTIONS_GROUP()).readEntry("Allow-non-square-thumbnails", false);
}

GalleryTheme::ParameterList GalleryTheme::parameterList() const
{
    return d->parameterList;
}

} // namespace DigikamGenericHtmlGalleryPlugin
