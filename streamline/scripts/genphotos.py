# SPDX-License-Identifier: BSD-3-Clause
# Usage: python3 genphotos.py <output-folder> <count>   (JOBS=<n> processes)
# Generate synthetic "phone photos" with EXIF capture dates for testing Photos mode.
import os, random, sys, math, datetime
from multiprocessing import Pool
from PIL import Image, ImageDraw, ImageFont

OUT = sys.argv[1]
N   = int(sys.argv[2])
random.seed(42)

def font(size):
    for p in ["/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

# Build "events": clusters of photos on the same day.
plan = []
start = datetime.datetime(2023, 1, 1)
while len(plan) < N:
    day = start + datetime.timedelta(days=random.randint(0, 1375))
    n = random.choice([1, 2, 3, 5, 8, 12, 20, 35])
    t = day.replace(hour=random.randint(7, 19), minute=random.randint(0, 59))
    hue = random.random()
    for _ in range(n):
        t += datetime.timedelta(seconds=random.randint(5, 600))
        plan.append((t, hue))
plan = plan[:N]

def hsv(h, s, v):
    i = int(h * 6) % 6; f = h * 6 - int(h * 6)
    p, q, tt = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
    r, g, b = [(v, tt, p), (q, v, p), (p, v, tt), (p, q, v), (tt, p, v), (v, p, q)][i]
    return (int(r * 255), int(g * 255), int(b * 255))

def make(args):
    idx, (t, hue) = args
    rnd = random.Random(idx)
    kind = rnd.random()
    w, h = (4032, 3024) if kind < 0.65 else ((3024, 4032) if kind < 0.9 else (1920, 1080))
    # Render small then upscale: fast, and still a realistic file size/decoding cost.
    sw, sh = w // 8, h // 8
    img = Image.new("RGB", (sw, sh))
    d = ImageDraw.Draw(img)
    hh = (hue + rnd.uniform(-0.05, 0.05)) % 1.0
    for y in range(sh):
        d.line([(0, y), (sw, y)], fill=hsv(hh, 0.55, 0.35 + 0.6 * y / sh))
    for _ in range(rnd.randint(3, 9)):
        x0, y0 = rnd.randint(0, sw), rnd.randint(0, sh)
        r = rnd.randint(sw // 12, sw // 3)
        d.ellipse([x0 - r, y0 - r, x0 + r, y0 + r], fill=hsv((hh + rnd.uniform(0.2, 0.8)) % 1, 0.6, rnd.uniform(0.5, 1)))
    d.text((sw * 0.06, sh * 0.72), t.strftime("%Y-%m-%d"), fill=(255, 255, 255), font=font(max(12, sw // 9)))
    d.text((sw * 0.06, sh * 0.86), "#%d" % idx, fill=(255, 255, 255), font=font(max(10, sw // 16)))
    img = img.resize((w, h), Image.BILINEAR)
    exif = Image.Exif()
    exif[0x010F] = "Synthetic"            # Make
    exif[0x0110] = "Phone 15"             # Model
    exif[0x0112] = 1                      # Orientation
    ifd = exif.get_ifd(0x8769)
    ifd[0x9003] = t.strftime("%Y:%m:%d %H:%M:%S")   # DateTimeOriginal
    ifd[0x9004] = t.strftime("%Y:%m:%d %H:%M:%S")   # DateTimeDigitized
    folder = os.path.join(OUT, t.strftime("%Y"), t.strftime("%m"))
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "IMG_%05d.jpg" % idx)
    img.save(path, "JPEG", quality=88, exif=exif)
    ts = t.timestamp()
    os.utime(path, (ts, ts))

if __name__ == "__main__":
    with Pool(int(os.environ.get("JOBS", "2"))) as p:
        p.map(make, list(enumerate(plan)), chunksize=16)
    print("generated", N)
