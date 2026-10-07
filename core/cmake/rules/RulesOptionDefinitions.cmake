#
# SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Packaging options.

option(ENABLE_DIGIKAM_CORE               "Build digiKam core (default=ON)"                                                           ON)
option(ENABLE_GUI_TRANSLATIONS           "Build GUI translations files (default=ON)"                                                 ON)
option(ENABLE_SHOWFOTO                   "Build with Showfoto stand-alone image editor (default=ON)"                                 ON)

# Features options.

option(ENABLE_KFILEMETADATASUPPORT       "Build digiKam with Plasma desktop files indexer support (default=OFF)"                     OFF)
option(ENABLE_AKONADICONTACTSUPPORT      "Build digiKam with Plasma desktop Mail Contacts support (default=OFF)"                     OFF)
option(ENABLE_GEOLOCATION                "Build digiKam with Geolocation support (default=ON)"                                       ON)
option(ENABLE_MEDIAPLAYER                "Build digiKam with Media Player support (default=ON)"                                      ON)
option(ENABLE_DBUS                       "Build digiKam with DBUS support (default=ON)"                                              ON)
option(ENABLE_APPSTYLES                  "Build digiKam with support for changing the widget application style (default=OFF)"        OFF)
option(ENABLE_KIO                        "Build digiKam with KIO support (default=ON)"                                               ON)
option(ENABLE_NLSEARCH_LLAMACPP          "Build digiKam with llama.cpp backend for natural-language search (default=OFF)"            OFF)

# Database options:

option(ENABLE_MARIADBSUPPORT             "Build digiKam with MariaDB dabatase support (default=ON)"                                  ON)
option(ENABLE_INTERNALMARIADB            "Build digiKam with internal MariaDB server executable (default=ON)"                        ON)

# Developer options:

option(ENABLE_DIGIKAM_MODELTEST          "Enable ModelTest on some models for debugging (default=OFF)"                               OFF)
option(ENABLE_SANITIZERS                 "Enable ASAN and UBSAN sanitizers when available (default=OFF)"                             OFF)
option(BUILD_WITH_CCACHE                 "Use ccache to speed up compilations"                                                       OFF)

