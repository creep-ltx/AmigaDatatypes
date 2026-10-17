#!/usr/bin/env python3
"""varblock: a fixed-block FLAC file as a variable-block one: each frame's
header says its first sample's number instead of the frame's, with the
blocking bit set and both CRCs made again. The frames ("off len start
block" lines, from check -f) come on stdin.
usage: check -f IN | varblock.py IN OUT"""
import sys

def crc8(b):
    c = 0
    for x in b:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 7) & 0xff if c & 0x80 else (c << 1) & 0xff
    return c

def crc16(b):
    c = 0
    for x in b:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x8005) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c

def utf8(n):
    if n < 0x80:
        return bytes([n])
    for k, bits in ((1, 11), (2, 16), (3, 21), (4, 26), (5, 31), (6, 36)):
        if n < 1 << bits:
            break
    out = []
    for _ in range(k):
        out.insert(0, 0x80 | (n & 0x3f))
        n >>= 6
    return bytes([(0xff00 >> (k + 1)) & 0xff | n]) + bytes(out)

def numlen(x):
    if x < 0x80:
        return 1
    k = 1
    while x & (0x40 >> k):
        k += 1
    return k + 1

src = open(sys.argv[1], 'rb').read()
frames = [tuple(map(int, l.split())) for l in sys.stdin if l.strip()]
if not frames:
    sys.exit(1)
out = bytearray(src[:frames[0][0]])
for off, ln, start, block in frames:
    f = src[off:off + ln]
    nl = numlen(f[4])
    bc, rc = f[2] >> 4, f[2] & 15
    extra = (1 if bc == 6 else 2 if bc == 7 else 0) + (1 if rc == 12 else 2 if rc in (13, 14) else 0)
    hend = 4 + nl + extra
    h = bytes([0xff, 0xf9]) + f[2:4] + utf8(start) + f[4 + nl:hend]
    h += bytes([crc8(h)])
    body = h + f[hend + 1:ln - 2]
    c = crc16(body)
    out += body + bytes([c >> 8, c & 0xff])
open(sys.argv[2], 'wb').write(out)
