// SPDX-License-Identifier: GPL-2.0-or-later
//
// Photos mode: file names of sync tools (photossyncnames.h).
//
//   g++ -std=c++17 -fPIC -I core/app/photos $(pkg-config --cflags Qt6Core) \
//       streamline/tests/syncnames_test.cpp $(pkg-config --libs Qt6Core) -o syncnames_test

#include <cstdio>

#include "photossyncnames.h"

using namespace Digikam;

static int failures = 0;

static void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);

    if (!ok)
    {
        ++failures;
    }
}

int main()
{
    // Temporary and hidden files of sync tools are not imported.

    check( photosIsTemporaryName(QLatin1String(".syncthing.IMG_0001.HEIC.tmp")),          "Syncthing temporary file");
    check( photosIsTemporaryName(QLatin1String("IMG_0001.HEIC.part")),                    "partial download");
    check( photosIsTemporaryName(QLatin1String("~IMG_0001.JPG")),                         "~ prefix");
    check( photosIsTemporaryName(QLatin1String(".stversions/IMG_0001.JPG")),             "file in a hidden folder");
    check( photosIsTemporaryName(QLatin1String("._IMG_0001.JPG")),                        "macOS resource fork");
    check( photosIsTemporaryName(QLatin1String("IMG_0001.sync-conflict-20261009-101500-ABC1234.JPG")), "conflict copy");
    check(!photosIsTemporaryName(QLatin1String("2026/IMG_0001.HEIC")),                    "photo in a subfolder");
    check(!photosIsTemporaryName(QLatin1String("PXL_20261009_101500123.MP.jpg")),         "Pixel motion photo name");

    // Conflict copies of sidecars, and of nothing else.

    check(photosConflictOriginal(QLatin1String("IMG_0001.JPG.sync-conflict-20261009-101500-ABC1234.xmp")) ==
          QLatin1String("IMG_0001.JPG.xmp"),                                                "Syncthing sidecar conflict");
    check(photosConflictOriginal(QLatin1String("IMG_0001.JPG (Anna's conflicted copy 2026-10-09).xmp")) ==
          QLatin1String("IMG_0001.JPG.xmp"),                                                "Dropbox sidecar conflict");
    check(photosConflictOriginal(QLatin1String("IMG_0001.JPG (conflicted copy 2026-10-09 101500).xmp")) ==
          QLatin1String("IMG_0001.JPG.xmp"),                                                "Nextcloud sidecar conflict");
    check(photosConflictOriginal(QLatin1String("IMG_0001.JPG_conflict-20261009-101500.xmp")) ==
          QLatin1String("IMG_0001.JPG.xmp"),                                                "ownCloud sidecar conflict");
    check(photosConflictOriginal(QLatin1String("IMG_0001.sync-conflict-20261009-101500-ABC1234.JPG")).isEmpty(),
                                                                                            "photo conflict: not a sidecar");
    check(photosConflictOriginal(QLatin1String("IMG_0001.JPG.xmp")).isEmpty(),              "plain sidecar");
    check(photosConflictOriginal(QLatin1String("Holiday (1).jpg.xmp")).isEmpty(),           "numbered copy is no conflict");

    std::printf("%s\n", failures ? "FAILED" : "all passed");

    return failures ? 1 : 0;
}
