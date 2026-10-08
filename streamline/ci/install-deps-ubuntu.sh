#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Install the build dependencies of digiKam (with Photos mode) on Ubuntu 24.04,
# from prebuilt packages only (see streamline/docs/02-building.md):
#  - Qt 6, KDE Frameworks 6 and OpenCV 4 from the KDE neon "user" repository,
#  - Exiv2 0.28 from the Ubuntu archive (24.04 ships 0.27).
#
# Used by the GitHub Actions workflow; also works on a developer machine.

set -euo pipefail

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

export DEBIAN_FRONTEND=noninteractive

${SUDO} apt-get update
${SUDO} apt-get install -y --no-install-recommends ca-certificates curl gnupg

curl -sSfL https://archive.neon.kde.org/public.key | ${SUDO} gpg --batch --yes --dearmor -o /usr/share/keyrings/neon.gpg
echo "deb [signed-by=/usr/share/keyrings/neon.gpg] https://archive.neon.kde.org/user noble main" |
    ${SUDO} tee /etc/apt/sources.list.d/neon.list > /dev/null

${SUDO} apt-get update
${SUDO} apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build ccache file patchelf desktop-file-utils \
    kf6-extra-cmake-modules qt6-base-dev qt6-base-private-dev qt6-base-dev-tools qt6-declarative-dev \
    qt6-webengine-dev qt6-networkauth-dev qt6-svg-dev qt6-scxml-dev qt6-multimedia-dev \
    qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-image-formats-plugins \
    libkf6xmlgui-dev libkf6coreaddons-dev libkf6config-dev libkf6service-dev \
    libkf6windowsystem-dev libkf6solid-dev libkf6i18n-dev libkf6iconthemes-dev \
    libkf6notifications-dev libkf6notifyconfig-dev libkf6threadweaver-dev \
    libkf6sonnet-dev libkf6kio-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtqml-workerscript \
    qml6-module-qtquick-templates qml6-module-qtqml-models \
    libopencv-dev liblcms2-dev libjpeg-dev libtiff-dev libpng-dev libboost-dev \
    libexpat1-dev libeigen3-dev libheif-dev libx265-dev libxml2-dev libxslt1-dev \
    liblensfun-dev flex bison gettext pkg-config \
    libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev \
    libswscale-dev libswresample-dev libgl-dev libglu1-mesa-dev \
    libinireader0

# Exiv2 0.28: the 0.27 code path of this digiKam snapshot does not compile.

EXIV2_VERSION="0.28.5+dfsg-1"
WORK="$(mktemp -d)"

for f in "libexiv2-28_${EXIV2_VERSION}_amd64.deb" \
         "libexiv2-dev_${EXIV2_VERSION}_amd64.deb" \
         "libexiv2-data_${EXIV2_VERSION}_all.deb" ; do
    curl -sSfL -o "${WORK}/${f}" "https://archive.ubuntu.com/ubuntu/pool/main/e/exiv2/${f}"
done

${SUDO} apt-get remove -y libexiv2-dev || true
${SUDO} dpkg -i --auto-deconfigure "${WORK}"/libexiv2-*.deb
${SUDO} dpkg -r libexiv2-27 || true
rm -rf "${WORK}"
