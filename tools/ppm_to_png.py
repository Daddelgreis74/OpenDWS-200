#!/usr/bin/env python3
"""Convert 320x240 PPM preview frames into 2x pixel-exact PNG screenshots."""

import glob
import os
import sys
from PIL import Image


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: ppm_to_png.py <src_dir> <dst_dir>", file=sys.stderr)
        return 2

    src_dir, dst_dir = sys.argv[1], sys.argv[2]
    os.makedirs(dst_dir, exist_ok=True)
    files = sorted(glob.glob(os.path.join(src_dir, "*.ppm")))
    if not files:
        print(f"No PPM files found in {src_dir}", file=sys.stderr)
        return 1

    for src in files:
        base = os.path.splitext(os.path.basename(src))[0] + ".png"
        dst = os.path.join(dst_dir, base)
        with Image.open(src) as img:
            scaled = img.resize((img.width * 2, img.height * 2), Image.Resampling.NEAREST)
            scaled.save(dst)
        print(f"Saved {dst}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
