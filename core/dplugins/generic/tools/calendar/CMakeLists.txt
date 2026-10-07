#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include(MacroDPlugins)

include_directories($<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Concurrent,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::PrintSupport,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>

                    ${CMAKE_CURRENT_SOURCE_DIR}/print
                    ${CMAKE_CURRENT_SOURCE_DIR}/wizard
)

if(KF6CalendarCore_FOUND)
    include_directories($<TARGET_PROPERTY:KF6::CalendarCore,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

set(calendarplugin_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/print/calsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/calpainter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/calprinter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/print/calsystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/calintropage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/calmonthwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/caltemplate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/calwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wizard/calwizard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/calendarplugin.cpp
)

ki18n_wrap_ui(calendarplugin_SRCS
              ${CMAKE_CURRENT_SOURCE_DIR}/ui/caltemplate.ui
              ${CMAKE_CURRENT_SOURCE_DIR}/ui/calevents.ui
              ${CMAKE_CURRENT_SOURCE_DIR}/ui/calprogress.ui
)

if(KF6CalendarCore_FOUND)

    DIGIKAM_ADD_GENERIC_PLUGIN(NAME    Calendar
                               SOURCES ${calendarplugin_SRCS}
                               DEPENDS KF6::CalendarCore Qt6::PrintSupport
    )

else()

    DIGIKAM_ADD_GENERIC_PLUGIN(NAME    Calendar
                               SOURCES ${calendarplugin_SRCS}
                               DEPENDS Qt6::PrintSupport
    )

endif()
