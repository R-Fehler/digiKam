#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED
                                    COMPONENTS
                                    XmlGui
                                    CoreAddons
                                    Config
                                    Service
                                    WindowSystem
                                    Solid
                                    I18n
)

find_package(KF6 ${KF6_MIN_VERSION} QUIET
                                    OPTIONAL_COMPONENTS
                                    KIO                         # For Desktop integration (Widgets only).
                                    IconThemes                  # For Desktop integration.
                                    ThreadWeaver                # For Panorama tool.
                                    NotifyConfig                # Plasma desktop application notify configuration.
                                    Notifications               # Plasma desktop notifications integration.
                                    Sonnet                      # For spell-checking.
)

if(ENABLE_KFILEMETADATASUPPORT)

    find_package(KF6 ${KF6_MIN_VERSION} QUIET
                                        OPTIONAL_COMPONENTS
                                        FileMetaData            # For Plasma desktop file indexer support.
    )

endif()

if(ENABLE_AKONADICONTACTSUPPORT)

    find_package(KPim6 ${KF6_MIN_VERSION} QUIET
                                          OPTIONAL_COMPONENTS
                                          Akonadi
                                          AkonadiContactCore  # For KDE Mail Contacts support.
    )

    find_package(KF6 ${KF6_MIN_VERSION} QUIET
                                        OPTIONAL_COMPONENTS
                                        Contacts            # API for contacts/address book data.
    )

    find_package(KPim6Akonadi ${KF6_MIN_VERSION} QUIET)
    find_package(KPim6AkonadiContactCore ${KF6_MIN_VERSION} QUIET)

endif()

find_package(KSaneWidgets6)

find_package(KF6 ${KF6_MIN_VERSION} QUIET
                                    OPTIONAL_COMPONENTS
                                    CalendarCore           # For Calendar tool.
)

set(HAVE_KCALENDAR_QDATETIME TRUE)

if(ENABLE_AKONADICONTACTSUPPORT AND
   (NOT (KPim6AkonadiContact_FOUND OR KPim6AkonadiContactCore_FOUND) OR NOT KF6Contacts_FOUND))

    set(ENABLE_AKONADICONTACTSUPPORT OFF)

endif()

if(ENABLE_KFILEMETADATASUPPORT AND NOT KF6FileMetaData_FOUND)

    set(ENABLE_KFILEMETADATASUPPORT OFF)

endif()

# Check if KIO have been compiled with KIOWidgets. digiKam only needs this one.

if(ENABLE_KIO)

    if(KF6KIO_FOUND)

        get_target_property(KIOWidgets_INCLUDE_DIRS KF6::KIOWidgets
                            INTERFACE_INCLUDE_DIRECTORIES)
        message(STATUS "KF6::KIOWidgets include dirs: ${KIOWidgets_INCLUDE_DIRS}")

        if(NOT KIOWidgets_INCLUDE_DIRS)

            message(STATUS "KF6::KIOWidgets not available in shared KIO library. KIO support disabled.")
            set(KF6KIO_FOUND FALSE)

        endif()

    endif()

else()

    message(STATUS "KF6::KIO support is explicitly disabled.")
    set(KF6KIO_FOUND FALSE)

endif()
