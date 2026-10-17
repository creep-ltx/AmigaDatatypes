#!/usr/bin/env python3
"""mkfont - bitmap fonts of all 256 characters, 8 pixels wide, as Amiga fonts,
for pcansi.datatype (the IBM VGA font) and amiansi.datatype (the Amiga's
art fonts).

    python3 tools/mkfont.py OUTDIR NAME:SIZE:RAWFILE:STEP ...

writes OUTDIR/NAME.font and OUTDIR/NAME/SIZE for each size: copy both into
FONTS:. RAWFILE holds 256 glyphs, one byte a row, bit 7 the left pixel,
SIZE * STEP rows each: STEP 2 takes every other row (TrueSchool's raw Amiga
fonts are their 8-row originals with every row doubled). The baseline is
the bottom row of the letter A.

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
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    outdir = sys.argv[1]
    fonts = {}
    for spec in sys.argv[2:]:
        name, size, raw, step = spec.split(':')
        size, step = int(size), int(step)
        data = open(raw, 'rb').read()
        rows = size * step
        if len(data) != 256 * rows:
            raise SystemExit(f'mkfont: {raw} is not 256 x {rows} rows')
        glyphs = bytes(data[c * rows + y * step] for c in range(256) for y in range(size))
        a = glyphs[0x41 * size:0x42 * size]
        baseline = max(y for y in range(size) if a[y])
        os.makedirs(os.path.join(outdir, name), exist_ok=True)
        with open(os.path.join(outdir, name, str(size)), 'wb') as f:
            f.write(size_file(name, size, baseline, glyphs))
        fonts.setdefault(name, []).append(size)
    for name, sizes in fonts.items():
        with open(os.path.join(outdir, name + '.font'), 'wb') as f:
            f.write(contents_file(name, sorted(sizes)))


if __name__ == '__main__':
    main()
