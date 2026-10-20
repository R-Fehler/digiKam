#!/bin/bash
#
# SPDX-FileCopyrightText: 2025 Ben Cooksley <bcooksley@kde.org>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Prepare an almalinux:9 container like KDE's Craft AppImage CI image
# (sysadmin/ci-images craft-appimage-alma9/build.sh), so that KDE's prebuilt
# Craft packages (built there) can be used. The parts only needed on KDE's
# GitLab runners (gitlab-runner, signing tools) are left out.
#
# AlmaLinux 9 has glibc 2.34: the resulting AppImage runs on Ubuntu 22.04,
# Debian 12, Fedora 36 and newer.

set -e

# CodeReady Builder (CRB) repo, needed at least for some xcb-util-*-devel.
dnf install -y 'dnf-command(config-manager)'
dnf config-manager --set-enabled crb

dnf update -y

packages=(
    # core tools
    git-core
    git-lfs
    cmake
    gcc-toolset-14
    python3.11-devel
    python3.11-pip
    sqlite
    zlib
    which
    xz
    # Craft runs linuxdeploy as an AppImage
    fuse
    # Qt
    at-spi2-atk-devel
    mesa-vulkan-drivers vulkan-headers wayland-devel mesa-libGL-devel
    libX11-devel libX11-xcb libxcb-devel libxkbcommon-x11-devel libxkbcommon-devel libXi-devel
    libXcomposite-devel libXcursor-devel libXrandr-devel libXtst-devel
    xcb-util-devel
    xcb-util-cursor xcb-util-cursor-devel
    xcb-util-keysyms xcb-util-keysyms-devel
    xcb-util-renderutil xcb-util-renderutil-devel
    xcb-util-wm xcb-util-wm-devel
    xcb-util-image xcb-util-image-devel
    xorg-x11-util-macros
    flex bison gperf
    libicu-devel
    ruby
    patch
    file
    alsa-lib-devel
    nspr-devel nss-devel
    # Qt WebEngine
    libxkbfile-devel libdrm-devel libXdamage-devel libxshmfence-devel gcc-toolset-14-libatomic-devel
    xorg-x11-proto-devel
    mesa-libgbm-devel
    # KDE Frameworks: Solid (libudev)
    systemd-devel
    # KDE Frameworks: KIO
    libmount-devel
    # KDE Frameworks: KWayland
    mesa-libEGL-devel
    # AppImage AppStream metadata
    appstream-devel
)
dnf install -y "${packages[@]}"

# PowerShell: used by Craft as a download helper.

if [ "$(arch)" == "x86_64" ]; then
    pwshArch="x64"
else
    pwshArch="arm64"
fi

curl -sSfL -o /tmp/powershell.tar.gz \
     "https://github.com/PowerShell/PowerShell/releases/download/v7.4.0/powershell-7.4.0-linux-${pwshArch}.tar.gz"
mkdir -p /opt/microsoft/powershell/7
tar zxf /tmp/powershell.tar.gz -C /opt/microsoft/powershell/7
rm /tmp/powershell.tar.gz
chmod +x /opt/microsoft/powershell/7/pwsh
ln -sf /opt/microsoft/powershell/7/pwsh /usr/bin/pwsh

git config --system merge.defaultToUpstream true

# The source tree is mounted from the host, owned by another user.
git config --system --add safe.directory '*'

dnf clean all
