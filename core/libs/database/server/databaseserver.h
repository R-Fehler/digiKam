/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2009-11-14
 * Description : MariaDB internal database server
 *
 * SPDX-FileCopyrightText: 2009-2011 by Holger Foerster <Hamsi2k at freenet dot de>
 * SPDX-FileCopyrightText: 2010-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2016      by Swati Lodha <swatilodha27 at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// QT includes

#include <QProcess>
#include <QThread>
#include <QString>

// Local includes

#include "databaseserverstarter.h"
#include "databaseservererror.h"
#include "dbengineparameters.h"
#include "digikam_export.h"

class QCoreApplication;

namespace Digikam
{

class DIGIKAM_EXPORT DatabaseServer : public QThread
{
    Q_OBJECT

public:

    enum DatabaseServerStateEnum
    {
        started,
        running,
        notRunning,
        stopped
    };
    DatabaseServerStateEnum databaseServerStateEnum;

    Q_ENUM(DatabaseServerStateEnum);

public:

    explicit DatabaseServer(const DbEngineParameters& params,
                            DatabaseServerStarter* const parent = DatabaseServerStarter::instance());
    ~DatabaseServer() override;

    /**
     * Starts the database management server.
     */
    DatabaseServerError startDatabaseProcess();

    /**
     * Terminates the databaser server process.
     */
    void stopDatabaseProcess();

    /**
     * Returns true if the server process is running.
     */
    bool isRunning()                                                     const;

Q_SIGNALS:

    void done();

protected:

    void run() override;

private:

    /**
     * Inits and Starts MariaDB server.
     */
    DatabaseServerError startMariaDBDatabaseProcess();

    /**
     * Checks if MariaDB binaries and database directories exists and creates
     * the latter if necessary.
     */
    DatabaseServerError checkDatabaseDirs()                              const;

    /**
     * Finds and updates (if necessary) configuration files for the MariaDB
     * server.
     */
    DatabaseServerError initMariaDBConfig()                              const;

    /**
     * Copy and remove MariaDB error log files.
     */
    void copyAndRemoveMariaDBLogs()                                      const;

    /**
     * Creates initial MariaDB database files for internal server.
     */
    DatabaseServerError createMariaDBFiles()                             const;

    /**
     * Starts the server for the internal database.
     */
    DatabaseServerError startMariaDBServer();

    /**
     * Creates or connects to database digikam in MariaDB.
     */
    DatabaseServerError initMariaDBDatabase(bool useDatabase)            const;

    /**
     * Perform a MariaDB database upgrade.
     */
    DatabaseServerError upgradeMariaDBDatabase();

    /**
     * Return the current user account name.
     */
    QString getcurrentAccountUserName()                                  const;

    /**
     * Returns i18n converted error message and writes to qCDebug.
     */
    QString processErrorLog(QProcess* const process,
                            const QString& msg)                          const;

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
