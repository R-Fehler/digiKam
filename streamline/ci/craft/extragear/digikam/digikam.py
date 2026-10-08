# -*- coding: utf-8 -*-
# Copyright (c) 2019-2022 by Gilles Caulier <caulier dot gilles at gmail dot com>
# Copyright (c) 2019-2020 by Ben Cooksley <bcooksley at kde dot org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

# NOTE: see relevant phabricator entry https://phabricator.kde.org/T12071
#
# digiKam Photos fork: copy of craft-blueprints-kde extragear/digikam/digikam.py,
# installed over it by streamline/ci/craft-build.py. Changes:
#  - no kde/applications/marble: this digiKam ships its own geolocation engine,
#    and Marble does not build on macOS with the current Craft cache;
#  - libs/libusb only where libgphoto2 uses it (not on Windows, where it does
#    not compile with current MSVC because of /WX);
#  - no Marble file shuffling in the Windows bundle;
#  - Qt modules this digiKam needs: qtscxml (StateMachine), qtmultimedia,
#    qtdeclarative (Qt Quick, for Photos mode);
#  - libs/boost (Boost Graph, now required) and libs/libheif (HEIC photos);
#  - ExifTool's test directory is removed from the bundles (fake binaries);
#  - Windows: digiKam's data moved from share/ to bin/data, where it is looked
#    for; Windows and Linux: Breeze icon resources added.

import os

import info
import utils
from CraftCore import CraftCore
from Package.CMakePackageBase import CMakePackageBase
from Utils import GetFiles


class subinfo(info.infoclass):
    def setTargets(self):
        self.svnTargets["master"] = "https://anongit.kde.org/digikam.git"
        self.defaultTarget = "master"
        self.displayName = "digiKam"
        self.webpage = "https://www.digikam.org"
        self.description = "Professional Photo Management with the Power of Open Source"

    def setDependencies(self):
        # For i18n extraction

        if CraftCore.compiler.isWindows or CraftCore.compiler.isMacOS:
            self.buildDependencies["dev-utils/subversion"] = None
            self.buildDependencies["dev-utils/ruby"] = None

        self.runtimeDependencies["virtual/base"] = None
        self.buildDependencies["kde/frameworks/extra-cmake-modules"] = None
        self.buildDependencies["dev-utils/flexbison"] = None

        # Android ffmpeg is broken.

        if not CraftCore.compiler.isAndroid:
            # digiKam mediaPlayer is not yet fully ported to FFMPEG 5 API

            self.runtimeDependencies["libs/ffmpeg"] = "4.4"

        self.runtimeDependencies["libs/opencv/opencv"] = None
        self.runtimeDependencies["libs/sqlite"] = None
        self.runtimeDependencies["libs/x265"] = None
        self.runtimeDependencies["libs/libass"] = None
        self.runtimeDependencies["libs/tiff"] = None

        if CraftCore.compiler.isLinux or CraftCore.compiler.isMacOS:
            self.runtimeDependencies["libs/libgphoto2"] = None
            self.runtimeDependencies["libs/libusb-compat"] = None

        # Boost Graph (header only) is required since digiKam 9.

        self.buildDependencies["libs/boost"] = None

        # HEIF/HEIC (phone photos).

        self.runtimeDependencies["libs/libheif"] = None

        self.runtimeDependencies["libs/expat"] = None
        self.runtimeDependencies["libs/lcms2"] = None
        self.runtimeDependencies["libs/eigen3"] = None
        self.runtimeDependencies["libs/exiv2"] = None
        self.runtimeDependencies["libs/lensfun"] = None
        self.runtimeDependencies["libs/libpng"] = None
        self.runtimeDependencies["libs/libxslt"] = None
        self.runtimeDependencies["libs/libxml2"] = None
        self.runtimeDependencies["libs/openal-soft"] = None
        self.runtimeDependencies["libs/libjpeg-turbo"] = None
        self.runtimeDependencies["libs/libass"] = None

        if CraftCore.compiler.isLinux or CraftCore.compiler.isMacOS:
            self.runtimeDependencies["libs/libusb"] = None
        self.runtimeDependencies["libs/qt/qtbase"] = None
        self.runtimeDependencies["libs/qt/qtsvg"] = None
        self.runtimeDependencies["libs/qt/qtimageformats"] = None
        self.runtimeDependencies["libs/qt/qtnetworkauth"] = None

        # StateMachine (required since digiKam 9), Multimedia (media player),
        # Qt Quick (Photos mode).

        self.runtimeDependencies["libs/qt/qtscxml"] = None
        self.runtimeDependencies["libs/qt/qtmultimedia"] = None
        self.runtimeDependencies["libs/qt/qtdeclarative"] = None

        if CraftCore.compiler.isMinGW():
            # mingw-based builds need this

            self.runtimeDependencies["libs/runtime"] = None

            self.buildDependencies["libs/boost"] = None
        else:
            self.runtimeDependencies["libs/qt/qtwebengine"] = None

        self.runtimeDependencies["kde/frameworks/tier1/breeze-icons"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kconfig"] = None
        self.runtimeDependencies["kde/frameworks/tier1/ki18n"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kxmlgui"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kwindowsystem"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kservice"] = None
        self.runtimeDependencies["kde/frameworks/tier1/solid"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kcoreaddons"] = None
        self.runtimeDependencies["kde/frameworks/tier3/knotifyconfig"] = None
        self.runtimeDependencies["kde/frameworks/tier3/knotifications"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kiconthemes"] = None
        self.runtimeDependencies["kde/plasma/breeze"] = None

        # For Panorama export tool.

        self.runtimeDependencies["kde/frameworks/tier1/threadweaver"] = None

        # For Calendar export plugin.

        self.runtimeDependencies["kde/frameworks/tier1/kcalendarcore"] = None

        # For some digiKam plugins used to export on web services.

        self.runtimeDependencies["kde/frameworks/tier3/kio"] = None

        # To support more formats in digiKam Qt plugin image loaders.

        self.runtimeDependencies["kde/frameworks/tier1/kimageformats"] = None

        # To support Mysql database

        self.runtimeDependencies["binary/mysql"] = None


class Package(CMakePackageBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        if CraftCore.compiler.isLinux:
            self.subinfo.options.configure.args = [
                "-DENABLE_KFILEMETADATASUPPORT=OFF",
                "-DENABLE_AKONADICONTACTSUPPORT=OFF",
                "-DENABLE_MEDIAPLAYER=ON",
                "-DENABLE_DBUS=ON",
                "-DENABLE_QWEBENGINE=ON",
                "-DENABLE_MYSQLSUPPORT=ON",
                "-DENABLE_INTERNALMYSQL=ON",
                "-DENABLE_DIGIKAM_MODELTEST=OFF",
                "-DENABLE_DRMINGW=OFF",
                "-DENABLE_MINGW_HARDENING_LINKER=OFF",
                "-DDIGIKAMSC_COMPILE_PO=ON",
                "-DDIGIKAMSC_COMPILE_DIGIKAM=ON",
            ]

        if CraftCore.compiler.isMSVC():
            self.subinfo.options.configure.args = [
                "-DENABLE_KFILEMETADATASUPPORT=OFF",
                "-DENABLE_AKONADICONTACTSUPPORT=OFF",
                "-DENABLE_MEDIAPLAYER=ON",
                "-DENABLE_DBUS=OFF",
                "-DENABLE_APPSTYLES=ON",
                "-DENABLE_QWEBENGINE=ON",
                "-DENABLE_MYSQLSUPPORT=ON",
                "-DENABLE_INTERNALMYSQL=ON",
                "-DENABLE_DIGIKAM_MODELTEST=OFF",
                "-DENABLE_DRMINGW=OFF",
                "-DENABLE_MINGW_HARDENING_LINKER=OFF",
                "-DDIGIKAMSC_COMPILE_PO=ON",
                "-DDIGIKAMSC_COMPILE_DIGIKAM=ON",
            ]

        if CraftCore.compiler.isMinGW():
            self.subinfo.options.configure.args = [
                "-DENABLE_KFILEMETADATASUPPORT=OFF",
                "-DENABLE_AKONADICONTACTSUPPORT=OFF",
                "-DENABLE_MEDIAPLAYER=ON",
                "-DENABLE_DBUS=OFF",
                "-DENABLE_QWEBENGINE=OFF",
                "-DENABLE_MYSQLSUPPORT=ON",
                "-DENABLE_INTERNALMYSQL=ON",
                "-DENABLE_DIGIKAM_MODELTEST=OFF",
                "-DENABLE_DRMINGW=ON",
                "-DENABLE_MINGW_HARDENING_LINKER=ON",
                "-DDIGIKAMSC_COMPILE_PO=ON",
                "-DDIGIKAMSC_COMPILE_DIGIKAM=ON",
            ]

        if CraftCore.compiler.isMacOS:
            self.subinfo.options.configure.args = [
                "-DENABLE_KFILEMETADATASUPPORT=OFF",
                "-DENABLE_AKONADICONTACTSUPPORT=OFF",
                "-DENABLE_MEDIAPLAYER=ON",
                "-DENABLE_DBUS=OFF",
                "-DENABLE_QWEBENGINE=ON",
                "-DENABLE_MYSQLSUPPORT=ON",
                "-DENABLE_INTERNALMYSQL=ON",
                "-DENABLE_DIGIKAM_MODELTEST=OFF",
                "-DENABLE_DRMINGW=OFF",
                "-DENABLE_MINGW_HARDENING_LINKER=OFF",
                "-DDIGIKAMSC_COMPILE_PO=ON",
                "-DDIGIKAMSC_COMPILE_DIGIKAM=ON",
            ]

    def createPackage(self):
        self.defines["productname"] = "digiKam"
        self.defines["website"] = "https://www.digikam.org"
        self.defines["company"] = "digiKam.org"
        self.defines["license"] = self.sourceDir() / "COPYING"

        # Not yet supported by Craft with NSIS
        # self.defines["readme"]      = os.path.join(self.blueprintDir(), "ABOUT.txt")

        # In AppImage, run the new startup script sctip with advanced features.

        self.defines["runenv"] = [
            'APPIMAGE_EXTRACT_AND_RUN=1 && "$this_dir"/usr/bin/AppRun.digiKam "$@" && exit',
        ]

        # Windows-only, mac is handled implicitly

        self.defines["executable"] = "bin\\digikam.exe"

        # Windows-only

        self.defines["icon"] = self.blueprintDir() / "digikam.ico"

        # Windows-only extra icons

        self.defines["icon_png"] = os.path.join(self.sourceDir(), "core", "data", "icons", "apps", "128-apps-digikam.png")

        # Windows-only application shortcuts

        self.defines["shortcuts"] = [
            {"name": "digiKam", "target": "bin/digikam.exe", "description": self.subinfo.description, "icon": "$INSTDIR\\digikam.ico"},
            {"name": "Showfoto", "target": "bin/showfoto.exe", "description": "digiKam stand alone Image Editor", "icon": "$INSTDIR\\showfoto.ico"},
        ]

        # Files to drop from the bundles

        self.blacklist_file.append(self.blueprintDir() / "blacklist_common.txt")

        if CraftCore.compiler.isWindows:
            self.blacklist_file.append(self.blueprintDir() / "blacklist_win.txt")

        if CraftCore.compiler.isMacOS:
            self.blacklist_file.append(self.blueprintDir() / "blacklist_mac.txt")

        if CraftCore.compiler.isLinux:
            self.blacklist_file.append(self.blueprintDir() / "blacklist_lin.txt")

        # Drop dbus support for non Linux target

        if not CraftCore.compiler.isLinux:
            self.ignoredPackages.append("libs/dbus")

        return super().createPackage()

    def _copyIconResources(self, dataDir):
        # Breeze icon themes as Qt resources, loaded by digiKam from its data
        # directory (as in the official bundles; Craft's packages provide none).

        for name in ("breeze.rcc", "breeze-dark.rcc"):
            if not utils.copyFile(self.sourceDir() / "project" / "bundles" / "common" / name, dataDir / name):
                print(f"Could not copy {name}")
                return False

        return True

    def preArchive(self):
        # Copy More application icons in Windows bundle.

        if CraftCore.compiler.isWindows:
            if not utils.copyFile(self.blueprintDir() / "showfoto.ico", self.archiveDir() / "showfoto.ico"):
                print("Could not copy showfoto.ico file")
                return False

        if CraftCore.compiler.isMSVC():
            archiveDir = self.archiveDir()
            binPath = archiveDir / "bin"

            # digiKam installs its data below share/, but on Windows Qt and KDE look
            # for it in bin/data (QStandardPaths: <APPDIR>/data, Craft's convention).
            # The bundled MySQL keeps its own files in share/.

            for name in ["digikam", "showfoto", "kxmlgui5", "knotifications6", "solid",
                         "applications", "metainfo", "icons", "locale"]:
                if (archiveDir / "share" / name).exists():
                    if not utils.mergeTree(archiveDir / "share" / name, binPath / "data" / name):
                        print(f"Could not move share/{name} to bin/data")
                        return False

            if not self._copyIconResources(binPath / "data" / "digikam"):
                return False

            # Move digiKam plugins from bin/digikam/ to bin/plugins/digikam/

            pluginsPath = archiveDir / "bin/plugins"
            utils.createDir(pluginsPath)

            if not utils.moveFile(archiveDir / "bin/digikam", pluginsPath / "digikam"):
                print("Could not move digiKam plugins dir")
                return False

            # Download exiftool.exe in the bundle

            if not GetFiles.getFile("https://files.kde.org/digikam/exiftool/exiftool.zip", binPath, "exiftool.zip"):
                print("Could not get ExifTool archive")
                return False

            if not utils.unpackFile(binPath, "exiftool.zip", binPath):
                print("Could not unpack ExifTool archive")
                return False

            if not utils.moveFile(os.path.join(binPath, "exiftool(-k).exe"), os.path.join(binPath, "exiftool.exe")):
                print("Could not rename ExifTool binary")
                return False

            if not utils.deleteFile(os.path.join(binPath, "exiftool.zip")):
                print("Could not remove ExifTool archive")
                return False

        if CraftCore.compiler.isLinux:
            # --- Manage files under AppImage bundle

            archiveDir = self.archiveDir()
            binPath = os.path.join(archiveDir, "bin")

            if not self._copyIconResources(archiveDir / "share" / "digikam"):
                return False

            # Download exiftool in the bundle

            if not GetFiles.getFile("https://files.kde.org/digikam/exiftool/Image-ExifTool.tar.gz", binPath, "Image-ExifTool.tar.gz"):
                print("Could not get ExifTool archive")
                return False

            if not utils.unpackFile(binPath, "Image-ExifTool.tar.gz", binPath):
                print("Could not unpack ExifTool archive")
                return False

            binfiles = os.listdir(binPath)
            etname = None

            for f in binfiles:
                if f.startswith("Image-ExifTool-"):
                    etname = f
                    break

            if not utils.moveFile(os.path.join(binPath, etname), os.path.join(binPath, "Image-ExifTool")):
                print("Could not rename ExifTool directory")
                return False

            # ExifTool's test suite contains fake executables (t/images/EXE.*),
            # which break the packager's binary processing (patchelf, signing).

            utils.rmtree(os.path.join(binPath, "Image-ExifTool", "t"))

            os.symlink(os.path.join("./", "Image-ExifTool", "exiftool"), os.path.join(self.archiveDir(), "bin", "exiftool"))

            if not utils.deleteFile(os.path.join(binPath, "Image-ExifTool.tar.gz")):
                print("Could not remove ExifTool archive")
                return False

            # Replace AppImage AppRun script by own one with advanced features.

            if not utils.copyFile(os.path.join(self.blueprintDir(), "AppRun"), os.path.join(binPath, "AppRun.digiKam")):
                print("Could not copy AppImage startup script")
                return False

        if CraftCore.compiler.isMacOS:
            # --- Manage files under macOS package

            archiveDir = self.archiveDir()
            binPath = os.path.join(archiveDir, "bin")

            # Download exiftool in the bundle

            if not GetFiles.getFile("https://files.kde.org/digikam/exiftool/Image-ExifTool.tar.gz", binPath, "Image-ExifTool.tar.gz"):
                print("Could not get ExifTool archive")
                return False

            if not utils.unpackFile(binPath, "Image-ExifTool.tar.gz", binPath):
                print("Could not unpack ExifTool archive")
                return False

            binfiles = os.listdir(binPath)
            etname = None

            for f in binfiles:
                if f.startswith("Image-ExifTool-"):
                    etname = f
                    break

            if not utils.moveFile(os.path.join(binPath, etname), os.path.join(binPath, "Image-ExifTool")):
                print("Could not rename ExifTool directory")
                return False

            # ExifTool's test suite contains fake executables (t/images/EXE.*),
            # which break the packager's binary processing (patchelf, signing).

            utils.rmtree(os.path.join(binPath, "Image-ExifTool", "t"))

            os.symlink(os.path.join("./", "Image-ExifTool", "exiftool"), os.path.join(self.archiveDir(), "bin", "exiftool"))

            if not utils.deleteFile(os.path.join(binPath, "Image-ExifTool.tar.gz")):
                print("Could not remove ExifTool archive")
                return False

        return True
