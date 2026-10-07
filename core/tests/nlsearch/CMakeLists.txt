#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

include_directories(
    $<TARGET_PROPERTY:Qt6::Test,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>

    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>

    ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/nlsearch
)

set(NLSEARCH_DIR ${CMAKE_SOURCE_DIR}/core/utilities/searchwindow/nlsearch)

set(nlsearchpipeline_utest_SRCS
    nlsearchpipeline_utest.cpp
)

add_executable(nlsearchpipeline_utest ${nlsearchpipeline_utest_SRCS})

target_link_libraries(nlsearchpipeline_utest

                     nlsearch

                     digikamcore
                     digikamdatabase

                     Qt6::Core
                     Qt6::Gui
                     Qt6::Widgets
                     Qt6::Test

                     KF6::I18n

                     ${OpenCV_LIBRARIES}
)

if(HAVE_LLAMACPP)

   target_link_libraries(nlsearchpipeline_utest ${llama_LIBRARY} ${GGML_LIBRARY})

endif()

add_test(NAME nlsearchpipeline_utest COMMAND nlsearchpipeline_utest)
