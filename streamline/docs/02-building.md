# Building and running Photos mode

Photos mode is part of the normal digiKam build. It is compiled when Qt Quick
(`Qt6Quick`, `Qt6QuickWidgets`) is available, and you start it with:

```sh
digikam --photos
```

Running `digikam` without the flag starts stock digiKam, which is unchanged.

## What this snapshot of digiKam needs

| Dependency | Minimum | Ubuntu 24.04 ships | Where to get it on 22.04 / 24.04 |
|---|---|---|---|
| Qt 6 (incl. Quick, QuickWidgets, WebEngine) | 6.5 | 6.4 | KDE neon repository |
| KDE Frameworks 6 | 6.5 | – | KDE neon repository |
| OpenCV | 4.8 | 4.6 | KDE neon repository (4.10) |
| Exiv2 | 0.28 in practice | 0.27 | Ubuntu 25.10 `.deb` files (see below) |

Ubuntu 26.04 should have all of these in its own archive, so the KDE neon and
Exiv2 steps below are not needed there.

About Exiv2: digiKam's CMake says 0.27.1 is enough, but the 0.27 code path in
this snapshot no longer compiles (`MetaEngineData::Private::size()` uses
0.28-only API). Use Exiv2 0.28.

## Ubuntu 24.04 (tested in a headless container)

```sh
# 1. KDE neon user repository: Qt 6.11, KF6 6.30, OpenCV 4.10 (prebuilt).
curl -sSL https://archive.neon.kde.org/public.key | sudo gpg --dearmor -o /usr/share/keyrings/neon.gpg
echo "deb [signed-by=/usr/share/keyrings/neon.gpg] https://archive.neon.kde.org/user noble main" \
    | sudo tee /etc/apt/sources.list.d/neon.list
sudo apt update

# 2. Build dependencies.
sudo apt install --no-install-recommends \
  kf6-extra-cmake-modules qt6-base-dev qt6-base-private-dev qt6-declarative-dev \
  qt6-webengine-dev qt6-networkauth-dev qt6-svg-dev qt6-scxml-dev qt6-multimedia-dev \
  qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
  libkf6xmlgui-dev libkf6coreaddons-dev libkf6config-dev libkf6service-dev \
  libkf6windowsystem-dev libkf6solid-dev libkf6i18n-dev libkf6iconthemes-dev \
  libkf6notifications-dev libkf6notifyconfig-dev libkf6threadweaver-dev \
  libkf6sonnet-dev libkf6kio-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  qml6-module-qtquick-templates qml6-module-qtqml-models \
  libopencv-dev liblcms2-dev libjpeg-dev libtiff-dev libpng-dev libboost-dev \
  libexpat1-dev libeigen3-dev libheif-dev libx265-dev libxml2-dev libxslt1-dev \
  liblensfun-dev flex bison gettext pkg-config \
  libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev \
  libswscale-dev libswresample-dev libgl-dev libglu1-mesa-dev ninja-build ccache

# 3. Exiv2 0.28.5 from Ubuntu 25.10 (only needs libraries 24.04 already has).
sudo apt install libinireader0
for f in libexiv2-28_0.28.5+dfsg-1_amd64.deb libexiv2-dev_0.28.5+dfsg-1_amd64.deb \
         libexiv2-data_0.28.5+dfsg-1_all.deb; do
  curl -sSfLO https://archive.ubuntu.com/ubuntu/pool/main/e/exiv2/$f
done
sudo apt remove libexiv2-dev            # the 0.27 headers
sudo dpkg -i --auto-deconfigure libexiv2-*.deb
sudo dpkg -r libexiv2-27                # nothing else depends on it in a fresh install
```

On 22.04 use the `jammy` neon repository instead of `noble` (untested).

## Configure and build

```sh
mkdir -p ~/build/digikam && cd ~/build/digikam
cmake -G Ninja ~/src/digiKam \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/opt/digikam \
  -DBUILD_TESTING=OFF -DENABLE_SHOWFOTO=OFF -DENABLE_GUI_TRANSLATIONS=OFF \
  -DENABLE_INTERNALMARIADB=OFF -DBUILD_WITH_CCACHE=ON
```

Check that the configure output contains:

```
-- Photos mode (Qt Quick front-end) will be compiled...... YES
```

Then either build everything and install:

```sh
ninja && sudo ninja install
/opt/digikam/bin/digikam --photos
```

or, for a faster development loop, build only the application and the image
loader plugins, and run it from the build directory:

```sh
ninja digikam DImg_JPEG_Plugin DImg_PNG_Plugin DImg_TIFF_Plugin DImg_RAW_Plugin \
      DImg_HEIF_Plugin DImg_PGF_Plugin DImg_QImage_Plugin
~/src/digiKam/streamline/scripts/run-dev.sh ~/build/digikam --photos
```

A full build of everything takes a long time (3400+ steps). The `digikam`
target alone skips the ~70 tool plugins (about half the build).

## Settings and data

* Photos mode stores its own settings in `~/.config/digikam-photosrc`. On the
  first start it copies `~/.config/digikamrc`, so it opens the same
  collections and databases as your stock digiKam.
* After that the two settings files are independent. Delete
  `digikam-photosrc` to re-seed it from `digikamrc`.
* Databases, thumbnails, sidecars and metadata are shared with stock digiKam
  and written through the same code. Nothing is stored that stock digiKam
  does not understand.
* Do not run stock digiKam and Photos mode at the same time on the same
  SQLite database.
