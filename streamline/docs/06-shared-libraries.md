# Photos mode: a library shared by two people or computers

This is the setup for two people, for example a couple, one with an iPhone and the other with an Android phone, who share one photo library across their computers. The library is shared through a NAS or a sync tool that keeps real files on each computer: Syncthing, Nextcloud, Google Drive, Dropbox, OneDrive, Synology Drive. On-demand placeholders are not supported: turn off "files on demand" / streaming, and use "Always keep on this device" or Google Drive's "mirror" mode.

## The rule

**The photos and their sidecars are shared. Each computer keeps its own database.**

| Shared (the library folder) | Per computer (never shared) |
|---|---|
| Photos and videos in `YYYY/MM` folders | digiKam's databases (`digikam4.db`, thumbnails, faces, similarity) |
| XMP sidecars: favorites, albums, captions, people, hidden | Settings (`digikam-photosrc`) |
| `.photos-imports/`: import history, inbox definitions | Hidden-file marks of sidecars (Windows, macOS) |
| `.dtrash/`: Recently Deleted | |
| Inbox folders, if inside the library | |

Two computers opening the same SQLite file through a share or a sync tool damage it. Each computer indexes the shared files with its own database, and they stay in step through the files:
- A favorite, album or caption set on one computer is written to the photo's sidecar.
- The sidecar syncs to the other computer.
- Photos there reads it again completely ("Rescan File If Modified"), including removals.

## Recommended setups

**NAS + sync (best).** The NAS holds `Photos/`, and every computer keeps a full copy through Syncthing, Synology Drive or Nextcloud.
- Works offline, at local speed.
- Folder monitoring sees changes as the sync tool writes them.
- The NAS is the always-on peer and the backup (snapshots).

**NAS share mounted directly** (SMB/NFS, as a library folder).
- One copy, but only at home.
- Shares don't report changes, so Photos rescans them every 10 minutes and when it comes to the front.

**Cloud folder** (Google Drive mirror, Dropbox, OneDrive, Nextcloud).
- Like NAS + sync, without hardware.
- Keep files on the device.

**Syncthing between the two computers.** Changes flow when both are on.

## Phones: one inbox each

| Phone | App | Settings |
|---|---|---|
| iPhone | PhotoSync, Synology Photos, Nextcloud (auto upload) | Upload originals to `PhotoInbox/Anna-iPhone` (or `Photos/Inbox/Anna-iPhone`). iOS uploads when the app runs or as background transfers allow, e.g. PhotoSync's "arriving home" trigger. Settings → Photos → Transfer to Mac or PC: "Keep Originals" (HEIC + MOV for Live Photos). |
| Android (Samsung, Pixel…) | Syncthing-Fork, Nextcloud, FolderSync | Syncthing: share `DCIM/Camera` as **Send Only** on the phone, **Receive Only** on the NAS or computer, into `…/Inbox/Ben-Galaxy`. |

In Photos, go to Settings → Inboxes → **Add an inbox…**, choose the folder, name the device ("Anna's iPhone") and choose what happens after import:
- **Move into the library.** For upload apps, and for Syncthing Send Only / Receive Only. A file leaves the inbox only after its copy in the library is verified (same size and digiKam hash).
- **Leave in the inbox.** For folders that mirror the phone both ways: deleting there would delete on the phone too. Files are imported once and skipped afterwards.

New photos are then imported by themselves:
- once each file has stopped changing for 30 seconds;
- never while a sync tool still holds it under a temporary name;
- files arriving together form one import, waiting up to 5 minutes for stragglers such as the video of a Live Photo.

A toast says "12 new photos from Anna's iPhone · Show". Imports lists the import with **Undo**, and Devices lists the phone.

**Two computers.** The inbox definitions live in `Photos/.photos-imports/inboxes.json`, so every computer syncing the library knows them.
- One computer imports each inbox: the one that added it. The other computers show "3 waiting, imported by anna-laptop" and offer **Import here** to take over.
- An inbox inside a library folder (`Photos/Inbox/…`) is the same on every computer, and digiKam does not index it.
- An inbox elsewhere is located once per computer with **Locate…**.

## What each computer does by itself

- **Database location.** If the database is inside a library folder, on a network share, or in a folder of a sync tool (recognized: Syncthing, Dropbox, Nextcloud/ownCloud, Resilio Sync, Google Drive, OneDrive, iCloud Drive), a banner offers to move it to this computer.
  - The move happens at the next start, before the database is opened.
  - digiKam's first run puts the database in the first library folder by default, which is exactly the case to avoid.
- **Rescans.** Library folders on network shares are rescanned every 10 minutes, and all library folders when Photos comes to the front after 15 minutes or more (sleep, other work). Sync tools writing local files are seen at once by folder monitoring.
- **Sync conflicts.** When both computers change the same photo before syncing, sync tools keep both sidecars:
  - Syncthing: `IMG.JPG.sync-conflict-…xmp`;
  - Dropbox / Nextcloud: `IMG.JPG (… conflicted copy …).xmp`;
  - ownCloud: `IMG.JPG_conflict-….xmp`.

  Photos merges the copy into the photo, then puts the copy in digiKam's trash:
  - albums, people and tags of both versions are kept;
  - the higher rating is kept;
  - when the captions differ, the newer one is kept.

  A tag removed on one side and kept on the other comes back. That is the safe direction.
- **Recently Deleted.** It is shared: `.dtrash` syncs, so a photo deleted on one computer can be restored on the other.
  - A trash record carries the database id of the computer that deleted the photo. Photos does not trust that id on another computer.
  - "Delete permanently" only removes a database entry when it is this photo's entry here.

## Tested

In the test container, with a library folder standing in for a synced folder and a second computer simulated by editing the shared files:

- **Database move.** The database in `/root/Pictures` was detected. After "Move it to this computer" and a restart it was in `~/.local/share/digikam/database`, with all 3005 photos, and no database files were left in the library folder.
- **Inbox, Move mode:**
  - Five files were written Syncthing-style (temporary name, then renamed), plus one file growing for 40 s.
  - Nothing was imported while that file grew.
  - 50 s after it was complete, all six (a Live Photo included) came in as one import.
  - The inbox was then empty, and the file written in chunks was byte-identical.
- **Inbox, another computer:**
  - With the definition's importer set to "other-pc", two new files stayed ("2 waiting, imported by other-pc").
  - "Import here" took over, and they were imported.
- **Inbox, Leave mode:** a file was imported once, stayed in the inbox, and was not imported again.
- **Recently Deleted:** a trash record with another database's id (5, an unrelated visible photo here) was deleted permanently. Photo 5 was untouched.
- **Sync conflict:** a Syncthing conflict copy with an extra album, a rating of 5 and a caption was merged into the photo (database and sidecar), and the copy went to the trash.
- **Unit test:** `streamline/tests/syncnames_test.cpp` checks the file names of sync tools (temporary files, conflict copies). It runs in CI.

Not tested here:
- a real NAS share (SMB/NFS);
- a real sync tool between two machines;
- the Windows and macOS paths.

## Not done yet

- **Personal and shared libraries.** Apple's "Both / Personal / Shared" switcher, and "Move to Shared". This maps onto library folders: a local Personal folder next to the synced Shared folder.
- **Headless inbox import on the NAS.** For example `digikam --photos --import-inboxes` run on a schedule in a container, when neither computer is on.
- **"Safe to delete from the phone up to <date>"** per device.
- **Per-person favorites.** Favorites are shared, as in Apple's iCloud Shared Library.
