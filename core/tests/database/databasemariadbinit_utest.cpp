/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2010-06-21
 * Description : unit test program for digiKam MariaDB database init
 *
 * SPDX-FileCopyrightText: 2013 by Michael G. Hansen <mike at mghansen dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "databasemariadbinit_utest.h"

// Qt includes

#include <QApplication>
#include <QSqlDatabase>
#include <QTimer>
#include <QCommandLineParser>

// KDE includes

#include <kaboutdata.h>

// Local includes

#include "digikam_debug.h"
#include "daboutdata.h"
#include "albummanager.h"
#include "coredbaccess.h"
#include "thumbsdbaccess.h"
#include "facedbaccess.h"
#include "similaritydbaccess.h"
#include "dbengineparameters.h"
#include "scancontroller.h"
#include "digikam_version.h"
#include "dtestdatadir.h"
#include "wstoolutils.h"
#include "mariadbupgradebinary.h"
#include "mariadbserverbinary.h"
#include "mariadbadminbinary.h"
#include "mariadbinitbinary.h"
#include "databaseserverstarter.h"

using namespace Digikam;

QTEST_MAIN(DatabaseMariaDBInitTest)

void DatabaseMariaDBInitTest::initTestCase()
{
    m_tempPath  = QString::fromLatin1(QTest::currentAppName());
    m_tempPath.replace(QLatin1String("./"), QString());
    m_tempDir   = WSToolUtils::makeTemporaryDir(m_tempPath.toLatin1().data());
    qCDebug(DIGIKAM_TESTS_LOG) << "Database Dir:" << m_tempDir.path();

    m_filesPath = DTestDataDir::TestData(QString::fromUtf8("core/tests/database/testimages"))
                           .root().path() + QLatin1Char('/');
    qCDebug(DIGIKAM_TESTS_LOG) << "Test Data Dir:" << m_filesPath;

    KAboutData aboutData(QLatin1String("digikam"),
                         QLatin1String("digiKam"), // No need i18n here.
                         digiKamVersion());

    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    aboutData.setupCommandLine(&parser);
    parser.process(*QCoreApplication::instance());
    aboutData.processCommandLine(&parser);
}

void DatabaseMariaDBInitTest::testMariaDBInit()
{
    qCDebug(DIGIKAM_TESTS_LOG) << "Setup MariaDB Database...";

    MariaDBUpgradeBinary mariadbUpgradeBin;

    if (!mariadbUpgradeBin.recheckDirectories())
    {
        qWarning() << QLatin1String("Not able to found the MariaDB Upgrade binary program. Test is aborted...");
        return;
    }

    MariaDBServerBinary  mariadbServerBin;
    mariadbServerBin.slotAddPossibleSearchDirectory(QLatin1String("/usr/sbin"));

    if (!mariadbServerBin.recheckDirectories())
    {
        qWarning() << QLatin1String("Not able to found the MariaDB Server binary program. Test is aborted...");
        return;
    }

    MariaDBAdminBinary   mariadbAdminBin;

    if (!mariadbAdminBin.recheckDirectories())
    {
        qWarning() << QLatin1String("Not able to found the MariaDB Admin binary program. Test is aborted...");
        return;
    }

    MariaDBInitBinary    mariadbInitBin;

    if (!mariadbInitBin.recheckDirectories())
    {
        qWarning() << QLatin1String("Not able to found the MariaDB Init binary program. Test is aborted...");
        return;
    }

    if (!QSqlDatabase::isDriverAvailable(DbEngineParameters::MariaDBDatabaseType()))
    {
        qWarning() << QLatin1String("Qt MariaDB plugin is missing.");
        return;
    }

    DbEngineParameters params;
    QString defaultAkDir                   = DbEngineParameters::serverPrivatePath();
    QString miscDir                        = QDir(defaultAkDir).absoluteFilePath(QLatin1String("db_misc"));
    params.databaseType                    = DbEngineParameters::MariaDBDatabaseType();
    params.databaseNameCore                = QLatin1String("digikam");
    params.databaseNameThumbnails          = QLatin1String("digikam");
    params.databaseNameFace                = QLatin1String("digikam");
    params.databaseNameSimilarity          = QLatin1String("digikam");
    params.userName                        = QLatin1String("root");
    params.password                        = QString();
    params.internalServer                  = true;
    params.internalServerDBPath            = m_tempDir.path();
    params.internalServerMariaDBUpgradeCmd = mariadbUpgradeBin.path();
    params.internalServerMariaDBServerCmd  = mariadbServerBin.path();
    params.internalServerMariaDBAdminCmd   = mariadbAdminBin.path();
    params.internalServerMariaDBInitCmd    = mariadbInitBin.path();
    params.hostName                        = QString();
    params.port                            = -1;
    params.connectOptions                  = QString::fromLatin1("UNIX_SOCKET=%1/mysql.socket").arg(miscDir);

    // ------------------------------------------------------------------------------------

    qCDebug(DIGIKAM_TESTS_LOG) << "Initializing MariaDB database...";
    QVERIFY2(AlbumManager::instance()->setDatabase(params, false, m_filesPath),
             "Cannot initialize MariaDB database");

    QTest::qWait(3000);

    qCDebug(DIGIKAM_TESTS_LOG) << "Shutting down MariaDB database";
    ScanController::instance()->shutDown();
    AlbumManager::instance()->cleanUp();

    qCDebug(DIGIKAM_TESTS_LOG) << "Cleaning MariaDB database";
    CoreDbAccess::cleanUpDatabase();
    ThumbsDbAccess::cleanUpDatabase();
    FaceDbAccess::cleanUpDatabase();
    SimilarityDbAccess::cleanUpDatabase();

    DatabaseServerStarter::instance()->stopServerManagerProcess();
}

void DatabaseMariaDBInitTest::cleanupTestCase()
{
    WSToolUtils::removeTemporaryDir(m_tempPath.toLatin1().data());
}

#include "moc_databasemariadbinit_utest.cpp"
