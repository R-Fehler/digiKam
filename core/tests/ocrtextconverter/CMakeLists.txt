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
                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::XmlGui,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::Service,INTERFACE_INCLUDE_DIRECTORIES>
)

add_executable(ocr_tesseract_cli  ${CMAKE_CURRENT_SOURCE_DIR}/ocr_tesseract_cli.cpp)
target_link_libraries(ocr_tesseract_cli

                      textconverterbackend
                      digikamcore

                      ${COMMON_TEST_LINK}
)

add_executable(ocr_tesseract_process   ${CMAKE_CURRENT_SOURCE_DIR}/ocr_tesseract_process_cli.cpp)
target_link_libraries(ocr_tesseract_process

                      textconverterbackend
                      digikamcore

                      ${COMMON_TEST_LINK}
)

add_executable(tesseract_version ${CMAKE_CURRENT_SOURCE_DIR}/tesseractversion_cli.cpp)
target_link_libraries(tesseract_version

                      textconverterbackend
                      digikamcore

                      ${COMMON_TEST_LINK}
)
