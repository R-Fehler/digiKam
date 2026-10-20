/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - the folders of the library (digiKam's
 *               collections): list, add, remove, and opening a
 *               folder or a photo given on the command line.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace Digikam
{

/**
 * Library folders are digiKam's collections ("album roots"): one database,
 * any number of folders, on internal or removable drives or network shares.
 * Stock digiKam shows the same folders. A folder on a drive which is not
 * connected stays in the library and comes back when the drive does.
 */
class PhotosLibraries : public QObject
{
    Q_OBJECT

    /// QVariantMap: id, path, name, available, removable, network, syncTool, shared.
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)

    /**
     * Personal and shared libraries, as in Apple Photos: the library folders
     * shared with others (a NAS, a synced folder) and the personal ones (on
     * this computer only). Shown: Both (0), Personal (1) or Shared (2);
     * only when there are folders of both kinds.
     */
    Q_PROPERTY(int          scope        READ scope        WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(bool         hasBothKinds READ hasBothKinds NOTIFY foldersChanged)

public:

    explicit PhotosLibraries(QObject* const parent = nullptr);
    ~PhotosLibraries() override = default;

    QVariantList folders()      const;
    int          scope()        const;
    void         setScope(int scope);
    bool         hasBothKinds() const;

    /// Library folders (ids) shown in the current scope; empty: all of them.
    QSet<int>    scopeRootIds() const;

    /// Library folders (ids) which are shared.
    QSet<int>    sharedRootIds() const;

    /**
     * A folder is shared when marked so in "<folder>/.photos-imports/library.json"
     * (which syncs with it: every computer agrees), else when it is on a
     * network share or in a folder of a sync tool.
     */
    Q_INVOKABLE void    setShared(int id, bool shared);

    /// Where photos moved to the personal or shared library go: the import
    /// folder when of that kind, else the first available folder of that kind.
    Q_INVOKABLE QString targetFolder(bool shared) const;

    /**
     * What opening path means (command line, file manager, Finder):
     *  - path:      the folder to show (the parent folder for a file);
     *  - file:      the photo to open, if path was a file;
     *  - inLibrary: the folder is in one of the library folders;
     *  - canAdd:    else, whether it can be added as a library folder;
     *  - message:   why not, or what adding means.
     */
    Q_INVOKABLE QVariantMap checkPath(const QString& path) const;

    /// Asks for a folder and returns it (empty when canceled).
    Q_INVOKABLE QString chooseFolder() const;

    /// Adds a library folder and scans it. Returns an error message, empty on success.
    Q_INVOKABLE QString addFolder(const QString& path);

    /// Removes a library folder from the library (not from the disk).
    Q_INVOKABLE void    removeFolder(int id);

    /// Number of photos and videos of the library in a folder (recursive).
    Q_INVOKABLE int     photoCount(int id) const;

    /**
     * Where the library database is, and whether that is a problem:
     *  - path:      folder of the SQLite files (empty for MariaDB);
     *  - problem:   "", "library" (inside a library folder), "network"
     *               (network share) or "synced" (folder of a sync tool);
     *  - tool:      the sync tool recognized, if any;
     *  - suggested: a folder on this computer;
     *  - pending:   a move is asked for at the next start.
     * Two computers must never open the same SQLite file: through a share
     * or a sync tool, it gets damaged. Each computer keeps its own database
     * and reads the shared photos and sidecars.
     */
    Q_INVOKABLE QVariantMap databaseCheck() const;

    /// Moves the database to target at the next start (empty: cancel).
    Q_INVOKABLE void        requestDatabaseMove(const QString& target);

    /// The folder is on a network share (no change notifications: rescanned periodically).
    static bool isNetworkPath(const QString& path);

    /// The sync tool managing this folder, from its marker files, or empty.
    static QString syncToolOf(const QString& path);

Q_SIGNALS:

    void foldersChanged();
    void scopeChanged();

private:

    static bool isShared(const QString& path, bool network);

private:

    int m_scope = 0;
};

} // namespace Digikam
