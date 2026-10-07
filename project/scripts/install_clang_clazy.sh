#!/bin/bash

# SPDX-FileCopyrightText: 2008-2025 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Script to compile and install clang for clazy static analyzer

CLANG_SRC_DIR=./clang_clazy_src
CLANG_BUILD_DIR=./clang_clazy_build     # Important that this is outside of the source directory
CLANG_INSTALL_DIR=/opt/clazy
CLANG_VERSION=llvmorg-19.1.7
CLANG_SHARED_LIB=$CLANG_INSTALL_DIR/lib/libclang.so
CMAKE_BINARY=/opt/qt6/bin/cmake

ARCH=$(uname -m)
case "$ARCH" in
    x86_64)        CLANG_LLVM_TARGET="X86" ;;
    aarch64|arm64) CLANG_LLVM_TARGET="AArch64" ;;
    armv7l|armv6l) CLANG_LLVM_TARGET="ARM" ;;
    *) echo "Architecture not supported: $ARCH"; exit 1 ;;
esac

echo "Clang Target Architecture: $CLANG_LLVM_TARGET"

if [ ! -d $CLANG_SRC_DIR ] ; then

    git clone https://github.com/llvm/llvm-project.git -b $CLANG_VERSION $CLANG_SRC_DIR

fi

if [ -d "$CLANG_BUILD_DIR" ]; then

    rm -fr $CLANG_BUILD_DIR

fi

mkdir $CLANG_BUILD_DIR
cd $CLANG_BUILD_DIR

$CMAKE_BINARY -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_DIR \
              -DLLVM_INCLUDE_EXAMPLES=OFF \
              -DLLVM_TARGETS_TO_BUILD="$CLANG_LLVM_TARGET" \
              -DLLVM_DEFAULT_TARGET_TRIPLE="$ARCH-linux-gnu" \
              -DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON \
              -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
              -DCMAKE_BUILD_TYPE=Release \
              -G "Ninja" \
              ../$CLANG_SRC_DIR/llvm

$CMAKE_BINARY --build . --parallel
sudo $CMAKE_BINARY --build . --target install

if [ -f $CLANG_SHARED_LIB ] ; then

    file $CLANG_SHARED_LIB

else

    echo "ERROR: $CLANG_SHARED_LIB do not exists!"

    exit -1

fi

echo "NOTE: Clang is installed. Add $CLANG_INSTALL_DIR/bin/ to the PATH env. variable..."

exit 0
