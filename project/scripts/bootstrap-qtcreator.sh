#!/bin/bash

# SPDX-FileCopyrightText: 2008-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copy this script on root folder where are source code

#export VERBOSE=1

# We will work on command line using MinGW compiler
export MAKEFILES_TYPE='Unix Makefiles'

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

export Options='-DWITH_TESTS=OFF \
                -DWITH_DEBUG_CMAKE=OFF \
                -DSHOW_BUILD_DATE=ON \
                -DWITH_SANITIZE=OFF \
                -DBUILD_DESIGNSTUDIO=OFF \
                -DBUILD_WITH_PCH=OFF \
                -DQTC_USE_INTERNAL_TASKTREE=ON \
                -DENABLE_SVG_SUPPORT=ON \
                -DBUILD_EXECUTABLE_CMDBRIDGE=OFF \
'

/opt/qt6/bin/cmake -G "$MAKEFILES_TYPE" \
      -DCMAKE_INSTALL_PREFIX=/opt/qtcreator \
      -DCMAKE_BUILD_TYPE=Release \
      -Wno-dev \
      $Options \
      .. \

