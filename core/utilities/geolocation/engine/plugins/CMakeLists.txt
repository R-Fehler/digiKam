#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include(CMakeParseArguments)

# -------------------------------------------------------------------------

# This macro implement the rules to compile and link a marble plugin with extra arguments.
#
# Usage: marble_add_plugin(NAME    _plugin_name_
#                          SOURCE  _plugin_sources_
#                          DEPENDS _plugin_dependencies_)
#
# With: _plugin_name_ the literal name of the plugin (mandatory).
#       _plugin_sources_ the list of source codes to compile (mandatory).
#       _plugin_dependencies_ the list of dependencies to link (optional).
#
macro(marble_add_plugin)

    set(_OPTIONS_ARGS)
    set(_ONE_VALUE_ARGS)
    set(_MULTI_VALUE_ARGS NAME SOURCES DEPENDS)

    cmake_parse_arguments(_parse_results "${_OPTIONS_ARGS}"
                                         "${_ONE_VALUE_ARGS}"
                                         "${_MULTI_VALUE_ARGS}"
                                         ${ARGN}
    )

    # Mandatory
    if(NOT _parse_results_NAME)
        message(FATAL_ERROR "Marble plugin name is required.")
    endif()

    if(NOT _parse_results_SOURCES)
        message(FATAL_ERROR "Marble plugin sources is required.")
    endif()

    if(APPLE)
        set(_extra_deps /System/Library/Frameworks/AppKit.framework)
    endif()

    add_library(${_parse_results_NAME}
                MODULE ${_parse_results_SOURCES})

    target_link_libraries(${_parse_results_NAME}

                          PRIVATE

                          digikamcore

                          Qt6::Core
                          Qt6::Gui
                          Qt6::Xml
                          Qt6::Widgets
                          Qt6::Network
                          Qt6::Svg

                          KF6::I18n

                          PNG::PNG                          # For zlib

                          ${_parse_results_DEPENDS}
                          ${_extra_deps}
    )

    if(Qt6_FOUND)

        target_link_libraries(${_parse_results_NAME}

                              PRIVATE

                              Qt6::SvgWidgets
        )

    endif()

    install(TARGETS ${_parse_results_NAME}
            DESTINATION ${KDE_INSTALL_FULL_PLUGINDIR}/digikam/marble
    )

endmacro()

add_subdirectory(render)
add_subdirectory(runner)
