#!/usr/bin/env python3
"""Crop a P6 ppm to (x0, y0)-(x1, y1) and box-average it to half resolution.

    python3 scratch/crop_ppm.py in.ppm out.ppm x0 y0 x1 y1

Half resolution because every render figure box-samples its input anyway; keeping
the full-resolution crop would track four times the bytes to draw the same SVG.
"""
import sys


def main():
    src, dst, x0, y0, x1, y1 = sys.argv[1], sys.argv[2], *map(int, sys.argv[3:7])
    with open(src, "rb") as fh:
        assert fh.readline().strip() == b"P6"
        w, h = map(int, fh.readline().split())
        assert fh.readline().strip() == b"255"
        data = fh.read()
    ow, oh = (x1 - x0) // 2, (y1 - y0) // 2
    out = bytearray()
    for y in range(oh):
        for x in range(ow):
            for c in range(3):
                s = 0
                for dy in (0, 1):
                    for dx in (0, 1):
                        s += data[((y0 + 2 * y + dy) * w + (x0 + 2 * x + dx)) * 3 + c]
                out.append((s + 2) // 4)
    with open(dst, "wb") as fh:
        fh.write(b"P6\n%d %d\n255\n" % (ow, oh) + bytes(out))
    print(f"crop_ppm: {dst} {ow}x{oh}")


if __name__ == "__main__":
    main()
