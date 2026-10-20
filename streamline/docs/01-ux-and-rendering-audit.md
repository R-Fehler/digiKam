# Streamline audit 01: pro-photographer UX, rendering and caching, compatibility

This is the first step toward a consumer-style front end on top of digiKam, aimed at people who mostly take photos on a phone. The look and feel we want is Apple Photos or Google Photos. The one hard rule is that **everything under the hood stays compatible with stock digiKam**. Users must be able to delete our build, start stock digiKam, and carry on with the same library.

Every finding comes from reading the source at the imported snapshot (`04e2d7b`). File references are relative to `core/` unless noted. Nothing was profiled at runtime yet, so all performance statements are code-level analysis. They still need measuring.

---

## 0. Summary

1. **Gaps and text under thumbnails come from the delegate, not the layout.**
   - The cell has hard-coded padding (`margin=5`, `radius=3`), a 1 px border, a drop shadow and 10 px spacing.
   - Letterboxing keeps the full image inside a square, so it is never cropped.
   - Seven to ten info lines can be drawn under each thumbnail.
   - With every option turned off, two square photos are still about 26–28 px apart.
   - Fix: write a new delegate with zero padding, a 1–2 px seam, a centre-crop and no text. It plugs into the existing view without touching the classic delegate (§2).
2. **Zooming is slow for structural reasons.**
   - The thumbnail cache key includes the exact pixel size (`…-thumbnail-<size>`).
   - Each 8 px zoom step therefore misses the cache for every visible tile.
   - Each tile is then re-read from the thumbnail DB, PGF-decoded, rescaled and converted on the GUI thread.
   - Until the new image arrives, the tile is drawn **blank**.
   - A 300 ms debounce means nothing moves while you zoom, and each size change triggers a full O(N) relayout (§3).
3. **Scrolling is slower than it needs to be.**
   - The first paint of each tile runs several synchronous SQL queries on the GUI thread (title, comment, tags, labels, GPS), all behind one global DB mutex.
   - The in-memory pixmap cache is about 52 MB, which is roughly 50 tiles on a HiDPI screen.
   - Off-screen thumbnail requests are never cancelled.
   - There is no kinetic scrolling (§3).
4. **The single-photo viewer works at full resolution by default.**
   - It loads the full-resolution image.
   - Every zoom step, pinch event and pan into a new area is smooth-scaled from the full-resolution image on the GUI thread.
   - The scaled-pixmap cache holds only 2 entries.
   - There is no tiling or image pyramid (§4).
5. **digiKam exposes about 300 settings, 11 menus, 9 + 7 sidebar tabs, a 9-page setup wizard and about 9 different ways to view a photo.** Most of these serve RAW and DAM workflows. §5 lists what to keep, simplify or hide.
6. **Compatibility is achievable if we follow a short list of rules (§1).**
   - Never touch the DB schema or its version keys.
   - Use our own config file instead of `digikamrc`.
   - Write metadata through digiKam's own `DMetadata`/`FileActionMngr` code paths.
   - Keep any data of our own in a separate file.

---

## 1. Compatibility rules (how to stay droppable)

### 1.1 Databases: do not change, only add

| DB | File | Schema version (code) | Version keys in `Settings` |
|---|---|---|---|
| Core | `digikam4.db` | 17 (`libs/database/coredb/coredbschemaupdater.cpp:48`) | `DBVersion`, `DBVersionRequired` |
| Thumbnails | `thumbnails-digikam.db` | 3 (`libs/database/thumbsdb/thumbsdbschemaupdater.cpp:40`) | `DBThumbnailsVersion[Required]` |
| Faces | `recognition.db` | 6 (`libs/facesengine/facedb/facedbschemaupdater.cpp:35`) | `DBFaceVersion[Required]` |
| Similarity | `similarity.db` | 1 (`libs/database/similaritydb/similaritydbschemaupdater.cpp:41`) | `DBSimilarityVersion[Required]` |

- All schemas live in `data/database/dbconfig.xml.cmake.in`.
- If `DBVersionRequired` is higher than a binary's schema version, stock digiKam **refuses to open the DB** (`coredbschemaupdater.cpp:253-280`).
- **Rule:** never write `DB*Version*` keys. Never alter existing tables or columns. Only bump the schema if upstream did.
- Stock digiKam ignores unknown tables, unknown `Settings` keys and unknown `TagProperties` names. Even so, the safest place for our own data is **a separate SQLite file**, for example `streamline.db` next to `digikam4.db`, keyed by `Images.id` or `uniqueHash`. Two reasons:
  - `ItemScanner::cleanDatabaseMetadata()` wipes `ImageProperties` during a full rescan (`libs/database/item/scanner/itemscanner_database.cpp:242`).
  - The SQLite↔MariaDB migration tool copies only the tables it knows about (`libs/database/coredb/coredbcopymanager.cpp:80-130`).
- The thumbnail DB is only a cache. It stores **one** size per image (whichever app generated it first) and scales on load (`libs/threadimageio/thumb/thumbnailcreator.cpp:340`).
  - Storing larger thumbnails (512 instead of 256) is backward compatible: stock simply scales them down.
  - The reverse case gives blurry tiles until a rebuild.

### 1.2 Config: use our own rc file

- Today `KAboutData("digikam", …)` (`app/main/main.cpp:215`) and `KSharedConfig::openConfig()` together give `~/.config/digikamrc`.
- If both apps share this file, they overwrite each other's window, view **and metadata-writing** settings.
- **Rule:** keep the component and data name `digikam`, because `~/.local/share/digikam/*` resolves through it (templates, queue.xml, breeze.rcc, the camera list…). Give our build a different *main config* name, for example via `KConfig::setMainConfigName("digikam-streamlinerc")` before the first `openConfig()`.
- Read the `Database Settings` group from `digikamrc` (read-only) so both apps open the same library.
  - Caution: `main.cpp:387` calls `params.writeToConfig()` unconditionally. Our build must not write it back into `digikamrc`.
- Keep `digikam_systemrc` shared. It holds OpenGL/OpenCL and AI model-path settings that are per machine.
- Hard-coded `digikamrc` users still exist (print and HTML-gallery plugins). That is harmless.

### 1.3 Files on disk: reuse digiKam's code, never re-implement it

- **XMP sidecars:** `file.ext.xmp`, or `file.xmp` when "compatible file name" is set (`libs/metadataengine/engine/metaengine_fileio.cpp:42`). We must honour the same `Metadata Settings`, or the two apps will write different sidecar names. The easiest way is to share these keys, or copy them, from `digikamrc`.
- **Metadata written into files:** `Xmp.digiKam.TagsList`, `ColorLabel`, `PickLabel`, `ImageHistory`, `ImageUniqueID`, plus standard EXIF/IPTC/XMP and MWG face regions. Always go through `DMetadata`, `MetadataHub` or `FileActionMngr` so that file contents and DB change signals match stock behaviour.
- **Edits / versioning:** `<base>_v<N>.<ext>` next to the original, linked through `ImageHistory`/`ImageRelations` (`libs/versionmanager/versionnamingscheme.cpp:76`). We can hide this in the UI ("Edited" badge, "Revert"), but the files and DB rows must look exactly as stock creates them.
- **Trash:** `<root>/.dtrash/`.
- **Internal tags:** `_Digikam_Internal_Tags_`. Never rename or delete them.

### 1.4 "Favorites", "Albums" and other consumer concepts map onto existing data

| Consumer concept | Stored as (stock-compatible) |
|---|---|
| Favorite ♥ | Rating, e.g. ★5 or ≥ ★4, decided once. Alternatives: Pick label = Accepted, or a tag. **Recommendation:** rating, because phones and Lightroom export it as `xmp:Rating`. |
| Album (virtual, Google/Apple style) | A tag under a dedicated parent tag, e.g. `Albums/…`. It shows up as a normal tag in stock digiKam. |
| Folder | A digiKam "album" (physical directory). |
| Library folders | Collections / album roots. |
| People | Face tags (unchanged). |
| Places | GPS (`ImagePositions`) and the map search. |
| Edited / Original | Versioning (unchanged engine). |

---

## 2. The grid: why it looks like it does, and how to get a "tiny seam"

### 2.1 Class structure

```
QListView → DCategorizedView → ItemViewCategorized → ItemCategorizedView → DigikamItemView   (main grid)
QAbstractItemDelegate → DItemDelegate → ItemViewDelegate → ItemDelegate → DigikamItemDelegate (classic look)
                                                                       → ItemThumbnailDelegate (thumbbar; already minimal)
```

- The layout is a **fixed uniform grid**: `setUniformItemSizes(true)`, every cell = `gridSize`, rows left-aligned. Leftover width is never redistributed (`libs/widgets/itemview/dcategorizedview_p.cpp:99-261`).
- `DigikamItemView` calls `setSpacing(10)` (`app/items/views/digikamitemview.cpp:86`). The spacing goes into the delegate's `gridSize`; `QListView::spacing()` stays 0.

### 2.2 What creates the gaps (and has no setting today)

| Source | Where | Effect |
|---|---|---|
| `margin = 5`, `radius = 3` | `libs/widgets/itemview/itemviewdelegate_p.h:91-92` | Inner padding around the image |
| Spacing 10 | `digikamitemview.cpp:86` | Gap between cells |
| Cell background in `Base` with a 1 px `Midlight` border | `itemviewdelegate.cpp:721-744` | Visible "card" |
| Fuzzy drop shadow | `ditemdelegate.cpp:73-106`, `itemviewdelegate.cpp:262-314` | Frame around the image |
| Fit-inside-square (letterboxing) | `itemviewdelegate.cpp:262-314`, `itemthumbnailmodel.cpp:207-252` | Empty bands next to non-square photos |
| Selection fills the whole cell with `Highlight` | `itemviewdelegate.cpp:721-744` | Heavy blue tiles |
| Dotted focus rect, 3 px hover rect | `itemviewdelegate.cpp:473-485, 660-669` | Visual noise |

Net result with **all** text turned off: cell = thumb+16 × thumb+18, pitch = thumb+26 × thumb+28.

### 2.3 Text and badges per tile (defaults)

Settings live in `libs/settings/applicationsettings.cpp:129-145`. Users can toggle them in Setup → Views → Icons.

| On by default | Off by default |
|---|---|
| Name, Title, Comments, Date, Tags, Rating, Pick label, Color label line, **Image format badge ("JPG"/"CR2")**, GPS globe, rotate buttons on hover, fullscreen button on hover | Modification date, Resolution, Aspect ratio, File size |

Also always present: the selection +/- hover button, invisible click hotspots for rating, group and GPS, and the "group" icon on grouped items.

### 2.4 Category headers

- By album (default), format, faces, **month** or **day**. Month and day already exist in `ItemSortSettings::CategorizationMode`.
- Each header is a full-width `Highlight`-coloured bar with bold text plus an "N Items" subline (`app/items/utils/itemcategorydrawer.cpp`).
- Headers are not sticky.

### 2.5 Proposed `PhotosItemDelegate` (low risk, additive)

This is a new sibling of `DigikamItemDelegate` (subclass `ItemDelegate`). `ItemThumbnailDelegate` is a good template.

- `margin = radius = 0`, `drawFocusFrame = drawMouseOverFrame = false`. No text rects. `coordinatesRect`/`groupRect` null.
- Override `setDefaultViewOptions` to remember the viewport width, then compute the cell size so columns **fill the width exactly**:
  - `n = max(1, round((vw + gap) / (target + gap)))`
  - `cell = (vw − (n−1)·gap) / n`
  - `gridSize = cell + gap`, with gap = 1–2 px
  - Watch out for scrollbar show/hide oscillation.
- Override `paint()`:
  - Draw the **centre-cropped** pixmap.
  - Show selection as a tint plus a check-circle.
  - Show only a video duration badge, a ♥ for favourites, and a small "stack" badge for groups.
  - Call `updateActualPixmapRect` and `drawOverlays`.
- Crop needs more pixels. The short side of the thumbnail must be at least the cell size: a 3:2 photo stored at 256 px long side is only 170 px short side. So request `cell × long/short` (capped), and **default to large (512) thumbnails** in our build.
- Pair the delegate with a minimal header drawer: plain bold date text on the background, no bar. `ItemCategoryDrawer`'s text helpers need to become `protected`, or take a "minimal" flag.
- Hook it into `DigikamItemView` (`digikamitemview.cpp:82-98`, `setSidebarViewMode`, `slotSetupChanged`) behind a setting. Overlay instances are bound per delegate, so the new delegate gets its own selection overlay.
- **Justified rows (Google Photos style)** cannot be done in the delegate alone. They need a rewrite of `DCategorizedView`'s layout maths (`visualRectInViewport`, `visualCategoryRectInViewport`, `intersectionSet`, `contentsSize`, `moveCursor`, `currentChanged`). Square crop is Apple's default and the right first step. Justified layout is phase 3.

---

## 3. How thumbnails are rendered and cached today

### 3.1 Pipeline

```
paint(tile) ─► ItemDelegate::retrieveThumbnailPixmap
            ─► ItemThumbnailModel::data(ThumbnailRole, size = thumbSize × dpr)
                 ├─ LoadingCache (RAM) hit  ─► QPixmap, drawn 1:1
                 └─ miss ─► queue on storageThread (read thumbnails-digikam.db only)
                              ├─ found  ─► PGF decode → smooth scale to requested size → EXIF rotate
                              └─ absent ─► defaultIconViewThread: generate
                                            JPEG: libjpeg DCT-scaled decode
                                            HEIF/RAW: embedded preview (RAW fallback: half-size demosaic)
                                            video: FFmpeg frame
                                            → scale to 256 (512 with large thumbs / HiDPI) → store as PGF in DB
            ◄─ GUI thread: QPixmap::fromImage + paint 1 px border + insert in cache → dataChanged → repaint
```

- **On disk:** one PGF thumbnail per image in `thumbnails-digikam.db`, 256 px long side by default. It is 512 with "Use large thumbnails" or on HiDPI, and 1024 on HiDPI with large thumbnails (`thumbnailcreator.cpp:80-112`). The FreeDesktop `~/.cache/thumbnails` store is only a fallback.
- **In RAM:** the `LoadingCache` singleton (`libs/threadimageio/fileio/loadingcache.cpp`) holds:
  - a QPixmap budget of about 52 MB (`setThumbnailCacheSize(10, 200)` = 200 × 256² × 4 bytes; ignores dpr);
  - a QImage budget of about 2.6 MB;
  - an image cache for previews of 6 % of RAM, clamped to 100–1024 MB.
- **Key:** `path + "-thumbnail-" + size` (`loadingdescription.cpp:141-147`), so every pixel size is a separate entry.
- **Threads:**
  - one storage-reader thread per model;
  - **one global generator thread** (`defaultIconViewThread`), shared with the thumbbars;
  - one low-priority pre-generator that fills the *disk* DB for the whole album, not RAM.
  - The queue is LIFO, so the newest request goes first. **There is no cancellation when items scroll out of view.**
- **Metadata access:** Exiv2 is serialised by one global mutex (`libs/metadataengine/engine/metaengine_p.cpp:49`).

### 3.2 Why zooming the grid stutters

1. **The 300 ms debounce** (`app/views/stack/itemiconview.cpp:214`): nothing changes visually while you zoom.
2. **A full relayout on each size change:** `DCategorizedView::slotLayoutChanged` → `rowsInsertedArtifficial` reads the category of every row (O(N)).
3. **The size-keyed cache:** each new size misses for every visible tile. Each tile is re-decoded from the DB and rescaled, then `fromImage` and the border paint run on the GUI thread.
4. **No fallback while loading:** a miss draws an empty cell (`itemviewdelegate.cpp:269`), which shows up as a blank flash.
5. **8 px steps and a minimum of 100 px:** you can't zoom out to a dense "year" overview (`itemiconview_zoom.cpp:166-177`, `libs/widgets/mainview/dzoombar.cpp:88`).

### 3.3 Why scrolling is slower than it could be

1. **Lazy SQL on the GUI thread at first paint**, behind the global `CoreDbAccess` mutex. With default settings each tile triggers `title()`, `comment()`, `tagIds()` (tags, pick and colour labels) and `hasCoordinates()` (`app/items/delegate/itemdelegate.cpp:293-390`). A text-free delegate removes most of this. The lister already pre-fills name, rating, dates, size and format.
2. **A RAM cache that is too small for HiDPI:** about 50 tiles at 2× and about 12 tiles at 2× with large thumbnails, so scrolling back up reloads from the DB.
3. **No cancellation:** a fast fling queues hundreds of loads for tiles that are already gone, and the tiles you stop on wait behind them. `findExistingTask` also does a linear scan with `dynamic_cast` on every paint of a tile that is still loading.
4. **Per-paint GUI work:**
   - grouped-and-closed items are smooth-scaled on every paint (`itemviewdelegate.cpp:294`);
   - `hasGroupedImages()` scans a list linearly;
   - `filePath()` is rebuilt twice per paint.
5. **No kinetic or pixel-smooth fling.** There is no `QScroller`; scrolling is `ScrollPerPixel` with step factor 10.

### 3.4 Fixes, ranked by value per effort

All of these are front-end or cache-level changes. None of them touch persisted data.

| # | Change | Where | Effect |
|---|---|---|---|
| A | **Request a few fixed "buckets"** (e.g. 256 and 512 physical px) instead of the exact tile size. Scale at paint time with `SmoothPixmapTransform`, or keep a small scaled cache per bucket. | `ItemThumbnailModel::data`, new delegate | Zooming within a bucket costs no reload at all. This is the core of "smooth zoom". |
| B | **Fallback drawing:** if the requested size misses, draw any cached size of the same image, scaled. Add `LoadingCache::findAnySize(path)`. | `loadingcache.cpp`, delegate | No blank flashes |
| C | **Live, animated zoom:** remove or shorten the 300 ms debounce. During pinch or Ctrl+wheel, scale the current pixmaps and only re-layout when the gesture ends. Snap to **column counts** (e.g. 3/5/7/9/13/20 per row) instead of 8 px steps. Allow cells below 100 px. | `itemiconview_zoom.cpp`, `dzoombar.cpp`, `ItemCategorizedView` | Photos-like zoom feel |
| D | **Text-free delegate**: no per-tile SQL on first paint. | §2.5 | Smoother first scroll |
| E | **Larger, dpr-aware RAM cache**, e.g. budget = min(15 % RAM, 1 GB), sized in bytes at the real pixel size. | `LoadingCache::setThumbnailCacheSize` | Scrolling back is instant |
| F | **Cancel or deprioritise off-screen requests** when the viewport changes, and **prefetch ±1–2 screens** into RAM, not only into the disk DB. | `ItemThumbnailModel`, `ManagedLoadSaveThread` | Fast flings settle quickly |
| G | **More than one generator thread** for cold albums (first import). | `ThumbnailLoadThread` | First-time browsing |
| H | **Kinetic scrolling** via `QScroller` and a smaller step factor. | `ItemViewCategorized` | Trackpad and touch feel |
| I | Skip the 1 px border paint per thumbnail (`highlight=false`) in our build. | `thumbnailloadthread.cpp:587-627` | Less GUI-thread work |

A, B, E and F are upstream-friendly performance patches and could be proposed to KDE. That shrinks what we have to maintain.

---

## 4. The single-photo viewer (preview)

- **Loading** (`libs/threadimageio/preview/previewtask.cpp`):
  - Default "Preview Load Full Image Size" = **true** (`libs/settings/applicationsettings.cpp:185`), which means a full-resolution `DImg::load` of a 12–50 MP photo every time.
  - RAW in Automatic mode uses the embedded JPEG if it is at least 48 % of full size, otherwise a half-size demosaic.
  - Preview size is clamped to 640–2560 px and does not account for dpr (`libs/widgets/graphicsview/dimgpreviewitem.cpp:78`).
- **Preloading:** only the next and previous image, one at a time, after the current one finishes. Switching images runs synchronous file probes (animated-image check, motion-photo metadata parse) on the GUI thread.
- **Zoom:**
  - There is no view transform. The item's bounding rect is resized, and each newly exposed region is `smoothScaleClipped` from the **full-resolution DImg on the GUI thread** (`graphicsdimgitem.cpp:230-299`).
  - The cache is just 2 pixmaps (`dimgitems_p.h:52`), under Qt's default ~10 MB `QPixmapCache` limit.
  - Each pinch event fully re-zooms and re-scales.
  - Wheel zoom steps by ×1.2 within the range 0.1–16.
- **Fixes:**
  1. Show a screen-sized preview first; HEIC/JPEG/embedded previews are fast. Load full resolution only when the user zooms past "fit".
  2. Build a 2–3 level pyramid (½, ¼) off-thread, and pick the nearest level when scaling.
  3. During pinch or wheel, apply a cheap `QTransform` to the last pixmap; render high quality when the gesture ends.
  4. Tile the full-resolution layer (e.g. 512² tiles) and render visible tiles off-thread.
  5. Preload ±2 images at screen size, and move the animated and motion-photo probes off the GUI thread.
  6. Set `QPixmapCache::setCacheLimit` appropriately.
  7. Longer term: an OpenGL/RHI `QQuickItem` or `QOpenGLWidget` viewer. digiKam already ships a GL viewer plugin (`dplugins/generic/view/glviewer`), which can serve as a reference.

---

## 5. Pro-oriented UX that gets in a casual user's way

Verdicts: **Keep**, **Simplify**, or **Hide** (moved behind an "Advanced" switch, or simply not shown in our build).

### 5.1 Main window (`app/main/digikamui5.rc`, `DigikamApp`)

| Element | Today | Verdict |
|---|---|---|
| Menus | 11 top-level menus, ~103 static entries + ~42 from plugins | Simplify to app menu + Import / Share / Edit buttons |
| Toolbar | Image Editor, **Light Table**, **Survey**, **Batch Queue Manager**, Import, Thumbnails / Preview / Map / **Table**, Slideshow, Full screen | Replace with Import · Search · Share · Slideshow |
| Status bar | Selection text with grouped counts, "No pending metadata synchronization" + Apply, "No active filter", progress, zoom slider + 100%/fit | Hide the metadata and filter widgets. Keep a quiet progress indicator and pinch / slider zoom |
| Sort menu | 17 criteria, separate "Separation order", sub-tree toggles, group toggles, colour-managed view (F12) | Simplify to Date taken / Date added / Name / Favorites |
| Context menu | ~25 entries (Light Table, Queue, Image Quality Sorter, group submenu, labels…) | Simplify to ~8 entries: Open, Share, Favorite, Add to album, Rotate, Edit, Show in folder, Delete |
| Rename (F2) | Opens the token-based `AdvancedRenameDialog` | Simplify to inline rename |
| Help menu | Supported RAW cameras, Solid hardware list, Components info, DB statistics | Hide |

### 5.2 Left sidebar (9 tabs, `app/views/stack/itemiconview.cpp:44-191`)

| Tab | Verdict |
|---|---|
| Albums (folder tree with collection roots as top nodes) | Keep as **Folders**, hide the root-node concept |
| Dates, Timeline | Merge into the main **Library** timeline (month/day headers, zoom levels) |
| Labels (rating / pick / colour) | Hide → **Favorites** smart view |
| Tags | Hide → becomes **Albums** (tag-backed) + search |
| Search | Promote to a global search field. digiKam already has a natural-language search (`utilities/searchwindow/nlsearch/`) |
| Similarity (duplicates / image / **sketch**) | Hide → a single "Find duplicates" utility |
| Map | Keep as **Places** |
| People | Keep; run face scanning automatically and hide the scan settings |

Navigation for our build: **Library · Albums · People · Places · Favorites · Folders**, plus search.

### 5.3 Right sidebar (`ItemPropertiesSideBarDB`)

| Tab | Verdict |
|---|---|
| Properties (File / Item / Photograph / Audio-Video / "digiKam Properties" / Rights) | Simplify into one **Info** panel: date, place + mini-map, camera one-liner, size, caption, people |
| Metadata (EXIF / **Makernote** / IPTC / XMP / ExifTool) | Hide |
| Colors (histogram stats, **ICC profile**) | Hide |
| Map | Fold into Info |
| Captions (multi-language title, caption, date, pick, colour, rating, templates, "Apply to all versions") | Simplify: caption + ♥ + albums, **auto-saved** |
| Versions | Hide; show an "Edited · Revert to original" badge instead |
| Filters (text, **MIME type**, geolocation, labels, tags AND/OR, people) | Hide; replace with search + chips (Photos / Videos / Favorites) |

### 5.4 First-run wizard (`utilities/firstrun/`, 9 pages)

7 of the 9 pages are technical:
- collection root;
- **database type** (SQLite / MariaDB internal / MariaDB server);
- RAW handling;
- metadata writing;
- reduced vs full previews;
- preview vs editor on open;
- tooltips;
- a long explanation of collection scanning.

After the wizard, a "Download Required Model Files" dialog appears on **every launch** until the AI models are present (`app/main/main.cpp:450-460`).
→ **Replace** the wizard with a single "Where are your photos?" page (default `~/Pictures`), with silent sensible defaults. Download the models in the background after a one-time consent.

### 5.5 Settings dialog (`utilities/setup/`, 14 pages, 300+ controls)

| Pages | Verdict |
|---|---|
| Database, Tool-Tip (~47 checkboxes), Metadata (~46: sidecars, ExifTool, Baloo, namespaces), Templates (IPTC rights), Image Editor (versioning, save settings, **RAW demosaicing** Bilinear/VNG/PPG/AHD/DCB/DHT/AAHD…), Color Management (~19), Light Table, Survey, Geolocation API, Cameras (~34), Plugins | Hide (Advanced) |
| Collections | Simplify to a "Library folders" list; turn folder monitoring **on** |
| Views, Miscellaneous | Simplify to about 10 consumer settings: theme, language, grid density, delete confirmation, face recognition on/off, write-to-files on/off |

### 5.6 Collections and import

- Terminology: "Collection" / "Root album folder" / "Album" (= directory). Adding a collection asks for local / removable / network category. Album properties ask for "Category" and "Child of".
- Import is a **separate window**: 6 menus and 61 actions, file-renaming options, DNG conversion, scripting, and a destination prompt for every download. Date folders are **off** by default.
- "Add images / folders" also asks for a destination every time.
- → One **Import** sheet: device auto-detected, "Import N new photos" button, automatic `YYYY/MM` folders, duplicates skipped. All of this is already possible with the existing engine and settings.

### 5.7 Too many ways to view a photo

Today there are 9 ways:
- preview mode (with magnifier, exposure indicators, focus points, colour-managed toggle in its overlay);
- Image Editor window (13 menus, Save / Save As / **Save As New Version** / Export);
- **Light Table**;
- **Survey**;
- Slideshow;
- Presentation;
- OpenGL viewer;
- full screen, Table and Map view modes;
- **Showfoto** (a separate app).

→ **One** viewer with an inline **Edit** mode, plus Slideshow. Hide the rest.

The editor has **45 tool plugins**. A consumer set is about 8:
- Auto-enhance (`autocorrection`)
- Crop / aspect (`ratiocrop`)
- Rotate / straighten (`freerotation`)
- Light (`bcg`)
- Colour (`hsl`, `whitebalance`)
- Filters / B&W
- Red-eye
- Heal (`healingclone`)

Hide curves, levels, channel mixer, 8/16-bit conversion, ICC conversion, hot pixels, lens tools, shear and perspective, and the per-tool "Load / Save settings" buttons. Plugin enable/disable already exists (`utilities/setup/setupplugins.cpp`).

### 5.8 Defaults that add prompts or clutter

| Setting | Stock default | Our default |
|---|---|---|
| Sidebar edits "Apply Changes?" prompt (`sidebarApplyDirectly`) | prompt | auto-apply |
| Grouping "operate on all grouped items?" (7 operations) | Ask | Yes |
| Editor close: save / new version / discard prompt | Ask | Auto-save as new version, show "Edited" |
| Show originals **and** intermediate versions in the grid | both on | show latest version only (stack) |
| File-save options dialog; WEBP/AVIF lossless | on; lossless | off; lossy, high quality |
| RAW+JPEG and burst grouping | manual | automatic (silent) |
| Thumbnail overlays (pick, colour, format, GPS, rotate) | on | off |
| Album folder monitoring | off | on |
| Import into date folders | off | on (`YYYY/MM`) |
| Preview loads full image | on | off (screen-size first, full on zoom) |
| Category headers | by album | by **day / month** (by date taken, newest first) |
| Large thumbnails | off | on (needed for crisp centre-crop) |

**Important:** if `Metadata Settings` are shared with stock digiKam (§1.3), write-to-file policy changes made in our build also apply to stock. That is intended, because both apps must behave the same towards files.

### 5.9 Vocabulary

| digiKam | Ours |
|---|---|
| Item | Photo / Video |
| Collection, Album root, Root album folder | Library folder |
| Album (directory) | Folder |
| Tag (under `Albums/`) | Album |
| Tags / Keywords | Keywords (Advanced) |
| Rating / Pick label / Color label | ♥ Favorite (stars and labels under Advanced) |
| Versions / Original / Intermediate | Edited · Revert to original |
| Light Table, Survey, Batch Queue Manager, Maintenance, Fingerprints, Sidecars, Makernote, Color-Managed View | (hidden) |

---

## 6. Suggested architecture

**Option 1 (recommended to start): a "Photos mode" inside the existing `digikam` binary, built as a separately named executable or launched with a flag.**
- New code lives in its own folder (e.g. `core/app/streamline/`): `PhotosItemDelegate`, a minimal category drawer, a reduced `streamlineui.rc`, a simplified Info panel, a one-page first-run, and default overrides.
- Upstream files get small, well-marked hooks only: delegate selection in `DigikamItemView`, which sidebar tabs get registered in `ItemIconView`, which rc file `DigikamApp` loads, and the config file name in `main.cpp`.
- This keeps merges from upstream mechanical, and avoids the ~19 call sites that assume `DigikamApp::instance()` exists (`dio.cpp`, `dbinfoiface.cpp`, `itemalbummodel.cpp`, maintenance tools…).

**Option 2 (later, if the shell needs to diverge a lot): a separate `streamline` executable** that links `digikamcore`, `digikamdatabase` and `digikamgui`, with its own `QMainWindow`. Showfoto is a precedent for a second binary in this tree.
- Startup must mirror `main.cpp:120-480`: `DbEngineParameters` → `AlbumManager::setDatabase` → `ScanController`, thumbnails DB, `LoadingCacheInterface`, `DPluginLoader`, and so on.
- Requires routing `DigikamApp::instance()` users through an interface.
- Some needed classes (`ItemAlbumModel`, `ItemAlbumFilterModel`, `DigikamItemView`) aren't exported, so they must either be exported or built into the target.

**Option 3 (most "Apple-like", largest effort): a QML/Qt Quick UI** on top of the same models (`ItemModel`/`ItemFilterModel`/`ItemThumbnailModel` from `digikamdatabase`), using a GPU scene graph for the grid and viewer. Defer this until phases 1–2 prove out the model and cache work.

### Suggested phases

1. **Look:** separate config file. `PhotosItemDelegate` (seam, centre-crop, no text) with date headers and newest-first ordering. Reduced menus, toolbar and sidebars. Consumer defaults (§5.8).
2. **Feel:** thumbnail buckets plus fallback drawing, live animated zoom snapping to column counts, a bigger dpr-aware cache, cancel and prefetch, kinetic scrolling (§3.4). Viewer with screen-size-first loading, gesture-time transform and a pyramid (§4).
3. **Concepts:** Favorites, tag-backed Albums, Library timeline, a one-sheet import, an inline editor with about 8 tools, a single-page first run.
4. **Optional:** justified layout, a QML or OpenGL grid and viewer, sticky headers, a year/month "zoomed-out" overview.

### Measure first

Before optimising, add a reproducible benchmark:
- an album of about 20k phone photos (HEIC + JPEG, a few videos);
- log the time to first full screen of tiles, frame time while flinging, frame time during zoom, and time to sharp preview;
- run with a cold and with a warm thumbnail DB, at 1× and 2× dpr.

`QElapsedTimer` probes around `ItemThumbnailModel::data`, `slotThumbnailLoaded`, `DCategorizedView::paintEvent` and `GraphicsDImgItem::paint` are enough to start.

---

## 7. Open decisions

1. **What "Favorite" maps to:** rating ≥ 4/5 (recommended), pick = Accepted, or a tag.
2. **Albums:** tag-backed virtual albums (recommended) or physical folders only.
3. **Grid shape:** square crop first (recommended), justified later?
4. **Where it lives:** Option 1 (flag/mode in the digiKam binary) vs a separate executable from day 1.
5. **Writing to files:** should our build write ♥ / captions / albums into XMP by default (portable, touches the user's files) or keep them DB-only (stock default)?
