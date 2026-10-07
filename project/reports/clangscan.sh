#!/bin/bash

# SPDX-FileCopyrightText: 2013-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Run Clang static analyzer on whole digiKam source code.
# https://clang-analyzer.llvm.org/
# Dependencies : LLVM clang static analyzer version >= 16.
#
# If '--nowebupdate' is passed as argument, static analyzer results are not pushed online at
# https://files.kde.org/digikam/reports/ (default yes).
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Halt and catch errors
set -eE
trap 'PREVIOUS_COMMAND=$THIS_COMMAND; THIS_COMMAND=$BASH_COMMAND' DEBUG
trap 'echo "FAILED COMMAND: $PREVIOUS_COMMAND"' ERR

. ./common.sh

# Analyzer configuration.
. ../../.scan-build

StartScript
checksCPUCores

if [[ ! -d /opt/qt6 ]] ; then

    echo "Qt6 install not present in /opt/qt6."
    echo "Please install Qt6 with https://github.com/cgilles/digikam-install-deps"
    echo "Aborted..."
    exit -1

fi

# Check run-time dependencies

if ! which scan-build ; then

    if ! which scan-build-21 ; then

        echo "SCAN-BUILD tool from LLVM is not installed!"
        echo "See https://clang-analyzer.llvm.org/scan-build.html for details."
        exit -1

    else

        SCAN_BUILD_BIN=scan-build-21

    fi

else

    SCAN_BUILD_BIN=scan-build

fi

echo "Found SCAN_BUILD tool: $SCAN_BUILD_BIN"

ORIG_WD="`pwd`"
REPORT_DIR="report.scan"

# Get active git branches to create report description string
TITLE="digiKam-$(parseGitBranch)$(parseGitHash)"
echo "Clang Scan Static Analyzer task name: $TITLE"

# Clean up and prepare to scan.

rm -fr $ORIG_WD/$REPORT_DIR

cd ../..

rm -fr build.scan
mkdir -p build.scan
cd build.scan

export Qt6_DIR=/opt/qt6
QTPATHS="/opt/qt6/bin/qtpaths6"
export CMAKE_BINARY=/opt/qt6/bin/cmake

$SCAN_BUILD_BIN $CMAKE_BINARY -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_TESTING=ON \
      -DDIGIKAMSC_CHECKOUT_PO=OFF \
      -DDIGIKAMSC_CHECKOUT_DOC=OFF \
      -DENABLE_GUI_TRANSLATIONS=OFF \
      -DDIGIKAMSC_COMPILE_DOC=OFF \
      -DENABLE_KFILEMETADATASUPPORT=ON \
      -DENABLE_AKONADICONTACTSUPPORT=ON \
      -DENABLE_MARIADBSUPPORT=ON \
      -DENABLE_INTERNALMARIADB=ON \
      -DENABLE_MEDIAPLAYER=ON \
      -DENABLE_DBUS=ON \
      -DENABLE_APPSTYLES=ON \
      -DENABLE_GEOLOCATION=ON \
      -DENABLE_NLSEARCH_LLAMACPP=ON \
      -Wno-dev \
      ..

# Print the skipped directories taken from the config file.

for DROP_ITEM in $IGNORE_DIRS ; do

    if [[ $DROP_ITEM != *exclude ]] ; then

        echo "Skipped dir: $DROP_ITEM"

    fi

done

$SCAN_BUILD_BIN -o $ORIG_WD/$REPORT_DIR \
           -v \
           -k \
           -no-failure-reports \
           --keep-empty \
           --show-description \
           --html-title $TITLE \
           $IGNORE_DIRS \
           make -j$CPU_CORES

cd $ORIG_WD

SCAN_BUILD_DIR=$(find "${ORIG_WD}/${REPORT_DIR}" -maxdepth 1 -not -empty -not -name "$(basename "${ORIG_WD}/${REPORT_DIR}")")
echo "Clang Report $TITLE is located to $SCAN_BUILD_DIR"

if [[ $1 != "--nowebupdate" ]] ; then

    cd $ORIG_WD
    updateOnlineReport "clang" $REPORT_DIR $TITLE $(parseGitBranch)

fi

cd $ORIG_WD

rm -fr ../../build.scan

TerminateScript
