/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - import photos and videos from a phone,
 *               a memory card or a folder into the library, with a
 *               history of the imports which can be undone.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QAtomicInt>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace Digikam
{

class PhotosLibraryModel;

/**
 * Importing copies the new photos and videos (with their XMP sidecars) into
 * "<import folder>/YYYY/MM/", by capture date, as digiKam's own import tool
 * can do. Files already in the library (same digiKam file hash and size) are
 * skipped, so importing the same phone again only brings the new photos.
 *
 * Each import records the device name given ("Anna's iPhone"): this tells
 * apart the phones of a family, even of the same model. It is kept in the
 * import record only, not in the photos' metadata, so that importing does
 * not create a sidecar for every photo (see devices()).
 *
 * Each import is recorded in a small JSON file in the hidden
 * "<collection>/.photos-imports/" folder: the history follows the library
 * when it is synced or moved, and digiKam does not scan hidden folders.
 * Undoing an import moves its photos to digiKam's trash (restorable).
 */
class PhotosImporter : public QObject
{
    Q_OBJECT

    /// Phones, cameras and memory cards found (QVariantMap: path, name).
    Q_PROPERTY(QVariantList sources      READ sources      NOTIFY sourcesChanged)

    /// Idle, Scanning, Ready (scan summary available), Importing, Done.
    Q_PROPERTY(int          state        READ state        NOTIFY stateChanged)

    /// Scanning: files looked at. Importing: files copied.
    Q_PROPERTY(int          done         READ done         NOTIFY progressChanged)
    Q_PROPERTY(int          total        READ total        NOTIFY progressChanged)

    /// After scanning: newCount, existingCount, firstDate, lastDate, device, source.
    /// After importing: also importedCount, failedCount, importId.
    Q_PROPERTY(QVariantMap  summary      READ summary      NOTIFY stateChanged)

    /// Library folder receiving the imports (one of the collections).
    Q_PROPERTY(QString      importFolder READ importFolder WRITE setImportFolder NOTIFY importFolderChanged)
    Q_PROPERTY(QStringList  libraryFolders READ libraryFolders NOTIFY importFolderChanged)

    /// Past imports, newest first (QVariantMap: id, date, dateText, device, count, undone, source).
    Q_PROPERTY(QVariantList history      READ history      NOTIFY historyChanged)

    /// Devices named at import (QVariantMap: name, count of their files still there).
    /// A file moved or renamed outside Photos mode leaves its device.
    Q_PROPERTY(QVariantList devices      READ devices      NOTIFY devicesChanged)

public:

    enum State
    {
        Idle      = 0,
        Scanning  = 1,
        Ready     = 2,
        Importing = 3,
        Done      = 4
    };
    Q_ENUM(State)

    /// Name of the top level tag of the device tags written by earlier versions.
    static QString devicesRootTagName();

public:

    explicit PhotosImporter(PhotosLibraryModel* const library, QObject* const parent = nullptr);
    ~PhotosImporter() override;

    QVariantList sources()        const;
    int          state()          const;
    int          done()           const;
    int          total()          const;
    QVariantMap  summary()        const;
    QString      importFolder()   const;
    void         setImportFolder(const QString& folder);
    QStringList  libraryFolders() const;
    QVariantList history()        const;
    QVariantList devices()        const;

    // --- QML API ---

    Q_INVOKABLE void    refreshSources();
    Q_INVOKABLE QString chooseFolder();

    /// Looks for photos and videos in folder (recursively).
    Q_INVOKABLE void    scan(const QString& folder);

    /// Imports the new files found by scan(), tagged with the device name.
    Q_INVOKABLE void    start(const QString& deviceName);

    Q_INVOKABLE void    cancel();

    /// Back to Idle (closing the import sheet).
    Q_INVOKABLE void    reset();

    Q_INVOKABLE void    reloadHistory();

    /// Shows the photos of an import in the grid.
    Q_INVOKABLE void    showImport(const QString& importId);

    /// Shows the photos imported from a device.
    Q_INVOKABLE void    showDevice(const QString& name);

    /// Moves the photos of an import still in the library to the trash.
    /// Returns the number of photos moved.
    Q_INVOKABLE int     undoImport(const QString& importId);

    /// Number of photos of an import still in the library.
    Q_INVOKABLE int     remainingCount(const QString& importId) const;

Q_SIGNALS:

    void sourcesChanged();
    void stateChanged();
    void progressChanged();
    void importFolderChanged();
    void historyChanged();
    void devicesChanged();

    /// An import finished (also when canceled after some files).
    void imported(const QString& importId, int count);

    /**
     * An import without the sheet (inbox) finished: handled are the source
     * files now in the library (copied, or there already).
     */
    void backgroundImported(const QString& inboxId, const QString& device, int count, int failed,
                            const QString& importId, const QStringList& handled);

public:

    /// One file found by scan().
    struct Candidate
    {
        QString   path;
        QDateTime date;
        QString   device;
        qint64    size     = 0;
        bool      existing = false;
    };

    struct ScanResult
    {
        QList<Candidate> files;
        QStringList      duplicates;    ///< same content twice in the source
        QString          source;
        bool             canceled = false;
    };

    struct DeviceCount
    {
        QVariantList            devices;
        QHash<QString, QString> deviceOfPath;
    };

    /// An import without the sheet, e.g. of an inbox.
    struct BackgroundJob
    {
        QString     inboxId;
        QString     device;
        QString     source;         ///< shown in the history
        QString     root;           ///< library folder receiving the files
        QStringList files;
        bool        moveSources = false;
        QDateTime   started;
    };

    struct BackgroundResult
    {
        QStringList copied;         ///< relative to root
        QStringList handled;        ///< source files now in the library
        int         existing = 0;
        int         failed   = 0;
    };

    /// Queues an import without the sheet (runs when no other import runs).
    void importInBackground(const BackgroundJob& job);

    /// An import of this inbox is queued or running.
    bool backgroundBusy(const QString& inboxId) const;

    struct ImportResult
    {
        QStringList      copied;    ///< relative to the import folder
        QList<QPair<QString, QString> > pairs;     ///< source, copy
        QList<qlonglong> ids;
        int              failed   = 0;
        bool             canceled = false;
    };

private:

    void setState(State state);
    void slotScanned();
    void slotImported();
    void slotDevicesCounted();
    void slotBackgroundImported();
    void startNextBackground();

    QString writeRecord(const QString& root, const QString& device, const QString& source,
                        const QStringList& copied, int skipped, const QDateTime& startTime,
                        const QString& inboxId);

    QVariantMap readImport(const QString& importId, QString* const filePath = nullptr) const;
    QStringList importedPaths(const QVariantMap& record)                                const;

private:

    PhotosLibraryModel*            m_library = nullptr;
    QVariantList                   m_sources;
    QVariantList                   m_history;
    State                          m_state   = Idle;
    QAtomicInt                     m_done;
    QAtomicInt                     m_total;
    QAtomicInt                     m_cancel;
    QVariantMap                    m_summary;
    ScanResult                     m_scan;
    QString                        m_deviceName;
    QString                        m_importFolder;
    QDateTime                      m_startTime;
    QFutureWatcher<ScanResult>     m_scanWatcher;
    QFutureWatcher<ImportResult>   m_importWatcher;
    QFutureWatcher<DeviceCount>    m_devicesWatcher;
    QFutureWatcher<BackgroundResult> m_backgroundWatcher;
    QList<BackgroundJob>           m_queue;
    BackgroundJob                  m_currentJob;
    QHash<QString, QStringList>    m_deviceFiles;
    QVariantList                   m_devices;
    bool                           m_devicesPending = false;
    QObject*                       m_progressTimer = nullptr;
};

} // namespace Digikam
