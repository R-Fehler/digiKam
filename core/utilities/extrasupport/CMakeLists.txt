#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

if(KPim6AkonadiContact_FOUND OR KPim6AkonadiContactCore_FOUND)
    add_subdirectory(addressbook)
endif()

if(KF6FileMetaData_FOUND)
    add_subdirectory(filesindexer)
endif()
