#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2015      by Veaceslav Munteanu, <veaceslav dot munteanu90 at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

set(libtemplate_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/templatemanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templatelist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templateselector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templateviewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/templatepanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/subjectedit.cpp
)

include_directories(
    $<TARGET_PROPERTY:Qt6::Xml,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:KF6::WidgetsAddons,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
)

# Used by digikamgui
add_library(gui_digikamtemplate_obj OBJECT  ${libtemplate_SRCS})

target_compile_definitions(gui_digikamtemplate_obj
                           PRIVATE
                           digikamgui_EXPORTS
)
