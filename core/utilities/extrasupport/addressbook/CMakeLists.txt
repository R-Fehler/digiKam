#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

APPLY_COMMON_POLICIES()

set(akonadiiface_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/akonadiiface.cpp)

include_directories($<TARGET_PROPERTY:Qt6::Widgets,INTERFACE_INCLUDE_DIRECTORIES>
                    $<TARGET_PROPERTY:Qt6::Core,INTERFACE_INCLUDE_DIRECTORIES>

                    $<TARGET_PROPERTY:KF6::I18n,INTERFACE_INCLUDE_DIRECTORIES>
)

if(KPim6AkonadiContact_FOUND)

    include_directories(
        $<TARGET_PROPERTY:KF6::AkonadiCore,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:KF6::Contacts,INTERFACE_INCLUDE_DIRECTORIES>

        $<TARGET_PROPERTY:KF6::CoreAddons,INTERFACE_INCLUDE_DIRECTORIES>
    )

elseif(KPim6AkonadiContactCore_FOUND)

    include_directories(
        $<TARGET_PROPERTY:KPim6::AkonadiCore,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:KF6::Contacts,INTERFACE_INCLUDE_DIRECTORIES>

        $<TARGET_PROPERTY:KF6::CoreAddons,INTERFACE_INCLUDE_DIRECTORIES>
    )

endif()

add_library(core_akonadiiface_obj OBJECT ${akonadiiface_SRCS})

target_compile_definitions(core_akonadiiface_obj
                           PRIVATE
                           digikamcore_EXPORTS
)


add_library(akonadiiface STATIC $<TARGET_OBJECTS:core_akonadiiface_obj>)

target_link_libraries(akonadiiface PRIVATE Qt6::Core KF6::I18n)

if(KPim6AkonadiContact_FOUND)

    target_include_directories(core_akonadiiface_obj PRIVATE $<TARGET_PROPERTY:KPim6::AkonadiContact,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(akonadiiface PRIVATE KPim6::AkonadiContact KF6::Contacts)

elseif(KPim6AkonadiContactCore_FOUND)

    target_include_directories(core_akonadiiface_obj PRIVATE $<TARGET_PROPERTY:KPim6::AkonadiContactCore,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(akonadiiface PRIVATE KPim6::AkonadiContactCore KF6::Contacts)

endif()
