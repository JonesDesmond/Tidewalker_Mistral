#!/usr/bin/env python3
"""Generates packaging/linux/tidewalker-icon.png (64x64): a pale sun over dark waves."""
import struct, zlib, math, os

W = H = 64
px = [[(10, 24, 38)] * W for _ in range(H)]

def circle(cx, cy, r, c):
    for y in range(H):
        for x in range(W):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                px[y][x] = c

def wave(y0, amp, col):
    for x in range(W):
        y = int(y0 + amp * math.sin(x / 6.0))
        for dy in range(3):
            if 0 <= y + dy < H:
                px[y + dy][x] = col

circle(32, 22, 10, (250, 214, 120))
wave(38, 4, (30, 80, 110))
wave(46, 5, (24, 64, 92))
wave(54, 6, (18, 48, 70))

raw = b"".join(b"\x00" + b"".join(bytes(px[y][x]) for x in range(W)) for y in range(H))
def chunk(t, d):
    c = struct.pack(">I", len(d)) + t + d
    return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(raw, 9))
       + chunk(b"IEND", b""))
out = os.path.join(os.path.dirname(__file__), "..", "packaging", "linux", "tidewalker-icon.png")
with open(out, "wb") as f:
    f.write(png)
print("wrote", os.path.normpath(out))
