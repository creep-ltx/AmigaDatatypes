#!/usr/bin/env python3
"""Write the Amiga test art: sample.ans (a SAUCE record naming Amiga Topaz 1+),
mk.ans (the same naming MicroKnight), screen.txt (no record, LF line ends:
an Amiga BBS screen). Form feed, the Amiga's 8-bit CSI (0x9B) next to ESC [,
every colour, bold, italic, underline, Latin-1 letters, cursor moves."""
import struct

def sgr(*p, csi=b'\x1b['):
    return csi + b';'.join(str(x).encode() for x in p) + b'm'

def art():
    o = bytearray(b'\x0c')                          # form feed: clear, home
    o += sgr(0, 1, 33) + '  amiansi.datatype test page - Amiga ANSI'.encode('latin-1') + sgr(0) + b'\n'
    o += sgr(0, 36, csi=b'\x9b') + b'  ' + ('¯' * 44).encode('latin-1') + sgr(0) + b'\n\n'
    for bg in range(8):
        o += b'  '
        for fg in range(8):
            o += sgr(0, 30 + fg, 40 + bg) + b' %d ' % fg
        o += sgr(0) + b' '
        for fg in range(8):
            o += sgr(0, 1, 30 + fg, 40 + bg) + b' %d ' % (fg + 8)
        o += sgr(0) + b'\n'
    o += b'\n  ' + sgr(0, 3) + b'italic' + sgr(0) + b'  ' + sgr(0, 4) + b'underlined' + sgr(0) + b'  ' \
        + sgr(0, 1, 3, 4, 35) + b'all three' + sgr(0) + b'  ' + sgr(0, 7) + b'reversed' + sgr(0) + b'\n\n'
    o += b'  ' + sgr(0, 32) + 'Räksmörgås, ÆØÅ æøå, ¡¿ ½ ¼ © ® ° ± µ ¶ · » «'.encode('latin-1') + sgr(0) + b'\n'
    o += b'  ' + sgr(0, 34) + '.·´¯`·.¸¸.·´¯`·.¸¸  _____ /\\___ ____/\\'.encode('latin-1') + sgr(0) + b'\n\n'
    o += sgr(0, 1, 32) + b'  left' + b'\x9bs' + b'\x9b20C' + b'right' + b'\x9bu' + b'!' + sgr(0) + b'\n'
    o += sgr(0, 35) + b'  ' + b'-' * 30 + b'\x9b1A' + b'\x9b4C' + sgr(1, 31) + b'up' + sgr(0) + b'\n'
    return bytes(o)

def sauce(n, font):
    r = bytearray(128)
    r[0:7] = b'SAUCE00'
    r[7:42] = b'amiansi.datatype test'.ljust(35)
    r[42:62] = b'AmigaDatatypes'.ljust(20)
    r[62:82] = b''.ljust(20)
    r[82:90] = b'20261017'
    struct.pack_into('<I', r, 90, n)
    r[94], r[95] = 1, 1
    struct.pack_into('<HH', r, 96, 80, 25)
    r[105] = 0
    r[106:128] = font.ljust(22, b'\0')
    return bytes(r)

a = art()
open('sample.ans', 'wb').write(a + b'\x1a' + sauce(len(a), b'Amiga Topaz 1+'))
open('mk.ans', 'wb').write(a + b'\x1a' + sauce(len(a), b'Amiga MicroKnight'))
open('screen.txt', 'wb').write(a)
