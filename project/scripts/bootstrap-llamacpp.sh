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

export Options='-DLLAMA_BUILD_TOOLS=ON \
                -DLLAMA_BUILD_SERVER=ON \
                -DLLAMA_BUILD_APP=ON \
                -DLLAMA_BUILD_COMMON=ON \
                -DGGML_NATIVE=ON \
                -DBUILD_SHARED_LIBS=ON \
                -DLLAMA_BUILD_TESTS=OFF \
                -DLLAMA_BUILD_EXAMPLES=OFF \
                -DLLAMA_CURL=OFF \
                -DGGML_CUDA=OFF \
                -DGGML_METAL=OFF \
                -DGGML_VULKAN=OFF'

cmake -G "$MAKEFILES_TYPE" . \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -Wno-dev \
      $Options \
      ..

