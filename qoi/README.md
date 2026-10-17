# qoi.datatype

QOI pictures (the "Quite OK Image" format, [qoiformat.org](https://qoiformat.org)) in every program that shows
pictures through datatypes: MultiView, a double-click on Workbench, file managers, paint programs that load
through datatypes.

- **Every Amiga with OS 3.0 or later**, any processor from the 68000 up, one file for all of them
- **Full colour with transparency** on OS 3.5, 3.9 and 3.2: semi-transparent pixels kept (picture.datatype 47.9
  and later draws them); pictures that only *say* they have transparency are shown as plain ones
- **256 colours on OS 3.0 and 3.1**, whose picture.datatype takes no full colour: a picture with 256 colours or
  fewer keeps exactly its own; a bigger one gets the best 256 (median cut) with Floyd-Steinberg dithering, and
  transparency is shown over Workbench's grey
- **Fast:** a decoder written for the 68k, with a hand-written loop for the 68020 and later chosen at run time,
  sized for the 68020's 256-byte instruction cache. On OS 3.2 the pixels go straight into picture.datatype's own
  buffer, with no copy. On the real A1200 (Emu68) photos open 2.4-3.6x faster than with the older QOI datatype
  on Aminet, and 3.5-9x faster than the same pictures as PNG with OS 3.2's own png.datatype
- **Little memory:** the file is read 16 KB at a time and never held whole; on OS 3.0/3.1 the picture is decoded
  twice instead of kept in full colour
- **Safe with broken files:** every byte of every picture is the same as the reference decoder's (`qoi.h`), also
  for damaged and cut-short files, checked on 450 test pictures on the PC and on the Amiga, and fuzzed
- Recognised by its first bytes (`qoif`), whatever the file is called

## Install

| File | Goes to |
|---|---|
| `qoi.datatype` | `SYS:Classes/DataTypes/` |
| `QOI` | `DEVS:DataTypes/` |
| `ToQOI` | `C:` (or anywhere): the converter, see below |

Then `AddDataTypes REFRESH`. **If an older QOI datatype was installed** (the one on Aminet has the same file
names), restart the Amiga instead: `AddDataTypes REFRESH` does not reload a descriptor the system already knows.

## ToQOI: pictures to QOI

`ToQOI` turns pictures into QOI files. It reads them through datatypes, so **every format the Amiga has a
picture datatype for** works: PNG, JPEG, GIF, BMP, WebP, ILBM and the rest.

```
ToQOI FROM/A/M,TO/K,FORCE/S,QUIET/S

ToQOI photo.png                 photo.qoi beside it
ToQOI Pics/#?.(png|jpg) TO Work:QOI    many at once, into a drawer
ToQOI logo.gif TO RAM:logo.qoi   one picture, any name
```

- **Full colour with transparency:** PNG transparency is kept exactly, and so is a GIF's or an ILBM's
  transparent colour. A picture that only *says* it has transparency is saved as a plain one
- **On OS 3.0 and 3.1** (no full-colour datatypes) it reads the picture's own bitmap and palette, HAM6, HAM8
  and Extra-Halfbrite included
- **From Workbench:** select pictures, hold Shift and double-click ToQOI (give it a tool icon first). Each QOI
  goes beside its picture, and a requester says how it went
- **Safe:** a QOI that is already there is left alone unless you say `FORCE`; a half-written file is deleted
  if something goes wrong; Ctrl-C stops it between pictures and in the middle of one
- **Little memory:** the picture is read a few rows at a time and the file is written 16 KB at a time, so
  neither one is ever held whole
- **Small:** about 10 KB, one build for every Amiga from the 68000 up, OS 3.0 or later
- Every byte it writes is the same as the reference encoder's (`qoi.h`), checked on 1,500 test pictures

On the real A1200 (Emu68, OS 3.2) an 800x600 PNG with transparency or a 768x512 photo turns into a QOI in
about a quarter of a second, reading the PNG included.

**Tested** on OS 3.2.1 and 3.1 (68020) and a 68000 A600 with 3.1 in FS-UAE, and on the real A1200: every pixel
of the QOI it made compared with the original. PNG, BMP, GIF, ILBM (plain, transparent colour,
Extra-Halfbrite) are exact; HAM comes out as OS 3.2's own picture.datatype shows it (ToQOI's own HAM
reading on 3.1 gives the same bytes); JPEG as the Amiga's JPEG datatype decodes it. Running it many times
loses no memory.

## How it works

A subclass of `picture.datatype`. `src/qoidec.c` decodes a band of rows at a time from a 16 KB buffer;
`src/fast020.s` is its hot loop for the 68020 and later. Then, by what picture.datatype offers:

| picture.datatype | OS | How the pixels get there |
|---|---|---|
| 47 | 3.2 | Straight into its pixel buffer (`PDTA_ObtainPixelBuffer`) |
| 43 to 46 | 3.5, 3.9 | `PDTM_WRITEPIXELARRAY`, 32 KB of rows at a time |
| 39 to 42 | 3.0, 3.1 | 256 colours (`src/quant.c`), drawn into a bitmap |

## Tested

**Every pixel** of the official QOI test pictures (qoiformat.org) read back through datatypes and compared with
the originals: the same, transparency included, on OS 3.2.1 in FS-UAE and on the real A1200. The decoder alone:
450 generated pictures (whole, damaged, cut short) against the reference decoder on the PC, and on the Amiga
through the 68020 loop and the C loop (68020 and 68000); fuzzed on the PC (about a million runs, nothing found).
Damaged files are refused or shown with garbage, never a crash. Open and close: no memory lost.

Opening a picture, the datatype's own part (OM_NEW). Times in FS-UAE are emulated, not real hardware:

| Picture | Real A1200, Emu68, OS 3.2 | FS-UAE A1200 68020, 3.2.1 | FS-UAE A1200 68020, 3.1 (256 colours) |
|---|---|---|---|
| 800x600 with transparency | 26 ms (older QOI datatype: 63, PNG: 231) | 2.6 s (older: 8.1) | 42 s |
| 768x512 photo | 38 ms (older: 109, PNG: 133) | 4.9 s (older: 14.5) | 36 s |
| 1152x858 photo | 68 ms (older: 243, PNG: 300) | 11.2 s (older: 35.7) | 79 s |
| 256x256 test card | | 0.3 s | 5 s |

On a 68000 (FS-UAE A600, 7 MHz, OS 3.1) a 256x256 picture takes 23-30 seconds. MultiView shows QOI pictures
on OS 3.2.1 (with transparency over the window), 3.1 and the A600.

The older datatype does not work on OS 3.1 at all; it needs picture.datatype 43.

## Building

`make` with bebbo's m68k-amigaos-gcc. `make check` compares the decoder with the reference one on the PC
(`tests/ref/qoi.h`, MIT, Dominic Szablewski); `make amigacheck` builds the same check for the Amiga;
`make bench` builds `qoibench`, which times the decoder alone. `make check` also holds ToQOI's encoder
(`src/qoienc.c`) to the reference encoder, byte for byte (`tests/enccheck.c`). `make` with `-DDEBUG` added to
CFLAGS makes a ToQOI that logs each step to `SYS:tq.log`.

## Versions

See [CHANGELOG.md](CHANGELOG.md).
