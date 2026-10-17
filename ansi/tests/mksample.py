#!/usr/bin/env python3
"""Write the test art: sample.ans (with a SAUCE record, iCE colours) and
nosauce.ans (the same without one, blink not iCE). Every colour on every
background, the shades and blocks, the box characters, all 256 CP437
characters, wrap at column 80 and cursor movement."""
import struct

ESC = b'\x1b['

def sgr(*p):
    return ESC + b';'.join(str(x).encode() for x in p) + b'm'

def art():
    o = bytearray()
    o += ESC + b'2J' + ESC + b'H'
    # a double box with the title
    o += sgr(0, 1, 36) + b'\xc9' + b'\xcd' * 78 + b'\xbb'      # a full row: no line end (wraps)
    t = b' ansi.datatype test page - PC ANSI on the Amiga '
    o += b'\xba' + sgr(0, 1, 33) + t.center(78) + sgr(0, 1, 36) + b'\xba'
    o += b'\xc8' + b'\xcd' * 78 + b'\xbc'
    o += sgr(0) + b'\r\n'
    # 16 foregrounds on 8 backgrounds
    for bg in range(8):
        o += sgr(0) + b'  '
        for fg in range(16):
            o += sgr(0, *( (1,) if fg > 7 else ()), 30 + fg % 8, 40 + bg) + (b'%02d' % fg) + b'\xfe'
        o += sgr(0) + b'\r\n'
    # bright backgrounds: blink (iCE colours with the record)
    o += sgr(0) + b'  '
    for bg in range(8):
        o += sgr(0, 5, 30, 40 + bg) + b' iCE%d ' % (bg + 8)
    o += sgr(0) + b'\r\n\r\n'
    # shades and blocks in colour
    o += b'  '
    for c in range(1, 8):
        o += sgr(0, 30 + c, 40 + (c + 3) % 8) + b'\xb0\xb1\xb2\xdb\xdc\xdf\xdd\xde '
    o += sgr(0) + b'\r\n\r\n'
    # every character, 32 a row (0x1B shown as a space: it starts a code)
    for row in range(8):
        o += b'  ' + sgr(0, 1, 37) + b'%02X ' % (row * 32) + sgr(0, 37)
        for k in range(32):
            c = row * 32 + k
            o += bytes([c]) if c not in (0x1B, 0x0A, 0x0D, 0x1A) else b' '
        o += sgr(0) + b'\r\n'
    o += b'\r\n'
    # cursor moves: a saved spot, forward, up
    o += sgr(0, 1, 32) + b'  left' + ESC + b's' + ESC + b'20C' + b'right' + ESC + b'u' + b'!' + sgr(0) + b'\r\n'
    o += sgr(0, 35) + b'  ' + b'\xc4' * 30 + ESC + b'1A' + ESC + b'5D' + sgr(1, 31) + b'up' + sgr(0) + b'\r\n\r\n'
    return bytes(o)

def sauce(n, flags):
    r = bytearray(128)
    r[0:7] = b'SAUCE00'
    r[7:42] = b'ansi.datatype test'.ljust(35)
    r[42:62] = b'AmigaDatatypes'.ljust(20)
    r[62:82] = b''.ljust(20)
    r[82:90] = b'20261017'
    struct.pack_into('<I', r, 90, n)
    r[94], r[95] = 1, 1
    struct.pack_into('<HH', r, 96, 80, 25)
    r[105] = flags
    r[106:128] = b'IBM VGA'.ljust(22, b'\0')
    return bytes(r)

a = art()
open('sample.ans', 'wb').write(a + b'\x1a' + sauce(len(a), 0x01 | 0x10))
open('nosauce.ans', 'wb').write(a.replace(b'\x00', b' ').replace(b'\x07', b' ').replace(b'\x08', b' ').replace(b'\x09', b' '))
