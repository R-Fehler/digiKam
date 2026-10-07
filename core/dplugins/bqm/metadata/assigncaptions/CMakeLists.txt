#
# SPDX-FileCopyrightText: 2015-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include(MacroDPlugins)

include_directories($<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
)

set(assigncaptionsplugin_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/assigncaptionsplugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/assigncaptions.cpp
)

DIGIKAM_ADD_BQM_PLUGIN(NAME    Assigncaptions
                       SOURCES ${assigncaptionsplugin_SRCS}
)
