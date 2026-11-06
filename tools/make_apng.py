#!/usr/bin/env python3
"""Assembles a directory of PNG frames (written by tidewalker --headless,
8-bit RGB, filter 0) into a single animated APNG.

Usage: python3 tools/make_apng.py <frame-dir> <output.apng> [fps]
Frames must be named frame_*.png and sorted.
"""
import struct, zlib, sys, os, glob

def read_png_rgb(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    pos = 8
    idat = b""
    w = h = None
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        typ = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", body[:10])
            assert bd == 8 and ct == 2, "expect 8-bit RGB"
        elif typ == b"IDAT":
            idat += body
        elif typ == b"IEND":
            break
        pos += 12 + ln
    raw = zlib.decompress(idat)
    stride = w * 3
    out = bytearray()
    p = 0
    for _ in range(h):
        assert raw[p] == 0, "expect filter 0"
        p += 1
        out += raw[p:p + stride]
        p += stride
    return w, h, bytes(out)

def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

def make_apng(paths, out, fps=10):
    frames = [read_png_rgb(p) for p in paths]
    w, h, _ = frames[0]
    for f in frames:
        assert f[0] == w and f[1] == h, "frame size mismatch"
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"acTL", struct.pack(">II", len(frames), 0))
    seq = 0
    for i, (_, _, rgb) in enumerate(frames):
        raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
        comp = zlib.compress(raw, 6)
        png += chunk(b"fcTL", struct.pack(">IIIIIHHBB", seq, w, h, 0, 0, 1, fps, 0, 0))
        seq += 1
        if i == 0:
            png += chunk(b"IDAT", comp)
        else:
            png += chunk(b"fdAT", struct.pack(">I", seq) + comp)
            seq += 1
    png += chunk(b"IEND", b"")
    with open(out, "wb") as f:
        f.write(png)
    return len(png)

if __name__ == "__main__":
    d, out = sys.argv[1], sys.argv[2]
    fps = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    paths = sorted(glob.glob(os.path.join(d, "frame_*.png")))
    n = make_apng(paths, out, fps)
    print(f"wrote {out}: {len(paths)} frames at {fps} fps, {n/1e6:.1f} MB")
