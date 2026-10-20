/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - moving photos between the personal and
 *               the shared library (library folders of each kind).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

namespace Digikam
{

class PhotosImporter;
class PhotosLibraries;
class PhotosLibraryModel;

/**
 * "Move to Shared" and "Move to Personal": the selected photos go to a
 * library folder of the other kind (see PhotosLibraries::targetFolder()),
 * at the same place in it ("2024/05/IMG_0001.HEIC"), with their sidecars
 * and the video of a Live Photo.
 *
 * The database entry moves with the file (CoreDB::moveItem(), as digiKam
 * does): albums, favorites, captions and people stay, sidecars or not. A
 * file is copied under a hidden temporary name when the folders are on
 * different drives, and removed from where it was once the copy has the
 * same size. The device a photo was imported from stays too (a record of
 * the move in the target folder, see PhotosImporter::recordMoved()).
 */
class PhotosMover : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:

    explicit PhotosMover(PhotosLibraryModel* const library, PhotosLibraries* const libraries,
                         PhotosImporter* const importer, QObject* const parent = nullptr);
    ~PhotosMover() override;

    bool busy() const;

    /// Moves the selected photos to the shared (or personal) library.
    Q_INVOKABLE void moveSelection(bool toShared);

public:

    struct Job
    {
        qlonglong id         = 0;
        QString   path;
        QString   relativeDir;      ///< "/2024/05", or "/" for the top of the folder
        int       albumId    = 0;
        QString   name;
    };

    struct Result
    {
        QList<QPair<QString, QString> > moved;     ///< from, to
        int                             failed = 0;
    };

Q_SIGNALS:

    void busyChanged();

    /// A move finished: count photos and videos moved, failed not.
    void moved(int count, int failed, bool toShared);

    /// Nothing moved, and why.
    void failed(const QString& message);

private:

    void slotMoved();

private:

    PhotosLibraryModel*    m_library   = nullptr;
    PhotosLibraries*       m_libraries = nullptr;
    PhotosImporter*        m_importer  = nullptr;
    QFutureWatcher<Result> m_watcher;
    QString                m_targetRoot;
    bool                   m_toShared  = false;
};

} // namespace Digikam
