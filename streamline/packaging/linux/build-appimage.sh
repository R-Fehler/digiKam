#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Package a digiKam build directory as "digiKam Photos" AppImage.
#
#   streamline/packaging/linux/build-appimage.sh <build-dir> <output-dir> [version]
#
# The build directory must be fully built ("ninja"). Uses linuxdeploy and its
# Qt plugin (downloaded unless LINUXDEPLOY_DIR points to a directory holding
# linuxdeploy-x86_64.AppImage and linuxdeploy-plugin-qt-x86_64.AppImage).
#
# Optional environment:
#   UPDATE_INFORMATION   AppImage update information (embedded, plus .zsync file).
#   WORK_DIR             Where the AppDir is assembled (default: <output-dir>/work).

set -euo pipefail

BUILD_DIR="$(cd "${1:?usage: build-appimage.sh <build-dir> <output-dir> [version]}" && pwd)"
mkdir -p "${2:?usage: build-appimage.sh <build-dir> <output-dir> [version]}"
OUT_DIR="$(cd "${2}" && pwd)"
VERSION="${3:-dev}"

HERE="$(cd "$(dirname "$(readlink -f "${0}")")" && pwd)"
SRC_DIR="$(cd "${HERE}/../../.." && pwd)"
WORK_DIR="${WORK_DIR:-${OUT_DIR}/work}"
APPDIR="${WORK_DIR}/AppDir"
export ARCH="$(uname -m)"        # Also read by appimagetool.

echo "--- Installing into the AppDir"

# KDE's install directories are absolute paths below the configured prefix:
# install with DESTDIR, then move the prefix to usr/.

PREFIX="$(sed -n 's/^CMAKE_INSTALL_PREFIX:PATH=//p' "${BUILD_DIR}/CMakeCache.txt")"

rm -rf "${APPDIR}" "${WORK_DIR}/root"
mkdir -p "${APPDIR}"
DESTDIR="${WORK_DIR}/root" cmake --install "${BUILD_DIR}" > "${WORK_DIR}/install.log"
mv "${WORK_DIR}/root${PREFIX}" "${APPDIR}/usr"
rm -rf "${WORK_DIR}/root"

# digiKam plugins: below the Qt plugin directory of the bundle (usr/plugins,
# see the qt.conf written by linuxdeploy-plugin-qt), where digiKam looks
# for them by default.

PLUGINS_SRC="$(find "${APPDIR}/usr" -type d -path "*/plugins/digikam" | head -n1)"

if [ -z "${PLUGINS_SRC}" ] ; then
    echo "No digiKam plugins found in the install tree" >&2
    exit 1
fi

if [ "${PLUGINS_SRC}" != "${APPDIR}/usr/plugins/digikam" ] ; then
    mkdir -p "${APPDIR}/usr/plugins"
    mv "${PLUGINS_SRC}" "${APPDIR}/usr/plugins/digikam"
fi

# Icon themes: digiKam loads breeze(-dark).rcc from its data directory, like
# the official bundles do.

cp "${SRC_DIR}/project/bundles/common/breeze.rcc"      "${APPDIR}/usr/share/digikam/"
cp "${SRC_DIR}/project/bundles/common/breeze-dark.rcc" "${APPDIR}/usr/share/digikam/"

# ExifTool (Perl, the system's perl runs it), as in the official bundles.

mkdir -p "${WORK_DIR}/exiftool"
curl -sSfL -o "${WORK_DIR}/exiftool/Image-ExifTool.tar.gz" \
     "https://files.kde.org/digikam/exiftool/Image-ExifTool.tar.gz"
tar -xzf "${WORK_DIR}/exiftool/Image-ExifTool.tar.gz" -C "${WORK_DIR}/exiftool"
mv "$(find "${WORK_DIR}/exiftool" -maxdepth 1 -type d -name "Image-ExifTool-*" | head -n1)" \
   "${APPDIR}/usr/bin/Image-ExifTool"
rm -rf "${APPDIR}/usr/bin/Image-ExifTool/t"            # Test suite, with fake binaries.
ln -s Image-ExifTool/exiftool "${APPDIR}/usr/bin/exiftool"
rm -rf "${WORK_DIR}/exiftool"

# Desktop entry of the bundle: Photos mode.

DESKTOP="${APPDIR}/usr/share/applications/org.kde.digikam.photos.desktop"
cp "${HERE}/org.kde.digikam.photos.desktop" "${DESKTOP}"

# Not needed in the bundle.

rm -rf "${APPDIR}/usr/include" "${APPDIR}/usr/lib/cmake" "${APPDIR}/usr/lib/"*/cmake
rm -f  "${APPDIR}/usr/share/applications/org.kde.showfoto.desktop"

echo "--- Collecting dependencies"

TOOLS="${LINUXDEPLOY_DIR:-${WORK_DIR}/tools}"
mkdir -p "${TOOLS}"

for tool in linuxdeploy linuxdeploy-plugin-qt ; do
    if [ ! -x "${TOOLS}/${tool}-${ARCH}.AppImage" ] ; then
        curl -sSfL -o "${TOOLS}/${tool}-${ARCH}.AppImage" \
             "https://github.com/linuxdeploy/${tool}/releases/download/continuous/${tool}-${ARCH}.AppImage"
        chmod +x "${TOOLS}/${tool}-${ARCH}.AppImage"
    fi
done

export PATH="${TOOLS}:${PATH}"
export APPIMAGE_EXTRACT_AND_RUN=1          # No FUSE needed (containers, CI runners).

# The Qt plugin finds Qt through qmake.

if [ -z "${QMAKE:-}" ] ; then
    for candidate in qmake6 /usr/lib/qt6/bin/qmake "/usr/lib/${ARCH}-linux-gnu/qt6/bin/qmake" qmake ; do
        if command -v "${candidate}" > /dev/null 2>&1 ; then
            QMAKE="$(command -v "${candidate}")"
            break
        fi
    done
fi

export QMAKE
export QML_SOURCES_PATHS="${SRC_DIR}/core/app/photos/qml"
export EXTRA_QT_MODULES="svg;"
export LD_LIBRARY_PATH="$(find "${APPDIR}/usr" -name "libdigikamcore.so*" -printf "%h\n" | head -n1):${LD_LIBRARY_PATH:-}"

# Plugins are loaded at run time: their libraries have to be collected explicitly.

DEPLOY_ARGS=()

while IFS= read -r -d '' plugin ; do
    DEPLOY_ARGS+=("--deploy-deps-only=${plugin}")
done < <(find "${APPDIR}/usr/plugins/digikam" -name "*.so" -print0)

export LDAI_OUTPUT="digiKam-Photos-${VERSION}-${ARCH}.AppImage"

if [ -n "${UPDATE_INFORMATION:-}" ] ; then
    export LDAI_UPDATE_INFORMATION="${UPDATE_INFORMATION}"
fi

cd "${OUT_DIR}"

"linuxdeploy-${ARCH}.AppImage" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/digikam" \
    --desktop-file "${DESKTOP}" \
    --custom-apprun "${HERE}/AppRun" \
    "${DEPLOY_ARGS[@]}" \
    --plugin qt \
    --output appimage

echo "--- Done: ${OUT_DIR}/${LDAI_OUTPUT}"
