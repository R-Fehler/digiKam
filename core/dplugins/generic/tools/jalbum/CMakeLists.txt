#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include(MacroDPlugins)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools

                    $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>

                    ${CMAKE_CURRENT_SOURCE_DIR}/generator
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools
                    ${CMAKE_CURRENT_SOURCE_DIR}/wizard
)

set(jalbumplugin_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/jalbumsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/generator/jalbumgenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/jalbumintropage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/jalbumfinalpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/jalbumoutputpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/jalbumselectionpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/jalbumwizard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/jalbumjar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/jalbumjava.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jalbumplugin.cpp
)

DIGIKAM_ADD_GENERIC_PLUGIN(NAME    JAlbum
                           SOURCES ${jalbumplugin_SRCS}
)
