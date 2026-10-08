#
# SPDX-License-Identifier: BSD-3-Clause
#
# Photos mode: streamlined Qt Quick front-end ("digikam --photos").
# Included from DigikamGuiTarget.cmake once the digikamgui target exists.
# Without Qt Quick the stock digiKam build is left untouched.

find_package(Qt6 ${QT6_MIN_VERSION} QUIET NO_MODULE COMPONENTS Network Qml Quick QuickWidgets)

if(Qt6Qml_FOUND AND Qt6Quick_FOUND AND Qt6QuickWidgets_FOUND)

    message(STATUS "Photos mode (Qt Quick front-end) will be compiled...... YES")

    set(photosmode_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photosmode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photoscontainer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photoslibrarymodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photosgridmodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photosimageproviders.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photosmetadata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photosimporter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/photos/photoslibraries.cpp
    )

    qt_add_resources(photosmode_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/photos/photos.qrc)

    add_library(gui_photosmode_obj OBJECT ${photosmode_SRCS})

    target_compile_definitions(gui_photosmode_obj
                               PRIVATE
                               digikamgui_EXPORTS
    )

    target_include_directories(gui_photosmode_obj
                               PRIVATE
                               ${DIGIKAM_TARGET_INCLUDES}
                               ${CMAKE_CURRENT_SOURCE_DIR}/photos
    )

    # Object libraries only take usage requirements (includes, definitions) from these.

    target_link_libraries(gui_photosmode_obj
                          PRIVATE
                          Qt6::Core
                          Qt6::Gui
                          Qt6::Widgets
                          Qt6::Sql
                          Qt6::Network
                          Qt6::Concurrent
                          Qt6::Qml
                          Qt6::Quick
                          Qt6::QuickWidgets
                          KF6::ConfigCore
                          KF6::CoreAddons
                          KF6::I18n
                          KF6::XmlGui
    )

    target_sources(digikamgui PRIVATE $<TARGET_OBJECTS:gui_photosmode_obj>)

    target_link_libraries(digikamgui
                          PRIVATE
                          Qt6::Network
                          Qt6::Concurrent
                          Qt6::Qml
                          Qt6::Quick
                          Qt6::QuickWidgets
    )

    # The hooks in the stock sources (main.cpp, digikamapp*.cpp) are guarded by this definition.

    add_compile_definitions(HAVE_PHOTOSMODE)
    target_include_directories(gui_digikam_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/photos)
    list(APPEND DIGIKAM_TARGET_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/photos)

else()

    message(STATUS "Photos mode (Qt Quick front-end) will be compiled...... NO (Qt6 Quick/QuickWidgets not found)")

endif()
