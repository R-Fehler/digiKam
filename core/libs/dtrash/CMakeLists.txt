#
# SPDX-FileCopyrightText: 2015 by Mohamed_Anwer, <m_dot_anwer at gmx dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

set(libdtrash_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dtrash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dtrashiteminfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dtrashitemmodel.cpp
)

include_directories(
    $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>

    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
)

# Used by digikamgui
add_library(gui_dtrash_obj OBJECT ${libdtrash_SRCS})

target_compile_definitions(gui_dtrash_obj
                           PRIVATE
                           digikamgui_EXPORTS
)
