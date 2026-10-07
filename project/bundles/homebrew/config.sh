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

# Target macOS architecture: "x86_64" for Intel 64 bits, or "arm64" for Apple Silicon 64 bits.
ARCH_TARGET="`uname -m`"

OSX_MIN_TARGET="11.3"

# Directory to build and install HomeBrew packages.
INSTALL_PREFIX="/opt/homebrew"

echo "Target Architecture: $ARCH_TARGET"

# Directory where target bundle contents will be installed.
RELOCATE_PREFIX="/Applications/digiKam.org"

########################################################################

# URL to git repository to checkout digiKam source code
# git protocol version which require a developer account with ssh keys.
DK_GITURL="git@invent.kde.org:graphics/digikam.git"
#DK_GITURL="git@invent.kde.org:michmill/digiKam.git"
# https protocol version which give annonyous access.
#DK_GITURL="https://invent.kde.org/graphics/digikam.git"

# digiKam tarball information
DK_URL="http://download.kde.org/stable/digikam"

# Location to build source code.
DK_BUILDTEMP=~/dktemp

# Qt version to use in bundle and provided by Homebrew.
DK_QTVERSION="6"

# digiKam tag version from git. Official tarball do not include extra shared libraries.
# The list of tags can be listed with this url: https://invent.kde.org/graphics/digikam/-/tags
# If you want to package current implementation from git, use "master" as tag.
#DK_VERSION=v8.5.0
DK_VERSION=master
#DK_VERSION="work/michmill/koboldllm"

# Installer sub version to differentiates newer updates of the installer itself, even if the underlying application hasn’t changed.
#DK_SUBVER="-01"

# needed to separate Homebrew from Macports
DK_APPLE_PACKAGE_MANAGER="homebrew"

# Installer will include or not digiKam debug symbols
DK_DEBUG=0

# copy debug symbols into ./data/symbols for debugging
DK_COPY_DEBUG_SYMBOLS=1

# Sign bundles with GPG. Passphrase must be hosted in ~/.gnupg/dkorg-gpg-pwd.txt
DK_SIGN=0

# Upload automatically bundle to files.kde.org (pre-release only).
DK_UPLOAD=0
DK_UPLOADURL="digikam@tinami.kde.org"

# KDE frameworks version + Upload URL.
# See official release here: https://download.kde.org/stable/frameworks/

# Use Qt 6.8 LTS for the moment.
MP_QTSUBVERSION="8"

# KDE KF6 frameworks version.
# See official release here: https://download.kde.org/stable/frameworks/
DK_KDE_VERSION="v6.14.0"

# KDE Plasma version.
# See official release here: https://download.kde.org/stable/plasma/
DK_KP_VERSION="v6.3.5"

# KDE Application version.
# See official release here: https://download.kde.org/stable/release-service/
DK_KA_VERSION="v25.04.1"

DK_UPLOADDIR="/srv/archives/files/digikam/unstable"
