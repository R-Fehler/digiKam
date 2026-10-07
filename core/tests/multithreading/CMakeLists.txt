#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include_directories(
    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Test,INTERFACE_INCLUDE_DIRECTORIES>
)

#------------------------------------------------------------------------

ecm_add_tests(${CMAKE_CURRENT_SOURCE_DIR}/actionjob_utest.cpp

              NAME_PREFIX

              "digikam-"

              LINK_LIBRARIES

              digikamcore

              ${COMMON_TEST_LINK}
)

#------------------------------------------------------------------------

set(multicorerawtopng_gui_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/processordlg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rawtopngconverterthread.cpp
)

add_executable(multicorerawtopng_gui ${multicorerawtopng_gui_SRCS})

target_link_libraries(multicorerawtopng_gui

                      digikamcore

                      ${COMMON_TEST_LINK}
)
