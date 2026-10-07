#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
# SPDX-FileCopyrightText: 2015      by Veaceslav Munteanu, <veaceslav dot munteanu90 at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

# -----------------------------------------------------------------------------------
# Headers to configure

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/utils/digikam_version.h.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/utils/digikam_version.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/utils/digikam_dbconfig.h.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/utils/digikam_dbconfig.h)

# -----------------------------------------------------------------------------------
# Define common includes rules for digiKam targets

HEADER_DIRECTORIES(${CMAKE_SOURCE_DIR}/core/utilities DK_LOCAL_INCLUDES_UTILS)

set(DIGIKAM_TARGET_INCLUDES $<TARGET_PROPERTY:Qt6::PrintSupport,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:Qt6::Xml,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:Qt6::Sql,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:Qt6::Network,INTERFACE_INCLUDE_DIRECTORIES>

                            $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:KF6::Solid,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:KF6::WindowSystem,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:KF6::XmlGui,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:KF6::Service,INTERFACE_INCLUDE_DIRECTORIES>
                            $<TARGET_PROPERTY:KF6::CoreAddons,INTERFACE_INCLUDE_DIRECTORIES>

                            ${CMAKE_SOURCE_DIR}/core/app/main
                            ${CMAKE_SOURCE_DIR}/core/app/date
                            ${CMAKE_SOURCE_DIR}/core/app/filters
                            ${CMAKE_SOURCE_DIR}/core/app/dragdrop
                            ${CMAKE_SOURCE_DIR}/core/app/views/stack
                            ${CMAKE_SOURCE_DIR}/core/app/views/utils
                            ${CMAKE_SOURCE_DIR}/core/app/views/sidebar
                            ${CMAKE_SOURCE_DIR}/core/app/views/tableview
                            ${CMAKE_SOURCE_DIR}/core/app/items/utils
                            ${CMAKE_SOURCE_DIR}/core/app/items/thumbbar
                            ${CMAKE_SOURCE_DIR}/core/app/items/delegate
                            ${CMAKE_SOURCE_DIR}/core/app/items/views
                            ${CMAKE_SOURCE_DIR}/core/app/items/overlays
                            ${CMAKE_SOURCE_DIR}/core/app/views/preview

                            ${DK_LOCAL_INCLUDES}
                            ${DK_LOCAL_INCLUDES_UTILS}
)

if(KF6IconThemes_FOUND)
    set(DIGIKAM_TARGET_INCLUDES
        ${DIGIKAM_TARGET_INCLUDES}
        $<TARGET_PROPERTY:KF6::IconThemes,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

set(DIGIKAM_TARGET_INCLUDES
    ${DIGIKAM_TARGET_INCLUDES}
    $<TARGET_PROPERTY:Qt6::WebEngineWidgets,INTERFACE_INCLUDE_DIRECTORIES>
)

if(KF6KIO_FOUND)
    set(DIGIKAM_TARGET_INCLUDES
        ${DIGIKAM_TARGET_INCLUDES}
        $<TARGET_PROPERTY:KF6::KIOWidgets,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

if(Gphoto2_FOUND)
    set(DIGIKAM_TARGET_INCLUDES
        ${DIGIKAM_TARGET_INCLUDES}
        ${GPHOTO2_INCLUDE_DIRS})
endif()

# -----------------------------------------------------------------------------------
# Targets to build

include(DigikamCoreTarget.cmake)
include(DigikamDatabaseTarget.cmake)
include(DigikamGuiTarget.cmake)
include(DigikamTarget.cmake)

# -----------------------------------------------------------------------------------
# Others Files to install

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/main/org.kde.digikam.desktop     DESTINATION ${KDE_INSTALL_FULL_APPDIR})
install(FILES    ${CMAKE_CURRENT_SOURCE_DIR}/main/org.kde.digikam.appdata.xml DESTINATION ${KDE_INSTALL_FULL_METAINFODIR})
install(FILES    ${CMAKE_CURRENT_SOURCE_DIR}/main/digikamui5.rc               DESTINATION ${KDE_INSTALL_FULL_KXMLGUIDIR}/digikam)
install(FILES    ${CMAKE_CURRENT_SOURCE_DIR}/main/digikam.notifyrc            DESTINATION ${KDE_INSTALL_FULL_KNOTIFYRCDIR})

include(DigikamExportAPI.cmake)
