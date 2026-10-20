/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - "Open in Photos" in the context menu of
 *               file managers, for the current user.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "photosshellintegration.h"

// Qt includes

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

// KDE includes

#include <klocalizedstring.h>

#ifdef Q_OS_WIN
#   include <windows.h>
#   include <shlobj.h>
#endif

namespace Digikam
{

namespace PhotosShellIntegration
{

namespace
{

#if !defined(Q_OS_MACOS)

QString menuLabel()
{
    return i18nc("@action: file manager context menu", "Open in Photos");
}

#endif

#if defined(Q_OS_WIN)

// --- Windows: Explorer verbs of the current user ---------------------------------

const char* const s_verb = "digiKamPhotos";

/// Where the verb goes, and the placeholder of the path for it.
const QList<QPair<QString, QString> >& verbKeys()
{
    static const QList<QPair<QString, QString> > keys =
    {
        { QStringLiteral("Directory/shell"),                       QStringLiteral("%1") },   // a folder
        { QStringLiteral("Directory/Background/shell"),            QStringLiteral("%V") },   // inside a folder
        { QStringLiteral("SystemFileAssociations/image/shell"),    QStringLiteral("%1") },
        { QStringLiteral("SystemFileAssociations/video/shell"),    QStringLiteral("%1") }
    };

    return keys;
}

QSettings& classes()
{
    static QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);

    return settings;
}

void notifyExplorer()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

#elif !defined(Q_OS_MACOS)

// --- Linux and other freedesktop systems -----------------------------------------

QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

QString dolphinMenu()
{
    return dataDir() + QLatin1String("/kio/servicemenus/org.kde.digikam.photos-open.desktop");
}

QString openWithEntry()
{
    return dataDir() + QLatin1String("/applications/org.kde.digikam.photos.desktop");
}

QString nemoAction()
{
    return dataDir() + QLatin1String("/nemo/actions/org.kde.digikam.photos.nemo_action");
}

QString nautilusScript()
{
    return dataDir() + QLatin1String("/nautilus/scripts/") + menuLabel();
}

QString cajaScript()
{
    return configDir() + QLatin1String("/caja/scripts/") + menuLabel();
}

/// Exec key of desktop entries: quoted when needed, with its escapes.
QString desktopExec(const QString& program)
{
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    bool quote = false;

    for (const QChar& c : program)
    {
        if (reserved.contains(c))
        {
            quote = true;
            break;
        }
    }

    if (!quote)
    {
        return program;
    }

    QString escaped;

    for (const QChar& c : program)
    {
        if ((c == QLatin1Char('"')) || (c == QLatin1Char('`')) || (c == QLatin1Char('$')) || (c == QLatin1Char('\\')))
        {
            escaped += QLatin1Char('\\');
        }

        escaped += c;
    }

    // Backslashes are escaped once more in a desktop entry string.

    escaped.replace(QLatin1String("\\"), QLatin1String("\\\\"));

    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

/// A path in single quotes for sh.
QString shellQuote(const QString& text)
{
    QString quoted = text;
    quoted.replace(QLatin1String("'"), QLatin1String("'\\''"));

    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

bool writeFile(const QString& path, const QString& content, bool executable)
{
    QDir().mkpath(QFileInfo(path).path());

    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }

    file.write(content.toUtf8());

    if (!file.commit())
    {
        return false;
    }

    if (executable)
    {
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner |
                                    QFileDevice::ExeGroup    | QFileDevice::ExeOther);
    }

    return true;
}

const char* const s_mimeTypes = "inode/directory;image/jpeg;image/png;image/heif;image/heic;image/avif;"
                                "image/jxl;image/webp;image/tiff;image/gif;image/x-adobe-dng;"
                                "image/x-canon-cr2;image/x-canon-cr3;image/x-nikon-nef;image/x-sony-arw;"
                                "image/x-fuji-raf;image/x-olympus-orf;image/x-panasonic-rw2;"
                                "video/mp4;video/quicktime;video/x-matroska;video/webm;";

#endif

} // namespace

QString programPath()
{
    // Run from an AppImage: the AppImage, not the program inside its mount.

    const QString appImage = qEnvironmentVariable("APPIMAGE");

    if (!appImage.isEmpty() && QFileInfo::exists(appImage))
    {
        return appImage;
    }

    return QCoreApplication::applicationFilePath();
}

#if defined(Q_OS_WIN)

bool isSupported()
{
    return true;
}

bool isInstalled()
{
    return classes().contains(verbKeys().constFirst().first + QLatin1Char('/') +
                              QLatin1String(s_verb) + QLatin1String("/command/Default"));
}

bool install()
{
    const QString program = QDir::toNativeSeparators(programPath());

    for (const QPair<QString, QString>& key : verbKeys())
    {
        const QString verb = key.first + QLatin1Char('/') + QLatin1String(s_verb);

        classes().setValue(verb + QLatin1String("/Default"),         menuLabel());
        classes().setValue(verb + QLatin1String("/Icon"),            QString(QLatin1Char('"') + program + QLatin1String("\",0")));
        classes().setValue(verb + QLatin1String("/command/Default"),
                           QString(QLatin1Char('"') + program + QLatin1String("\" --photos \"") + key.second + QLatin1Char('"')));
    }

    classes().sync();
    notifyExplorer();

    return (classes().status() == QSettings::NoError);
}

void uninstall()
{
    for (const QPair<QString, QString>& key : verbKeys())
    {
        classes().remove(key.first + QLatin1Char('/') + QLatin1String(s_verb));
    }

    classes().sync();
    notifyExplorer();
}

#elif defined(Q_OS_MACOS)

bool isSupported()
{
    return false;       // Finder: "Open With", from the document types of the bundle.
}

bool isInstalled()
{
    return false;
}

bool install()
{
    return false;
}

void uninstall()
{
}

#else

bool isSupported()
{
    return true;
}

bool isInstalled()
{
    return QFileInfo::exists(openWithEntry());
}

bool install()
{
    const QString program = programPath();
    const QString exec    = desktopExec(program);
    bool ok               = true;

    // "Open With" in every file manager, and in application menus.

    ok &= writeFile(openWithEntry(), QString::fromLatin1(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=digiKam Photos\n"
        "GenericName=Photo Library\n"
        "Comment=%1\n"
        "Exec=%2 --photos %U\n"
        "Icon=digikam\n"
        "Terminal=false\n"
        "Categories=Qt;KDE;Graphics;Photography;\n"
        "MimeType=%3\n"
        "Keywords=photos;pictures;album;library;\n")
        .arg(i18n("Browse your photos with digiKam's Photos mode"), exec, QLatin1String(s_mimeTypes)), false);

    // Dolphin (KDE): a service menu, executable as KF6 asks.

    if (!QStandardPaths::findExecutable(QLatin1String("dolphin")).isEmpty() || QFileInfo::exists(dolphinMenu()))
    {
        ok &= writeFile(dolphinMenu(), QString::fromLatin1(
            "[Desktop Entry]\n"
            "Type=Service\n"
            "MimeType=%1\n"
            "Actions=openInPhotos;\n"
            "X-KDE-Priority=TopLevel\n"
            "\n"
            "[Desktop Action openInPhotos]\n"
            "Name=%2\n"
            "Icon=digikam\n"
            "Exec=%3 --photos %U\n")
            .arg(QLatin1String(s_mimeTypes), menuLabel(), exec), true);
    }

    // Nemo (Cinnamon): an action.

    if (!QStandardPaths::findExecutable(QLatin1String("nemo")).isEmpty() || QFileInfo::exists(nemoAction()))
    {
        ok &= writeFile(nemoAction(), QString::fromLatin1(
            "[Nemo Action]\n"
            "Name=%1\n"
            "Comment=%1\n"
            "Exec=%2 --photos %F\n"
            "Icon-Name=digikam\n"
            "Selection=any\n"
            "Mimetypes=%3\n")
            .arg(menuLabel(), exec, QLatin1String(s_mimeTypes)), false);
    }

    // Nautilus (GNOME) and Caja (MATE): scripts, under "Scripts" in the menu.
    // Nothing selected: the folder shown.

    const QString script = QString::fromLatin1(
        "#!/bin/sh\n"
        "# Installed by digiKam (Photos mode settings): opens the selection, or this folder, in Photos.\n"
        "if [ \"$#\" -eq 0 ]; then\n"
        "    set -- \"${NAUTILUS_SCRIPT_CURRENT_URI:-$CAJA_SCRIPT_CURRENT_URI}\"\n"
        "fi\n"
        "for f in \"$@\"; do\n"
        "    case \"$f\" in\n"
        "        /*|*://*) set -- \"$@\" \"$f\" ;;\n"
        "        *)        set -- \"$@\" \"$PWD/$f\" ;;\n"
        "    esac\n"
        "    shift\n"
        "done\n"
        "exec %1 --photos \"$@\"\n").arg(shellQuote(program));

    if (!QStandardPaths::findExecutable(QLatin1String("nautilus")).isEmpty() || QFileInfo::exists(nautilusScript()))
    {
        ok &= writeFile(nautilusScript(), script, true);
    }

    if (!QStandardPaths::findExecutable(QLatin1String("caja")).isEmpty() || QFileInfo::exists(cajaScript()))
    {
        ok &= writeFile(cajaScript(), script, true);
    }

    QProcess::startDetached(QLatin1String("update-desktop-database"),
                            QStringList() << dataDir() + QLatin1String("/applications"));

    return ok;
}

void uninstall()
{
    QFile::remove(dolphinMenu());
    QFile::remove(openWithEntry());
    QFile::remove(nemoAction());
    QFile::remove(nautilusScript());
    QFile::remove(cajaScript());

    QProcess::startDetached(QLatin1String("update-desktop-database"),
                            QStringList() << dataDir() + QLatin1String("/applications"));
}

#endif

} // namespace PhotosShellIntegration

} // namespace Digikam
