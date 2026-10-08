#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Build and package digiKam (with Photos mode) for Windows or macOS with
# KDE Craft, the way KDE's own CI builds digiKam's release bundles
# (sysadmin/craft-ci gitlab-templates/blocks/{windows,macos}-base.yml),
# minus code signing and notarization.
#
#   python3 streamline/ci/craft-build.py --target windows-msvc2022_64-cl \
#           --root C:/CR --out dist
#
# Dependencies come from KDE's binary cache; digiKam is compiled from this
# source tree (Craft's "srcDir" option) with the official blueprint
# (craft-blueprints-kde extragear/digikam).

import argparse
import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path

SRC_DIR = Path(__file__).resolve().parents[2]
PACKAGE = "digikam"


def group(title):
    if os.environ.get("GITHUB_ACTIONS"):
        print(f"::endgroup::\n::group::{title}", flush=True)
    else:
        print(f"--- {title}", flush=True)


def run(cmd, **kwargs):
    print("+", subprocess.list2cmdline([str(c) for c in cmd]), flush=True)
    return subprocess.run([str(c) for c in cmd], check=True, **kwargs)


def clone(url, dest):
    if not (dest / ".git").exists():
        run(["git", "clone", "--depth=1", url, dest])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, help="Craft target, e.g. windows-msvc2022_64-cl or macos-arm-clang")
    parser.add_argument("--root", required=True, help="Craft working directory (short path on Windows)")
    parser.add_argument("--out", required=True, help="Where the packages are copied")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = Path(args.out).resolve()
    root.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    os.environ.setdefault("CRAFT_PYTHON", str(Path(sys.executable).parent))
    os.environ["PYTHONUTF8"] = "1"

    group("Fetching CraftMaster and KDE's Craft CI configuration")

    clone("https://invent.kde.org/packaging/craftmaster.git", root / "craftmaster")
    clone("https://invent.kde.org/sysadmin/craft-ci.git", root / "craft-ci")

    craftmaster = [
        sys.executable, "-u", root / "craftmaster" / "CraftMaster.py",
        "--config", root / "craft-ci" / "qt6" / "CraftConfig.ini",
        "--config-override", SRC_DIR / "streamline" / "ci" / "craft-override.ini",
        "--target", args.target,
    ]

    src_option = f"{PACKAGE}.srcDir={SRC_DIR.as_posix()}"

    group("Setting up Craft")
    run(craftmaster + ["--setup"])
    run(craftmaster + ["-c", "-i", "--options", "virtual.ignored=True", "--update", "craft"])

    # Our copy of the digiKam blueprint (see its header for the differences).

    blueprints = glob.glob(str(root / "**" / "craft-blueprints-kde" / "extragear" / "digikam" / "digikam.py"), recursive=True)

    if not blueprints:
        print("KDE's digiKam blueprint was not found below", root)
        return 1

    for blueprint in blueprints:
        shutil.copy2(SRC_DIR / "streamline" / "ci" / "craft" / "digikam.py", blueprint)
        print("Installed", blueprint)

    group("Installing dependencies (KDE binary cache)")
    run(craftmaster + ["-c", "--install-deps", PACKAGE])

    group("Building digiKam")
    run(craftmaster + ["-c", "--no-cache", "--options", src_option, PACKAGE])

    if args.target.startswith("windows"):
        group("Installing NSIS")
        run(craftmaster + ["-c", "-i", "--update", "nsis"])

    group("Packaging")
    run(craftmaster + ["-c", "--package", "--options", src_option, PACKAGE])

    package_dir = subprocess.run(
        [str(c) for c in craftmaster + ["-c", "-q", "--get", "packageDestinationDir()", "virtual/base"]],
        check=True, capture_output=True, text=True,
    ).stdout.strip().splitlines()[-1]

    copied = []

    for pattern in ("*.exe", "*.7z", "*.dmg", "*.sha256"):
        for f in glob.glob(os.path.join(package_dir, "**", pattern), recursive=True):
            shutil.copy2(f, out)
            copied.append(os.path.basename(f))

    print("::endgroup::" if os.environ.get("GITHUB_ACTIONS") else "", flush=True)
    print("Packages:", ", ".join(copied) if copied else "none")

    return 0 if copied else 1


if __name__ == "__main__":
    sys.exit(main())
