#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

set(QT_DEFAULT_MAJOR_VERSION 6)
find_package(Qt6 ${QT6_MIN_VERSION} REQUIRED COMPONENTS Core)

set(QT_VERSION       ${Qt6Core_VERSION})
set(QT_MIN_VERSION   ${QT6_MIN_VERSION})
set(QT_VERSION_MAJOR 6)

message(STATUS "Suitable Qt6 >= ${QT_MIN_VERSION} detected: '${QT_VERSION}'.")
