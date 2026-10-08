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

    /// QVariantMap: id, path, name, available.
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)

public:

    explicit PhotosLibraries(QObject* const parent = nullptr);
    ~PhotosLibraries() override = default;

    QVariantList folders() const;

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

Q_SIGNALS:

    void foldersChanged();
};

} // namespace Digikam
