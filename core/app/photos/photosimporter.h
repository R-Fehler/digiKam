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
 * The photos of an import get the tag "Devices/<device name>": this tells
 * apart the phones of a family, even of the same model, and is written to
 * the sidecars with the other information (see PhotosMetadata).
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

    /// Name of the top level tag holding the device tags.
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

    /// An import finished (also when canceled after some files).
    void imported(const QString& importId, int count);

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
        QString          source;
        bool             canceled = false;
    };

    struct ImportResult
    {
        QStringList      copied;    ///< relative to the import folder
        QList<qlonglong> ids;
        int              failed   = 0;
        bool             canceled = false;
    };

private:

    void setState(State state);
    void slotScanned();
    void slotImported();

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
    QObject*                       m_progressTimer = nullptr;
};

} // namespace Digikam
