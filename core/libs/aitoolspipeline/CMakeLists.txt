#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2024-2025 by Michael Miller, <michael underscore miller at msn dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

set(libaitoolspipeline_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/aitoolspipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aitoolspipelinepackage.cpp
)

include_directories($<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
)

# Used by digikamcore
add_library(gui_aitoolspipeline_obj OBJECT ${libaitoolspipeline_SRCS})

target_compile_definitions(gui_aitoolspipeline_obj
                           PRIVATE
                           digikamgui_EXPORTS
)
