#
# SPDX-FileCopyrightText: 2015-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

# Libtiff generate warnings about deprecated legacy uint32, uint16, uint8 types.
# Remove these lines when Tiff loader will uses modern types instead.
REMOVE_COMPILER_WARNINGS("-Wdeprecated-declarations")
DISABLE_GCC_COMPILER_WARNINGS("5.99.99" "-Wno-deprecated-declarations")
DISABLE_CLANG_COMPILER_WARNINGS("3.99.99" "-Wno-deprecated-declarations")


include(MacroDPlugins)

include_directories($<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
)

set(dimgtiffplugin_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dimgtiffplugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dimgtiffloader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dimgtiffloader_load.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dimgtiffloader_save.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dimgtiffexportsettings.cpp
)

DIGIKAM_ADD_DIMG_PLUGIN(NAME    TIFF
                        SOURCES ${dimgtiffplugin_SRCS}
                        DEPENDS ${TIFF_LIBRARIES}
)
