#!/usr/bin/env python3
"""Generate assets/uv_grid.png — the test image Lesson 4.7 samples.

Designed so that ORIENTATION IS READABLE, by eye and by a program: each corner
carries a distinct flat colour, so a harness can sample four points and say which
way up the texture arrived without a human looking at it. Lesson 3.9 established
that OBJ counts v upwards and every GPU texture counts it downwards, and applied
the flip at import — a claim no image has ever been available to test.

256x256, so it is a power of two (mipmaps, Module 6) and every feature lands on
an exact texel boundary.
"""
import struct, zlib

N = 256

# Corner colours, in TEXTURE space: (0,0) is top-left, v increases downwards.
TL = (222,  70,  60)    # red
TR = ( 82, 190, 110)    # green
BL = ( 80, 130, 230)    # blue
BR = (238, 214, 150)    # pale
CORNER = 40             # size of each corner block, in texels

DARK = (34, 37, 46)
LIGHT = (208, 212, 222)
GRID = (250, 190, 70)   # amber gridlines, one per 32 texels


def pixel(x, y):
    # Corner blocks first, so they win over everything.
    if x < CORNER and y < CORNER:                 return TL
    if x >= N - CORNER and y < CORNER:            return TR
    if x < CORNER and y >= N - CORNER:            return BL
    if x >= N - CORNER and y >= N - CORNER:       return BR

    # An arrow pointing to SMALL v — "up" in texture space. The single feature
    # that makes a vertical flip obvious at a glance rather than by comparison.
    ax, ay = x - N // 2, y - N // 2
    if -70 <= ay <= 70:
        if ay < -20:
            # the head: a triangle widening downwards from the tip
            if abs(ax) <= (ay + 70) * 0.75:       return GRID
        elif abs(ax) <= 14:
            # the shaft
            return GRID

    # Gridlines every 32 texels, one texel wide.
    if x % 32 == 0 or y % 32 == 0:                return GRID

    # A checkerboard of 16-texel cells underneath, for the filtering comparison.
    return LIGHT if ((x // 16) + (y // 16)) % 2 == 0 else DARK


def main():
    rows = []
    for y in range(N):
        row = bytearray([0])          # filter byte 0 (None) per scanline
        for x in range(N):
            row += bytes(pixel(x, y))
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))

    with open("assets/uv_grid.png", "wb") as fh:
        fh.write(png)
    print(f"wrote assets/uv_grid.png  ({len(png):,} bytes, {N}x{N})")


if __name__ == "__main__":
    main()
