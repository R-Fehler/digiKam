#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include_directories(
                    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Network,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Concurrent,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Xml,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::XmlGui,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::Service,INTERFACE_INCLUDE_DIRECTORIES>
)

# -----------------------------------------------------------------------------

set(detect_object_cli_SRCS  ${CMAKE_CURRENT_SOURCE_DIR}/detect_object_cli.cpp)

add_executable(detect_object_cli ${detect_object_cli_SRCS})

target_link_libraries(detect_object_cli

                      digikamcore
                      digikamdatabase
                      digikamgui

                      KF6::XmlGui
                      KF6::ConfigCore
                      KF6::Service
                      ${COMMON_TEST_LINK}
)

# -----------------------------------------------------------------------------

set(detect_object_gui_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/detect_object_gui.cpp)

add_executable(detect_object_gui ${detect_object_gui_SRCS})

target_link_libraries(detect_object_gui

                      digikamcore
                      digikamdatabase
                      digikamgui

                      KF6::XmlGui
                      KF6::ConfigCore
                      KF6::Service
                      ${COMMON_TEST_LINK}
)

# -----------------------------------------------------------------------------

set(detect_object_batch_cli_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/detect_object_batch_cli.cpp)
add_executable(detect_object_batch_cli ${detect_object_batch_cli_SRCS})
target_link_libraries(detect_object_batch_cli

                      digikamcore
                      digikamdatabase
                      digikamgui

                      KF6::XmlGui
                      KF6::ConfigCore
                      KF6::Service
                      ${COMMON_TEST_LINK}
)



# -----------------------------------------------------------------------------

set(autotagging_batch_cli_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/autotagging_batch_cli.cpp)
add_executable(autotagging_batch_cli ${autotagging_batch_cli_SRCS})
target_link_libraries(autotagging_batch_cli

                      digikamcore
                      digikamdatabase
                      digikamgui

                      KF6::XmlGui
                      KF6::ConfigCore
                      KF6::Service
                      ${COMMON_TEST_LINK}
)

# -----------------------------------------------------------------------------

set(resnet_cli_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/resnet_cli.cpp)
add_executable(resnet_cli ${resnet_cli_SRCS})
target_link_libraries(resnet_cli

                      digikamcore
                      digikamdatabase
                      digikamgui

                      KF6::XmlGui
                      KF6::ConfigCore
                      KF6::Service
                      ${COMMON_TEST_LINK}
)
