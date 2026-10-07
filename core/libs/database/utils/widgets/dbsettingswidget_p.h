/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2009-11-14
 * Description : database settings widget - private container
 *
 * SPDX-FileCopyrightText: 2009-2010 by Holger Foerster <Hamsi2k at freenet dot de>
 * SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

#include "dbsettingswidget.h"

// Qt includes

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStandardPaths>
#include <QString>
#include <QPushButton>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QTextBrowser>
#include <QTimer>
#include <QHelpEvent>
#include <QVBoxLayout>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_config.h"
#include "digikam_debug.h"
#include "digikam_globals.h"
#include "applicationsettings.h"
#include "dfileselector.h"
#include "dbengineparameters.h"
#include "dbinarysearch.h"
#include "dexpanderbox.h"
#include "dlayoutbox.h"
#include "mariadbupgradebinary.h"
#include "mariadbserverbinary.h"
#include "mariadbadminbinary.h"
#include "mariadbinitbinary.h"
#include "albummanager.h"

namespace Digikam
{

class Q_DECL_HIDDEN DatabaseSettingsWidget::Private
{

public:

    Private() = default;

public:

    DVBox*               mariadbCmdBox            = nullptr;

    QLineEdit*           dbNameCore               = nullptr;
    QLineEdit*           dbNameFace               = nullptr;
    QLineEdit*           dbNameSimilarity         = nullptr;
    QLineEdit*           hostName                 = nullptr;
    QLineEdit*           connectOpts              = nullptr;
    QLineEdit*           userName                 = nullptr;
    QLineEdit*           password                 = nullptr;

    QSpinBox*            hostPort                 = nullptr;

    QComboBox*           dbType                   = nullptr;
    QPushButton*         dbTypeBtn                = nullptr;
    QLabel*              dbPathLbl                = nullptr;
    QPushButton*         dbPathBtn                = nullptr;
    QPushButton*         walModeBtn               = nullptr;
    QLabel*              dbThumbsLabel            = nullptr;
    QTextBrowser*        sqlInit                  = nullptr;
    QGroupBox*           expertSettings           = nullptr;
    QGroupBox*           dbNoticeBox              = nullptr;
    QGroupBox*           dbDetailsBox             = nullptr;
    QTabWidget*          tab                      = nullptr;

    DFileSelector*       dbPathEdit               = nullptr;
    DFileSelector*       dbNameThumbs             = nullptr;

    QCheckBox*           walModeCheck             = nullptr;

    DBinarySearch*       dbBinariesWidget         = nullptr;

    MariaDBUpgradeBinary mariadbUpgradeBin;
    MariaDBServerBinary  mariadbServerBin;
    MariaDBAdminBinary   mariadbAdminBin;
    MariaDBInitBinary    mariadbInitBin;

    DbEngineParameters   orgPrms;

    QMap<int, int>       dbTypeMap;

    QGroupBox*           ignoreDirectoriesBox     = nullptr;
    QLineEdit*           ignoreDirectoriesEdit    = nullptr;
    QLabel*              ignoreDirectoriesLabel   = nullptr;
};

} // namespace Digikam
