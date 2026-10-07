#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2015      by Veaceslav Munteanu, <veaceslav dot munteanu90 at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

# To fill MacOS and Windows bundles metadata

set(BUNDLE_APP_NAME_STRING          "digikam")
set(BUNDLE_APP_DESCRIPTION_STRING   "Advanced digital photo management application")
set(BUNDLE_LEGAL_COPYRIGHT_STRING   "GNU Public License V2")
set(BUNDLE_COMMENT_STRING           "Free and open source software to manage photo")
set(BUNDLE_LONG_VERSION_STRING      ${DIGIKAM_VERSION_STRING})
set(BUNDLE_SHORT_VERSION_STRING     ${DIGIKAM_VERSION_SHORT})
set(BUNDLE_VERSION_STRING           ${DIGIKAM_VERSION_STRING})

# digiKam executable

set(digikam_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main/main.cpp
)

# Set the application icon on the application

file(GLOB ICONS_SRCS "${CMAKE_SOURCE_DIR}/core/data/icons/apps/*-apps-digikam.png")

if(WIN32)

    # Build the main implementation into a DLL to be called by a stub EXE.
    # This is a work around "command line is too long" issue on Windows.
    # see https://stackoverflow.com/questions/43184251/cmake-command-line-too-long-windows

    add_library(digikam SHARED ${digikam_SRCS})
    set_target_properties(digikam PROPERTIES PREFIX "")

elseif(APPLE)

    ecm_add_app_icon(digikam_SRCS ICONS ${ICONS_SRCS})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/templates/DigikamInfo.plist.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
    add_executable(digikam ${digikam_SRCS})
    set_target_properties(digikam PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

else()

    ecm_add_app_icon(digikam_SRCS ICONS ${ICONS_SRCS})
    add_executable(digikam ${digikam_SRCS})

endif()

target_include_directories(digikam
                           PRIVATE
                           ${DIGIKAM_TARGET_INCLUDES}
)

target_link_libraries(digikam

                      PUBLIC

                      Qt6::Core
                      Qt6::Gui
                      Qt6::Widgets
                      Qt6::Sql

                      KF6::WindowSystem
                      KF6::I18n
                      KF6::XmlGui
                      KF6::ConfigCore
                      KF6::Service
                      KF6::CoreAddons

                      digikamcore
                      digikamdatabase
                      digikamgui
)

if(ENABLE_DBUS)

    target_link_libraries(digikam
                          PUBLIC
                          Qt6::DBus
    )

endif()

if(KF6IconThemes_FOUND)

    target_link_libraries(digikam
                          PUBLIC
                          KF6::IconThemes
                          KF6::IconWidgets
    )

endif()

if(KF6KIO_FOUND)

    target_link_libraries(digikam
                          PUBLIC
                          KF6::KIOWidgets
    )

endif()

if(ImageMagick_Magick++_FOUND)

    target_link_libraries(digikam
                          PUBLIC
                          ${ImageMagick_LIBRARIES}
    )

endif()

install(TARGETS digikam DESTINATION ${KDE_INSTALL_BINDIR})

if(APPLE)
    install(FILES "$<TARGET_FILE:digikam>.dSYM" DESTINATION "${CMAKE_INSTALL_BINDIR}" CONFIGURATIONS Debug RelWithDebInfo)
endif()

if(WIN32)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/templates/versioninfo.rc.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc)

    set(digikam_windows_stub_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/main/windows_stub_main.cpp)

    ecm_add_app_icon(digikam_windows_stub_SRCS ICONS ${ICONS_SRCS})

    add_executable(digikam_windows_stub_exe
                   ${digikam_windows_stub_SRCS}
                   ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc
    )

    target_link_libraries(digikam_windows_stub_exe
                          PRIVATE
                          digikam)

    set_target_properties(digikam_windows_stub_exe PROPERTIES OUTPUT_NAME "digikam")
    target_include_directories(digikam_windows_stub_exe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main)

    install(TARGETS digikam_windows_stub_exe ${INSTALL_TARGETS_DEFAULT_ARGS})

endif()

if(WIN32)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/templates/digikam.exe.manifest.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/digikam.exe.manifest)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/digikam.exe.manifest DESTINATION ${KDE_INSTALL_BINDIR})

endif()
