#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

include_directories(${CMAKE_SOURCE_DIR}/core/libs/dplugins/webservices/o2/src)

include_directories($<TARGET_PROPERTY:Qt6::WebEngineWidgets,INTERFACE_INCLUDE_DIRECTORIES>)
set(QWEB_BACKEND Qt6::WebEngineWidgets)

add_subdirectory(box)
add_subdirectory(dropbox)
add_subdirectory(filecopy)
add_subdirectory(flickr)
add_subdirectory(google)
add_subdirectory(imgur)
add_subdirectory(imageshack)
add_subdirectory(inaturalist)
add_subdirectory(mediawiki)
add_subdirectory(onedrive)
add_subdirectory(pinterest)
add_subdirectory(piwigo)
add_subdirectory(smugmug)
add_subdirectory(twitter)

if(KF6KIO_FOUND)

    add_subdirectory(filetransfer)

endif()

# Deprecated
#add_subdirectory(facebook)
#add_subdirectory(ipfs)

# Not yet finalized
#add_subdirectory(unified)
