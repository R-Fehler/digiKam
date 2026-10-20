#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Cold thumbnail benchmark for Photos mode (X11 / Xvfb, needs xdotool, ImageMagick, bc).
#
#   coldbench.sh <loaders (0 = default)> <pregen: 0|1> <label>
#
# WARNING: deletes <library>/thumbnails-digikam.db (a cache) unless KEEPDB=1.
# Environment: LIB (photo collection with the digiKam databases, default ~/Pictures),
#              BUILD (build dir, default ~/build/digikam), DISPLAY.
# Measures, from the moment placeholder tiles appear, the time until the first
# screen is filled at 20 columns, the same after a fast scroll, and the number
# of thumbnails generated in the first 30 seconds.
# Assumes the window at the top-left of a 1600x1000 Xvfb screen without window
# manager (the grid area is cropped at fixed coordinates).
set -u
LIB=${LIB:-$HOME/Pictures}
BUILD=${BUILD:-$HOME/build/digikam}
HERE=$(cd "$(dirname "$0")" && pwd)
THREADS=$1; PREGEN=$2; LABEL=$3
export DISPLAY=${DISPLAY:-:99}
OUT=${OUT:-/tmp/photos-bench}; mkdir -p $OUT
kill $(pgrep -x digikam) 2>/dev/null; sleep 3
[ "${KEEPDB:-0}" = 1 ] || rm -f "$LIB/thumbnails-digikam.db"
python3 - <<'PY'
import re
import os
p=os.path.expanduser('~/.config/digikam-photosrc')
s=open(p).read()
if '[Photos Mode]' in s:
    s=re.sub(r'(\[Photos Mode\][^\[]*?)Columns=\d+', r'\1Columns=20', s, flags=re.S)
else:
    s+='\n[Photos Mode]\nColumns=20\n'
open(p,'w').write(s)
PY
ENVV="DIGIKAM_PHOTOS_THUMB_THREADS=$THREADS"; [ "$THREADS" = "0" ] && ENVV="PHOTOS_BENCH_DEFAULT=1"
[ "$PREGEN" = "0" ] && ENVV="$ENVV DIGIKAM_PHOTOS_NO_PREGEN=1"
xdotool mousemove 1590 990
T0=$(date +%s.%N)
(env $ENVV dbus-run-session -- "$HERE/run-dev.sh" "$BUILD" --photos > $OUT/$LABEL.log 2>&1 &)
count() { import -window root $OUT/cur.png 2>/dev/null; convert $OUT/cur.png -crop 1220x830+220+70 +repage -fill black +opaque "#E0E1E3" -fill white -opaque "#E0E1E3" -format "%[fx:round(mean*w*h)]" info:; }
measure() {   # waits for placeholders to appear then disappear; prints fill time since they appeared
  local seen=0 n ts
  for i in $(seq 1 240); do
    n=$(count)
    if [ $seen = 0 ] && [ "$n" -gt 5000 ]; then seen=1; ts=$(date +%s.%N); fi
    if [ $seen = 1 ] && [ "$n" -lt 2000 ]; then echo "$(echo "$(date +%s.%N) - $ts" | bc)"; return; fi
    sleep 0.2
  done
  echo "timeout"
}
FIRST=$(measure $T0)
sleep 1
T1=$(date +%s.%N); xdotool mousemove 800 500; for k in $(seq 1 120); do xdotool click 5; done; xdotool mousemove 1590 990
SCROLL=$(measure $T1)
sleep $(echo "30 - ($(date +%s.%N) - $T0)" | bc | awk '{print ($1>0)?$1:0}')
DB=$(python3 -c "import sqlite3;print(sqlite3.connect('$LIB/thumbnails-digikam.db').execute('select count(*) from Thumbnails').fetchone()[0])")
echo "$LABEL threads=$THREADS pregen=$PREGEN first_screen_filled_s=$FIRST after_fast_scroll_s=$SCROLL thumbs_in_db_at_30s=$DB"
