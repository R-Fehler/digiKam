/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Description : Photos mode - where favorites, albums, captions and
 *               people are stored: XMP sidecar files next to the
 *               photos (default), or the library database only.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QPointer>
#include <QStringList>

class KConfigGroup;

namespace Digikam
{

class MetaEngineSettingsContainer;
class ProgressItem;

/**
 * Sidecar mode uses digiKam's own settings, as the classic Settings dialog
 * would set them:
 *
 *  - everything Photos mode changes (rating, tags such as albums and people,
 *    captions, dates, positions, labels) is written to XMP sidecars only
 *    ("IMG_0001.HEIC.xmp"); the photos themselves are never modified;
 *  - sidecars are read when scanning;
 *  - a photo or sidecar changed on disk (another device of a synced folder,
 *    another application) is read again completely ("Rescan File If
 *    Modified"), so that removed tags and albums are removed here as well.
 *
 * The database stays a cache which can be rebuilt from the files.
 */
class PhotosMetadata
{
public:

    static bool sidecarMode(const MetaEngineSettingsContainer& settings);
    static void setSidecarMode(MetaEngineSettingsContainer& settings, bool sidecars);

    /// Same as setSidecarMode(true), on the "Metadata Settings" group of a config file.
    static void writeSidecarDefaults(KConfigGroup& group);
};

/**
 * Writes the favorites, albums and people that only exist in the database
 * (set before sidecar mode was enabled, or by the classic interface with
 * database-only settings) to sidecars, once. A photo whose file changes
 * later is read again from its files: without this, that information would
 * be lost.
 */
class PhotosSidecarSync : public QObject
{
    Q_OBJECT

public:

    explicit PhotosSidecarSync(QObject* const parent = nullptr);
    ~PhotosSidecarSync() override;

    /// Starts when pending (saved in the config) and in sidecar mode.
    void startIfPending();

    /// Marks it pending (sidecar mode was just enabled) and starts it.
    void restart();

    /// Percent, or -1 when not running.
    int  progress() const;

Q_SIGNALS:

    void signalProgressChanged();

private:

    void setProgress(int percent);
    void finish();

private:

    QPointer<ProgressItem> m_tool;
    int                    m_progress = -1;
};

/**
 * Optionally hides the sidecars in file managers, without renaming them
 * (other applications still read them):
 *
 *  - Windows: the hidden file attribute;
 *  - macOS:   the hidden flag of Finder (UF_HIDDEN, as "chflags hidden");
 *  - Linux:   the ".hidden" file of each folder, a list of names hidden by
 *             GNOME Files, Dolphin and others; entries of the user are kept.
 *
 * Attributes and flags do not travel with synced files: each computer
 * applies them again (at start, then for each sidecar written). ".hidden"
 * files do travel, and are only read by Linux file managers.
 */
class PhotosSidecarVisibility : public QObject
{
    Q_OBJECT

public:

    explicit PhotosSidecarVisibility(QObject* const parent = nullptr);
    ~PhotosSidecarVisibility() override;

    bool hidden() const;

    /// Saves the setting and hides or shows the sidecars of all library folders.
    void setHidden(bool hidden);

    /// Applies the setting to the folders of these files, a bit later
    /// (their sidecars may still be being written).
    void scheduleFolders(const QStringList& filePaths);

    /// Hides or shows the sidecars of one folder (not its subfolders).
    static void applyToFolder(const QString& folder, bool hide);

    /// Same, for a folder and all its subfolders.
    static void applyToTree(const QString& root, bool hide);

Q_SIGNALS:

    void signalHiddenChanged();

private:

    void applyAll();
    void slotApplyScheduled();

private:

    bool          m_hidden = false;
    QStringList   m_scheduled;
    QObject*      m_timer  = nullptr;
};

} // namespace Digikam
