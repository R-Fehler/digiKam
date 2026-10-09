/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - inbox folders: folders where phones and
 *               other devices drop new photos (sync apps, uploads),
 *               imported into the library automatically.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QFileSystemWatcher;
class QTimer;

namespace Digikam
{

class PhotosImporter;

/**
 * An inbox is a folder where something else drops new photos: a phone sync
 * app (Syncthing, PhotoSync, Nextcloud, Synology Photos...), a partner, a
 * scanner. Its new files are imported like with the Import sheet (no
 * duplicates, YYYY/MM folders, import record with the device name), then
 * moved out of the inbox or left there.
 *
 * Made for synced libraries (NAS shares, Syncthing, Nextcloud, Google
 * Drive... with real files, not on-demand placeholders):
 *
 *  - definitions live in the library folder they import into
 *    (<library folder>/.photos-imports/inboxes.json, paths relative to it
 *    when the inbox is inside it), so every computer syncing the library
 *    knows them;
 *  - one computer imports an inbox ("importer", the one which added it by
 *    default): two computers never import the same files; the others show
 *    its state and can take over;
 *  - a file is imported once it has stopped changing for 30 s (sync tools
 *    write files in several steps), never under a temporary name
 *    (.syncthing.*.tmp, .part, ~...); files arriving together form one
 *    import (one record, one Undo);
 *  - folders are listed every 30 s (network shares and some sync tools do
 *    not report changes) and at once when the system reports a change.
 */
class PhotosInboxes : public QObject
{
    Q_OBJECT

    /**
     * QVariantMap per inbox: id, path, device, mode ("move" or "leave"),
     * importer (computer), thisComputer, found, waiting, importing, status,
     * lastImport (text), root (library folder).
     */
    Q_PROPERTY(QVariantList inboxes READ inboxes NOTIFY inboxesChanged)

public:

    explicit PhotosInboxes(PhotosImporter* const importer, QObject* const parent = nullptr);
    ~PhotosInboxes() override;

    QVariantList inboxes() const;

    // --- QML API ---

    Q_INVOKABLE QString chooseFolder() const;

    /// Suggested device name for a folder (its name: "Anna-iPhone" -> "Anna iPhone").
    Q_INVOKABLE QString suggestedDevice(const QString& folder) const;

    /**
     * Adds an inbox importing into the library folder root (the one
     * containing folder, if any). Returns an error message, empty on success.
     */
    Q_INVOKABLE QString addInbox(const QString& folder, const QString& device,
                                 bool move, const QString& root);

    Q_INVOKABLE void    removeInbox(const QString& id);

    /// This computer imports this inbox from now on.
    Q_INVOKABLE void    importOnThisComputer(const QString& id);

    /// Where an inbox outside the library is on this computer (other path than on its computer).
    Q_INVOKABLE void    locateInbox(const QString& id, const QString& folder);

    /// Looks at the inboxes now (also done every 30 s).
    Q_INVOKABLE void    checkNow();

    /// The library folder containing folder, or empty.
    Q_INVOKABLE QString libraryFolderOf(const QString& folder) const;

    /// Files which are not photos or videos yet: temporary names of sync tools, hidden files.
    static bool isTemporaryName(const QString& relativePath);

Q_SIGNALS:

    void inboxesChanged();

public:

    struct Definition
    {
        QString id;
        QString root;           ///< library folder receiving the imports
        QString storedPath;     ///< as in inboxes.json: relative to root, or absolute
        QString path;           ///< on this computer
        QString device;
        QString importer;       ///< computer importing it
        bool    move = true;
    };

    struct Stamp
    {
        QString   path;
        qint64    size = 0;
        QDateTime modified;
    };

    struct Listing
    {
        QHash<QString, QList<Stamp> > files;    ///< by inbox id
        QHash<QString, bool>          found;
    };

private:

    struct FileState
    {
        qint64    size = 0;
        QDateTime modified;
        QDateTime changedAt;    ///< when seen with this size and date first
        QDateTime retryAfter;   ///< failed to import: not before
        bool      handled = false;
    };

    void loadDefinitions();
    bool saveDefinitions(const QString& root);
    void slotListed();
    void slotBackgroundImported(const QString& inboxId, const QString& device, int count, int failed,
                                const QString& importId, const QStringList& handled);
    void ignoreInLibrary(const Definition& inbox);
    void updateWatcher();

private:

    PhotosImporter*                            m_importer = nullptr;
    QList<Definition>                          m_definitions;
    QHash<QString, QHash<QString, FileState> > m_state;          ///< inbox id -> path -> state
    QHash<QString, bool>                       m_found;
    QHash<QString, QString>                    m_lastError;
    QTimer*                                    m_pollTimer = nullptr;
    QTimer*                                    m_soonTimer = nullptr;
    QFileSystemWatcher*                        m_watcher   = nullptr;
    QFutureWatcher<Listing>                    m_listWatcher;
    QHash<QString, QDateTime>                  m_definitionStamps;  ///< inboxes.json per library folder
    bool                                       m_listAgain = false;
};

} // namespace Digikam
