#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Run digiKam straight from a build directory, without "ninja install".
#
#   streamline/scripts/run-dev.sh <build-dir> [digikam arguments...]
#
# Example:
#   streamline/scripts/run-dev.sh ~/build/digikam --photos
#
# Needs the "digikam" target and the DImg loader plugins built:
#   ninja digikam DImg_JPEG_Plugin DImg_PNG_Plugin DImg_TIFF_Plugin DImg_RAW_Plugin \
#         DImg_HEIF_Plugin DImg_PGF_Plugin DImg_QImage_Plugin

set -euo pipefail

BUILD_DIR="$(cd "${1:?usage: run-dev.sh <build-dir> [args]}" && pwd)"
shift

STAGE="${BUILD_DIR}/streamline-stage"

# Runtime data (database schema, AI model list, icons...) is looked up through
# the XDG data dirs: install only the data directory into a staging area.

if [ ! -f "${STAGE}/.data-installed" ] || [ "${BUILD_DIR}/core/data/cmake_install.cmake" -nt "${STAGE}/.data-installed" ] ; then
    DESTDIR="${STAGE}/root" cmake -P "${BUILD_DIR}/core/data/cmake_install.cmake" > /dev/null
    touch "${STAGE}/.data-installed"
fi

DATA_DIR="$(dirname "$(find "${STAGE}/root" -type d -path "*/share/digikam" | head -n1)")"
ln -sfn "${BUILD_DIR}/core/data/database/dbconfig.xml" "${DATA_DIR}/digikam/database/dbconfig.xml"

# Plugins are searched recursively below DK_PLUGIN_PATH.

mkdir -p "${STAGE}/plugins"

find "${BUILD_DIR}" -path "${STAGE}" -prune -o -name "DImg_*_Plugin.so" -print \
     -o -name "Generic_*_Plugin.so" -print -o -name "Editor_*_Plugin.so" -print 2>/dev/null |
while read -r plugin ; do
    ln -sf "${plugin}" "${STAGE}/plugins/$(basename "${plugin}")"
done

export XDG_DATA_DIRS="${DATA_DIR}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export DK_PLUGIN_PATH="${STAGE}/plugins"

# Print Qt / digiKam logs on the terminal (Qt uses the system journal otherwise).
export QT_FORCE_STDERR_LOGGING=1

DIGIKAM_BIN="$(find "${BUILD_DIR}" -path "${STAGE}" -prune -o -type f -name digikam -perm -u+x -print | head -n1)"

exec "${DIGIKAM_BIN}" "$@"
