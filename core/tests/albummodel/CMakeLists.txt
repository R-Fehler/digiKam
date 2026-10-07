#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include_directories(
    $<TARGET_PROPERTY:Qt6::Xml,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Test,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Sql,INTERFACE_INCLUDE_DIRECTORIES>

    $<TARGET_PROPERTY:KF6::XmlGui,INTERFACE_INCLUDE_DIRECTORIES>
)

add_executable(albummodel_utest ${CMAKE_CURRENT_SOURCE_DIR}/albummodel_utest.cpp)

target_link_libraries(albummodel_utest

                      digikamcore
                      digikamdatabase
                      digikamgui
                      libmodeltest

                      ${COMMON_TEST_LINK}
)

# TODO: this unit test needs to be re-written, since it depends on database,
# threading etc, therefore it is not predictable and very hard to fix.
# Only a command line version is provided for the moment.
#
#ecm_add_tests(${CMAKE_CURRENT_SOURCE_DIR}/albummodel_utest.cpp
#
#              NAME_PREFIX
#
#              "digikam-"
#
#              LINK_LIBRARIES
#
#              digikamcore
#              digikamdatabase
#              digikamgui
#
#              libmodeltest
#
#              ${COMMON_TEST_LINK}
#)
