#!/bin/bash

# SPDX-FileCopyrightText: 2008-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copy this script on root folder where are source code

#export VERBOSE=1

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

CLANG_INSTALL_DIR=/opt/clazy

#export Options='-DCLAZY_AST_MATCHERS_CRASH_WORKAROUND=ON \
#                -DLINK_CLAZY_TO_LLVM=ON \

export PATH=$CLANG_INSTALL_DIR/bin/:$PATH

/opt/qt6/bin/cmake -G "Unix Makefiles" \
      -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_DIR \
      -DAPPIMAGE_HACK=OFF \
      -DCLAZY_MAN_PAGE=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -Wno-dev \
      ..
