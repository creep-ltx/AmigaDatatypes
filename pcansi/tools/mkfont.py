#!/usr/bin/env python3
"""mkfont - the IBM VGA font (CP437, all 256 characters) as an Amiga bitmap
font, for pcansi.datatype: PC ANSI art is drawn with it.

    python3 tools/mkfont.py OUTDIR

writes OUTDIR/ibmvga.font and OUTDIR/ibmvga/8 and /16: copy both into FONTS:.
The glyphs are the VGA ROM's, from VileR's Ultimate Oldschool PC Font Pack
(int10h.org, CC BY-SA 4.0; see fonts/README.md): fonts/IBM_VGA_8x16.raw
(256 x 16 rows) and fonts/IBM_VGA_8x8.raw (256 x 8 rows, the VGA's 8x8 ROM
font: 80x50 mode), one byte a row, bit 7 the left pixel.

The file layout is CTerm's tools/ttf2font.py (MicroKnight/8's): a hunk file,
"moveq #-1,d0; rts", a DiskFontHeader, its TextFont, the bits, the CharLoc
table, four relocations."""
import os
import struct
import sys

LO, HI = 0, 255
NT_FONT = 12
FPF_DISKFONT, FPF_DESIGNED = 0x02, 0x40
DFH_ID, FCH_ID = 0x0F80, 0x0F00
HUNK_HEADER, HUNK_CODE, HUNK_RELOC32, HUNK_END = 0x3F3, 0x3E9, 0x3EC, 0x3F2
NAME = 'ibmvga'
HERE = os.path.dirname(os.path.abspath(__file__))
SOURCES = {8: ('IBM_VGA_8x8.raw', 6), 16: ('IBM_VGA_8x16.raw', 11)}   # file, baseline


def size_file(name, size, baseline, raw):
    width = 8
    n = HI - LO + 2                             # the characters and the default (empty)
    modulo = (n * width + 15) // 16 * 2         # bytes a row, even
    bits = bytearray(modulo * size)
    for i in range(HI - LO + 1):
        for y in range(size):
            bits[y * modulo + i] = raw[(LO + i) * size + y]
    cloc = b''.join(struct.pack('>HH', i * width, width) for i in range(n))

    dfh = 4                                     # after moveq/rts
    nameoff = dfh + 22
    tf = dfh + 54
    cdata = tf + 52
    clocoff = cdata + len(bits)
    end = clocoff + len(cloc)
    data = bytearray(end + (-end) % 4)
    data[0:4] = b'\x70\xff\x4e\x75'
    struct.pack_into('>IIBBI', data, dfh, 0, 0, NT_FONT, 0, nameoff)
    struct.pack_into('>HHI', data, dfh + 14, DFH_ID, 1, 0)
    data[nameoff:nameoff + 31] = name.encode('latin-1')[:31].ljust(31, b'\0')
    struct.pack_into('>IIBBIIH', data, tf, 0, 0, NT_FONT, 0, nameoff, 0, end - cdata)
    struct.pack_into('>HBBHHHHBBIHIII', data, tf + 20, size, 0, FPF_DESIGNED,
                     width, baseline, 1, 0, LO, HI, cdata, modulo, clocoff, 0, 0)
    data[cdata:clocoff] = bits
    data[clocoff:end] = cloc
    relocs = [dfh + 10, tf + 10, tf + 34, tf + 40]
    longs = len(data) // 4
    h = struct.pack('>IIIII', HUNK_HEADER, 0, 1, 0, 0) + struct.pack('>I', longs)
    h += struct.pack('>II', HUNK_CODE, longs) + bytes(data)
    h += struct.pack('>III', HUNK_RELOC32, len(relocs), 0)
    h += b''.join(struct.pack('>I', r) for r in relocs) + struct.pack('>I', 0)
    return h + struct.pack('>I', HUNK_END)


def contents_file(name, sizes):
    out = struct.pack('>HH', FCH_ID, len(sizes))
    for s in sizes:
        fn = f'{name}/{s}'.encode('latin-1')
        out += fn.ljust(256, b'\0') + struct.pack('>HBB', s, 0, FPF_DESIGNED | FPF_DISKFONT)
    return out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    outdir = sys.argv[1]
    os.makedirs(os.path.join(outdir, NAME), exist_ok=True)
    for size, (src, baseline) in SOURCES.items():
        raw = open(os.path.join(HERE, '..', 'fonts', src), 'rb').read()
        if len(raw) != 256 * size:
            raise SystemExit(f'mkfont: {src} is not 256 x {size} rows')
        with open(os.path.join(outdir, NAME, str(size)), 'wb') as f:
            f.write(size_file(NAME, size, baseline, raw))
    with open(os.path.join(outdir, NAME + '.font'), 'wb') as f:
        f.write(contents_file(NAME, sorted(SOURCES)))


if __name__ == '__main__':
    main()
