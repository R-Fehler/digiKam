#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

include_directories($<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::ConfigCore,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:KF6::Service,INTERFACE_INCLUDE_DIRECTORIES>
)

###

set(libvideotoolscommon_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/vidslidesettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/ffmpegbinary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/ffmpeglauncher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/vidslidetask.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manager/vidslidethread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/osd/frameosd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/osd/frameosdsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/osd/frameosdwidget.cpp
)

if(ENABLE_MEDIAPLAYER)

    set(libvideotoolscommon_SRCS
        ${libvideotoolscommon_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/ffmpegconfighelper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/player/vidplayerdlg.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/player/audplayerwdg.cpp
    )

endif()

# Used by digikamcore
add_library(core_videotoolscommon_obj OBJECT ${libvideotoolscommon_SRCS})

target_compile_definitions(core_videotoolscommon_obj
                           PRIVATE
                           digikamcore_EXPORTS
)

###

if(ENABLE_MEDIAPLAYER)

    set(libvideotools_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/player/mediaplayerview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/player/slidevideo.cpp
    )

    # Used by digikamcore
    add_library(core_videotools_obj OBJECT ${libvideotools_SRCS})

    target_compile_definitions(core_videotools_obj
                               PRIVATE
                               digikamcore_EXPORTS
    )

    target_compile_definitions(core_videotools_obj
                               PRIVATE
                               ${MEDIAPLAYER_DEFINITIONS}
    )

endif()
