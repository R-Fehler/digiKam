/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - keeping a synced or shared library in
 *               shape: rescans where changes are not reported, and
 *               merging of sidecar sync conflicts.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QDateTime>
#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class QTimer;

namespace Digikam
{

/**
 * Libraries shared through a NAS or synced (Syncthing, Nextcloud, Google
 * Drive, Dropbox, OneDrive... with real files):
 *
 *  - Rescans. Folder monitoring sees changes made on this computer and
 *    files written by sync tools, but network shares (SMB, NFS) report
 *    nothing, and a computer coming back from sleep may have missed some.
 *    Library folders on network shares are rescanned every 10 minutes, all
 *    of them when Photos comes to the front after 15 minutes or more.
 *
 *  - Conflicts. When two computers change the same sidecar before syncing,
 *    sync tools keep both: "IMG.JPG.sync-conflict-20261009-101500-ABC1234.xmp"
 *    (Syncthing), "IMG.JPG (Anna's conflicted copy 2026-10-09).xmp"
 *    (Dropbox), "IMG.JPG (conflicted copy 2026-10-09 101500).xmp"
 *    (Nextcloud, ownCloud). The copy is merged into the photo: albums, people
 *    and tags of both, the higher rating, the caption of the newer one when
 *    they differ; then the copy goes to digiKam's trash. Only done in
 *    sidecar mode.
 */
class PhotosSyncHealth : public QObject
{
    Q_OBJECT

public:

    explicit PhotosSyncHealth(QObject* const parent = nullptr);
    ~PhotosSyncHealth() override;

    /// For a conflict copy of a sidecar, the name of the original sidecar; empty otherwise.
    static QString conflictOriginal(const QString& fileName);

Q_SIGNALS:

    /// Conflict copies merged into their photo.
    void conflictsMerged(int count);

private:

    void run(bool allFolders);
    void slotConflictsFound();

    /// Returns true when the photo changed.
    static bool mergeConflict(const QString& conflictPath, const QString& sidecarPath);

private:

    QTimer*                                         m_timer = nullptr;
    QDateTime                                       m_lastFullScan;
    QFutureWatcher<QList<QPair<QString, QString> > > m_conflicts;   ///< conflict copy, sidecar
};

} // namespace Digikam
