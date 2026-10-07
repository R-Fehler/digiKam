#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

find_package(Qt6
             REQUIRED
             NO_MODULE COMPONENTS

             Core
             Concurrent
             Widgets
             Gui
             Sql
             Xml
             PrintSupport
             Network
             NetworkAuth
             Svg
             WebEngineWidgets
             StateMachine
             SvgWidgets
)

find_package(Qt6
             OPTIONAL_COMPONENTS

             DBus
             OpenGL
             OpenGLWidgets
)

if(ENABLE_DBUS)

    if(NOT Qt6DBus_FOUND)

        set(ENABLE_DBUS OFF)

    endif()

endif()

# Qt Dependencies For unit tests and CLI test tools

if(BUILD_TESTING)

    find_package(Qt6
                 REQUIRED
                 NO_MODULE COMPONENTS

                 Test
    )

endif()
