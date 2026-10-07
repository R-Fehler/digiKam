#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

if(ENABLE_MEDIAPLAYER)

    # NOTE: For Qt6, QtMultimedia based on FFMpeg is only supported with Qt version >= 6.5.

    find_package(Qt6
                 OPTIONAL_COMPONENTS
                 Multimedia
                 MultimediaWidgets
    )

    include_directories($<TARGET_PROPERTY:Qt6::Multimedia,INTERFACE_INCLUDE_DIRECTORIES>
                        $<TARGET_PROPERTY:Qt6::MultimediaWidgets,INTERFACE_INCLUDE_DIRECTORIES>)


    if (Qt6Multimedia_VERSION VERSION_GREATER_EQUAL 6.5.0)

        message(STATUS "MediaPlayer type:     Qt6::Multimedia")

    else()

        set(ENABLE_MEDIAPLAYER OFF)
        message(STATUS "MediaPlayer type:     None")

    endif()

endif()
