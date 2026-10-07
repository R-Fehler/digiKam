#!/bin/bash

# SPDX-FileCopyrightText: 2013-2026 by Gilles Caulier  <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

########################################################################

# Absolute path where are downloaded all tarballs to compile.
DOWNLOAD_DIR="`pwd`/temp.dwnld"

# Absolute path where are compiled all tarballs
BUILDING_DIR="`pwd`/temp.build"

########################################################################

# Build universal bundle (arm64 + Intel architecture). Work only with Silicon computer.
MP_UNIVERSAL=0

# Target macOS architecture: "x86_64" for Intel 64 bits, or "arm64" for Apple Silicon 64 bits.
# Used with the macport configuration.
HOST_ARCH="`uname -m`"

# Target bundle architecture. Used to configure CMake target and create prefix for the log files
# and the Macports install directory.
# Equal: "x86_64" for Intel 64 bits, "arm64" for Apple Silicon 64 bits, or "universal for both architectures.
TARGET_ARCH=$HOST_ARCH

# Note: Apple Silicon is supported since macOS BigSur
OSX_MIN_TARGET="11.3"

echo "Host Architecture  : $HOST_ARCH"
echo "Target Architecture: $TARGET_ARCH"

# Directory to build and install Macports packages.
INSTALL_PREFIX="/opt/digikam.org.$TARGET_ARCH"
# Local install prefix which do not require sudo right
#INSTALL_PREFIX="`pwd`/digikam.org.$TARGET_DIR_SUF"

# Directory where target bundle contents will be installed.
RELOCATE_PREFIX="/Applications/digiKam.org"

# Macports configuration
MP_URL="https://distfiles.macports.org/MacPorts/"
MP_BUILDTEMP=~/mptemp

# Uncomment this line to force a specific version of Macports to use, else latest will be used.
#MP_VERSION="2.3.3"

########################################################################

# URL to git repository to checkout digiKam source code
# git protocol version which require a developer account with ssh keys.
DK_GITURL="git@invent.kde.org:graphics/digikam.git"
# https protocol version which give annonyous access.
#DK_GITURL="https://invent.kde.org/graphics/digikam.git"

# digiKam tarball information
DK_URL="http://download.kde.org/stable/digikam"

# Location to build source code.
DK_BUILDTEMP=~/dktemp

# Qt version to use in bundle and provided by Macports.
DK_QTVERSION="6"

# Mariadb version to install for Qt SQL plugin.
DK_MARIADB_VERSION="10.11"

MP_MARIADB_VARIANT="+mariadb$DK_MARIADB_VERSION"
MP_MARIADB_VARIANT=${MP_MARIADB_VARIANT//./_}
MARIADB_SUFFIX="-$DK_MARIADB_VERSION"

# digiKam tag version from git. Official tarball do not include extra shared libraries.
# The list of tags can be listed with this url: https://invent.kde.org/graphics/digikam/-/tags
# If you want to package current implementation from git, use "master" as tag.
#DK_VERSION=v8.4.0
DK_VERSION=master
#DK_VERSION=development/dplugins

# Installer sub version to differentiates newer updates of the installer itself, even if the underlying application hasn’t changed.
#DK_SUBVER="-01"

# needed to separate Macports from Homebrew
DK_APPLE_PACKAGE_MANAGER="macports"

# Installer will include or not digiKam debug symbols
DK_DEBUG=0

# Sign bundles with GPG. Passphrase must be hosted in ~/.gnupg/dkorg-gpg-pwd.txt
DK_SIGN=0

# Upload automatically bundle to files.kde.org (pre-release only).
DK_UPLOAD=1
DK_UPLOADURL="digikam@tinami.kde.org"

# KDE frameworks version + Upload URL.
# See official release here: https://download.kde.org/stable/frameworks/

# KDE KF6 frameworks version.
# See official release here: https://download.kde.org/stable/frameworks/
DK_KDE_VERSION="v6.28.0"

# KDE Plasma version.
# See official release here: https://download.kde.org/stable/plasma/
DK_KP_VERSION="v6.7.4"

# KDE Application version.
# See official release here: https://download.kde.org/stable/release-service/
DK_KA_VERSION="v26.04.3"

DK_UPLOADDIR="/srv/archives/files/digikam/"
