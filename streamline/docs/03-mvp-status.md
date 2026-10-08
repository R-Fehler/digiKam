# Photos mode MVP: status

Start it with `digikam --photos`. See `02-building.md` for how to build and run it.

## What was tested

The test ran in a headless Linux container: Ubuntu 24.04, KDE neon Qt 6.11, a virtual X display with software OpenGL (no GPU), and a 4-core CPU. The library had 3,000 synthetic 12 MP JPEGs with EXIF capture dates from 2023 to 2026, in `YYYY/MM` folders.

The first start went through digiKam's own first-run wizard with its default answers.

| Check | Result |
|---|---|
| Opens straight into the Photos UI, no menu or tool bars | ok |
| Library: 3,000 items, newest first, one header per day | ok |
| Square centre-cropped tiles, 2 px seam, no text, full window width | ok |
| Zoom with Ctrl+wheel: 5 → 7 → 9 → 12 → 16 → 20 columns, month headers from 8 columns | ok |
| Thumbnails already loaded: no reload or blank tiles when zooming | ok |
| Cold thumbnails (empty thumbnail DB) fill in within a few seconds per screen | ok, see "performance" |
| Viewer: click a tile, thumbnail first then preview, date and file name shown | ok |
| Viewer: wheel zoom, Esc to reset zoom and then close, arrow keys to go next/previous | ok |
| Favorite (♥ or F): rating 5 in `digikam4.db`, heart badge in grid, Favorites view | ok, kept after restart |
| Add to album "Summer 2026": tag `Albums/Summer 2026` created and assigned, listed in sidebar | ok |
| Ctrl+Shift+P switches to the classic interface and back | ok |

Screenshots: `img/mvp-grid-days.png`, `img/mvp-grid-months.png`, `img/mvp-grid-dense-loading.png`, `img/mvp-viewer.png`, `img/mvp-favorites-album.png`.

Not tested yet:

- pinch gestures and swipe;
- videos;
- HEIC and RAW files;
- HiDPI screens;
- a real GPU;
- macOS and Windows;
- MariaDB;
- write-to-file metadata settings, i.e. whether XMP sidecars get written when that is enabled.

## Round 2: selection, trash and cold-library speed

### Selection and actions

| Check | Result |
|---|---|
| Hover shows a check circle; clicking it selects the photo | ok |
| Ctrl+click toggles; Shift+click selects a range from the last toggled photo | ok (10 selected in test) |
| Drag on the grid draws a selection rectangle; auto-scrolls near the edges; Ctrl/Shift adds | ok (exactly 4 / 8 selected in tests) |
| While photos are selected, the title bar becomes "N selected" with ♥, Add to album, Move to trash, ✕ | ok |
| In selection mode a click toggles; double-click opens; Esc clears; Ctrl+A selects all | ok |
| ♥ on 10 selected photos: rating written for all 10 in `digikam4.db` | ok |
| Right-click menu: Open, Add to / Remove from Favorites, Add to album…, Select, Show in folder, Move to trash. It acts on the selection when the photo is part of it, otherwise on that photo only | ok |
| Move to trash, from the grid (button, Delete key) or the viewer (button, Delete key), with a confirmation | ok: files moved to `<collection>/.dtrash`, database status updated |
| Toast "Moved N photos to the trash · Undo" for 8 s; Undo restores the files, which reappear with their ratings | ok |
| Viewer after deleting: shows the next photo | ok |
| A photo replaced on disk by another application refreshes its tile by itself, within a few seconds | ok (needs folder monitoring, now on by default in Photos mode) |

Screenshots: `img/mvp-selection.png`, `img/mvp-drag-select.png`, `img/mvp-trash-confirm.png`, `img/mvp-trash-undo.png`, `img/mvp-context-menu.png`.

Trash and restore go through digiKam's own `DIO` code. Photos deleted in Photos mode can therefore also be restored, or the trash emptied, from the trash view of stock digiKam.

### Cold-library thumbnails: what changed

1. **Parallel loaders.** There is one thumbnail loader per CPU core, from 2 to 6, at lowered thread priority. A given file always goes to the same loader, so duplicate requests are merged.
2. **Cancel what scrolled away.** When a tile leaves the screen, its pending thumbnail request is dropped, so the loaders work on what is visible.
3. **Visible first.** Rows are pre-created 1.5 screens ahead so scrolling stays smooth. Their thumbnails, however, are only requested once a row is within half a screen of the viewport. Before this, digiKam's newest-first queue made the farthest pre-created rows load before the visible ones.
4. **Background pre-generation.** Three seconds after the library is listed, a lowest-priority loader generates missing thumbnails for the whole library into `thumbnails-digikam.db`, the same database stock digiKam uses.
5. **Stale thumbnails.** When digiKam reports that a file changed, the cached thumbnail is dropped and the tile requests it again (a version segment in the image URL).

Debug switches for benchmarks: `DIGIKAM_PHOTOS_THUMB_THREADS=<n>` and `DIGIKAM_PHOTOS_NO_PREGEN=1`.

### Measurements

Setup: an empty thumbnail database (every thumbnail generated from the 12 MP JPEG), 20 columns (about 260 tiles per screen), a 1454×909 window, and the same shared 4-core container with no GPU. Pre-generation was off unless noted. The benchmark script polls screenshots and counts placeholder pixels. Times run from the moment placeholders appear to the moment the screen is filled.

| Configuration | First screen filled | Screen filled after a fast scroll | Thumbnails generated in the first 30 s |
|---|---|---|---|
| 1 loader, before "visible first" | 19.1 s | not measurable (scroll stayed within prefetch) | 694 |
| 1 loader | 18.9 s | 10.0 s | 1,058 |
| 2 loaders | 10.6 s | 5.9 s | 1,241 |
| 4 loaders (the default on 4 cores) | 7.4–7.9 s | 3.3–3.6 s | 1,472–1,480 |
| default + background pre-generation | 7.2 s | 4.1 s | 1,626 (whole library of 2,988 done after about 55 s) |
| **next start, thumbnail database warm** | **2.9 s** | **nothing visible: tiles already filled when the screenshot was taken** | n/a |

Generation from full-size camera files is CPU bound. On this machine, 4 loaders are about 2.5× faster than the single loader of the first MVP. Machines with more cores should scale further, up to the cap of 6. Once pre-generation has run once, scrolling anywhere in the library is instant.

## Round 3: touch input, scaling to many cores, thumbnail size ladder

### Touch screens

The grid's mouse layer now ignores events synthesized from touch, so finger input reaches the list and dedicated touch handlers:

- finger drag scrolls (flick);
- tap opens a photo, or toggles it while a selection exists;
- long press selects; keeping the finger down and dragging extends the selection in order, with auto-scroll at the edges;
- pinch zooms.

The viewer already used touch-capable handlers (swipe, pinch, double tap).

**Not tested:** the test environment has no touch device. Mouse behaviour was re-checked after the change.

### Many cores

- **Loaders for visible thumbnails:** one per core, from 2 to 16 (was capped at 6). Override with `DIGIKAM_PHOTOS_THUMB_THREADS`.
- **Background pre-generation:** up to one loader per core, from 1 to 16, at the lowest priority. Override with `DIGIKAM_PHOTOS_PREGEN_THREADS`.
- **Shared thread pool:** digiKam's loaders all run on one pool of cores + 1 threads, and a loader keeps its pool thread until its queue is empty. Long pre-generation queues would therefore hold the pool and delay visible thumbnails. For this reason the broker hands out pre-generation in batches of 8 photos per loader, only while no visible thumbnail is waiting and the user has not scrolled for 0.5 s. It also pauses running batches as soon as a visible thumbnail is requested; interrupted batches are queued again.
- **Progress:** while pre-generation runs for more than 2 s, the title shows "· preparing thumbnails N%".

Measured on the 4-core container (empty thumbnail database, 20 columns, 2 runs):

| | First screen | After fast scroll | Whole library pre-generated |
|---|---|---|---|
| Round 2 (1 continuous pre-generation thread) | 7.2 s | 4.1 s | about 55 s |
| Round 3 (batched, paused for visible work) | 7.4–8.3 s | 2.6–3.0 s | 56–58 s |

Visible loading after scrolling is faster because it no longer competes with pre-generation. On machines with more than 6 cores, both visible loading and pre-generation can now use the extra cores. Use `streamline/scripts/coldbench.sh` with different `DIGIKAM_PHOTOS_THUMB_THREADS` values to find where your machine stops scaling. Likely limits are the serialized Exiv2 metadata reading and the single SQLite writer.

### Thumbnail size ladder

Thumbnails are served in fixed sizes: 192 px, 512 px, and on HiDPI screens 1024 px. Each tile picks the smallest size whose short side still covers it after the square crop. Small sizes are scaled from a larger one already in memory when available, otherwise digiKam's loader scales the stored thumbnail on its worker thread. When zooming across a size boundary, the old texture stays visible until the new size has loaded. The switch `DIGIKAM_PHOTOS_SINGLE_SIZE=1` restores the previous single-size behaviour for comparison.

Process memory after scrolling the whole library (2,988 photos) at 26 columns, with a warm thumbnail database. Rendering is software-only here, so textures live in process memory:

| | After start | After scrolling the whole library |
|---|---|---|
| Single 512 px size | 1,238 MB | 2,897 MB |
| Size ladder | 572 MB | 1,051 MB |

Zooming from 26 to 9 columns (crossing from 192 px to 512 px) showed no placeholder pixels, neither immediately nor after 1.5 s.

### Memory budget and Qt Quick's image cache

- **Our cache scales with RAM and gives memory back.** The decoded-thumbnail cache gets 20% of physical memory, between 512 MiB and 8 GiB (override: `DIGIKAM_PHOTOS_CACHE_MB`). That is about 1.6 GiB with 8 GiB of RAM.
  - Every 5 s it checks available memory. When less than max(1 GiB, 10% of RAM) is available, it gives back the missing amount (least recently used thumbnails first, never below 64 MiB) and returns the freed heap to the system (`malloc_trim` on glibc). Once twice that amount is available again, it grows back by 1/8 of the budget per check.
  - RAM figures come from KDE Frameworks' `KMemoryInfo` (Linux, macOS, Windows, FreeBSD). On Linux, a cgroup v1/v2 memory limit (Flatpak, Snap, systemd slices, containers) is also taken into account; inactive file cache counts as available, as for the kernel.
  - The policy is a pure function in `photoscachepolicy.h`, unit tested by `streamline/tests/cachepolicy_test.cpp`.
  - Live test: the cache was filled (754 MB process memory), then another process left 975 MiB available. The budget dropped to 64 MiB and process memory to 504 MB. After the other process ended, the budget grew back 342 MiB per check towards 2.7 GiB.
- **Qt Quick barely caches.** In Qt 6.11 (`qquickpixmapcache.cpp`), an image is kept while an `Image` item uses it. Once released, it goes to an LRU list of only 2 MiB (`cache_limit`), and a 30 s timer drops a quarter of that list each time. A tile that scrolls out of the pre-created rows therefore loses its image almost at once. Scrolling back is served from our cache.
- **No CPU-side duplication.** Qt Quick's texture factory keeps the image as is when it is `RGB32` or `ARGB32_Premultiplied`, sharing the pixel data with our cache (Qt's implicit sharing); for any other format it would keep a converted copy. The broker now stores thumbnails in one of these two formats.
- **One extra copy on a real GPU.** There, the texture of each on-screen tile is a second copy in video memory, which cannot be avoided. It is bounded by the number of instantiated tiles and kept small at dense zoom by the size ladder.

## Round 4: pinch on Wayland, faster full screen viewer, thumbnail strip

### Pinch to zoom

On Wayland only the +/- buttons zoomed. There were two causes, both from `QQuickWidget`, which hosts the Qt Quick scene inside the classic main window:

- **Touchpad pinch** arrives as `QEvent::NativeGesture`, and `QQuickWidget` does not pass that event on to its scene. `PhotosContainer` now forwards it to the Qt Quick window (`photosForwardNativeGesture()` in `photosgestures.h`). The `PinchHandler`s in the grid and the viewer then work on Wayland, X11 (XInput 2.4) and macOS.
- **Touch screen pinch** had several problems:
  - Qt widgets stop sending a touch point that the widget did not accept, so a second finger nobody took vanished.
  - A list that is already scrolling with one finger hides a newly pressed second finger from the handlers inside it.

  To fix this:
  - Two-finger pinch is now recognized in C++ from the touch events the widget receives (`PhotosTouchPinch`), and all points are accepted.
  - The scene gets `photosApp.multiTouch` (it stops scrolling or panning while two fingers are down) and `touchPinch*` signals, which Main.qml sends to the grid or the viewer.
- **AppImage:** the Ubuntu 24.04 AppImage now bundles the Qt Wayland platform plugin and its shell integration, decoration and graphics plugins. It runs as a native Wayland client (checked under Weston). The 22.04 compatible (Craft) AppImage already had them. Without them, the app ran through XWayland, which delivers no touchpad gestures.

`streamline/tests/gesture_test.cpp` drives the same event path through a `QQuickWidget`. It covers:

- touchpad pinch over the grid and over the viewer;
- two-finger pinch;
- a second finger landing after the first one started scrolling;
- one-finger tap and scroll.

CI builds and runs it after the build:

```
g++ -std=c++17 -fPIC -I core/app/photos streamline/tests/gesture_test.cpp \
    $(pkg-config --cflags --libs Qt6QuickWidgets Qt6Quick Qt6Test Qt6Widgets) -o gesture_test
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./gesture_test
```

**Not tested** on a real touchpad or touch screen yet. The events are simulated.

### Full screen speed

- **Preview loader.** Previews sized to the screen are decoded on their own small thread pool. That is half the cores, from 2 to 4; override with `DIGIKAM_PHOTOS_PREVIEW_THREADS`. Decoding uses digiKam's fast path: the embedded preview of RAW and JPEG files when it is large enough.
- **The photo on screen first.** Requests for the photo on screen always come first. One thread never takes prefetch work, so a jump is not stuck behind prefetching.
- **Prefetch.** The viewer prefetches the next photo, the previous one, then two more ahead and one more behind. Videos are skipped. The list is replaced at each move, so stale prefetches are dropped.
- **Preview cache.** Decoded previews stay in a memory cache of 1/16 of RAM, between 256 MiB and 2 GiB (`previewBudgetKiB()` in `photoscachepolicy.h`). That is about 40 photos at 1600 px with 8 GiB of RAM. It shrinks under memory pressure like the thumbnail cache. Full resolution (only loaded when zooming past the preview's own resolution) is not cached.
- Images are stored as `RGB32` or `ARGB32_Premultiplied`, so Qt Quick uploads them without a converted copy. They are shown without mipmaps, so changing photo does not regenerate mipmap levels.
- **Measured** in the container with 4032×3024 JPEGs: a prefetch decode takes 220–330 ms. After the first photo, moving with the arrow keys was served from the cache every time. Set `DIGIKAM_PHOTOS_TRACE=1` to log decode times and cache hits.

### Thumbnail strip

![Viewer with the thumbnail strip](img/viewer-filmstrip.jpg)

- **Layout.** A scrollable strip of small thumbnails sits under the full screen photo, as in iOS Photos or Samsung Gallery. The current photo stays in the middle, slightly larger and outlined.
- **Scrubbing.** Swiping or flicking the strip moves through the photos. While it moves, only thumbnails are shown, so scrubbing does not start full decodes. The preview loads once it stops. A tap jumps to a photo.
- **Visibility.** It shows and hides with the viewer's controls, and the controls stay up while the pointer is over the strip. The "Thumbnails" button in the top bar or the T key turns it off or on. The choice is saved (`Photos Mode` / `Filmstrip` in `digikam-photosrc`).
- **Thumbnails.** It uses the smallest thumbnail size, which the grid has usually already loaded.

## Bugs found and fixed while testing

- The grid kept a mid-library scroll position while the first collection scan was still adding photos. It now stays at the top when it is at the top.
- Thumbnails from the classic views' shared pixmap cache have a painted 1 px border. Photos mode now only takes the clean QImage results.
- Tiles were soft. Photos mode now enables digiKam's "Use Large Thumbs" setting (512 px) in its own config.
- The classic main window took Escape and +/- away from the Qt Quick scene. Photos mode now claims the plain keys it uses while it has the focus.
- The "Download required model files" dialog appeared on every start. It is skipped in Photos mode, which uses no AI feature yet. Classic mode is unchanged.

## Performance notes

These are only indicative: there was no GPU and the CPU was shared with the build.

- **Warm cache.** With thumbnails already in memory, zooming the grid is instant. Each tile is just a GPU-scaled texture.
- **Cold cache.** See the measurements above. Several loaders work in parallel, and requests for tiles that scrolled away are cancelled. Once background pre-generation has covered the library, scrolling stays fully filled.
- **Library query.** The read-only query for 3,000 items and its model reset feel instant. 50k+ items has not been measured.

## Known gaps and next steps

1. **Sharing and export** of selected photos: copy to a folder, email, and the existing export plugins.
   Import, sidecars, devices, library views, video and the info panel are in `05-files-import-and-views.md`.
2. **Touch testing** on a real touch screen and touchpad (implemented and tested with simulated events only).
4. **First run.** Replace the 9-page wizard with a single "Where are your photos?" page when started with `--photos`.
5. **Import sheet** for phones and cards, with automatic `YYYY/MM` folders.
6. **People and Places** views using the existing face tags and GPS data.
7. **Inline edit mode** with the about 8 consumer tools listed in the audit.
8. **Videos.** Inline playback; currently the system player is used.
9. **Dev runner limitation.** `run-dev.sh` stages the data files but not the classic XMLGUI menu files. When running from the build tree, the classic interface therefore shows only Settings/Help menus. An installed build (`ninja install`) is complete.

## Footprint in stock digiKam files

All of the new code is in `core/app/photos/`. The changes to stock files are:

| File | Change |
|---|---|
| `core/app/main/main.cpp` | `PhotosMode::preInitialize()` (own config file, Photos defaults applied once: folder monitoring and large thumbnails), `--photos` option, hand-over of `--photos <path>` to a running Photos mode (`PhotosMode::forwardToRunningInstance()`), skip the model download prompt in Photos mode |
| `core/app/main/digikamapp_setup.cpp` | wrap the classic view in the Photos container |
| `core/app/main/digikamapp.cpp` | `PhotosMode::finalizeMainWindow()` |
| `core/app/main/digikamapp_p.h` | include |
| `core/app/DigikamGuiTarget.cmake` | include `photos/PhotosMode.cmake` |

Every hook is behind `#ifdef HAVE_PHOTOSMODE`, which is only defined when Qt Quick is found.
