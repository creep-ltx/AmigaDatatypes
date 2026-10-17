# amiansi.datatype

Amiga ANSI art shown as the artists drew it, in every program that shows text through datatypes: MultiView,
a double-click on Workbench, file managers and viewers. BBS screens, `.ans` files drawn on an Amiga, Amiga
`.asc` and `.nfo` art: in Topaz or the font the art was made in, the Amiga ANSI colours, italic and underline,
on black, with every cursor move in its place. Without it, the system's text datatype shows such a file in
the Workbench's font and pens on grey, and anything drawn with cursor moves lands in the wrong place.

- **The scene's fonts:** Topaz 1, Topaz 1+, Topaz 2, Topaz 2+, MicroKnight, MicroKnight+, P0T-NOoDLE and
  mO'sOul (TrueSchool's pixel-exact versions). The art's SAUCE record says which; without one it is Topaz 1+.
  Only the font a file needs is loaded.
- **The right proportions:** on a 640x256 or 640x200 screen the fonts have their own 8 rows; on an
  interlaced or graphics card screen every row is doubled (16), so the art keeps its shape.
- **Read as an Amiga terminal shows it:** Latin-1 letters (å ä ö, ¯ · ´ ¸ for the art), the Amiga's own
  8-bit CSI (0x9B) as well as ESC [, form feed as clear screen, colours, bold as the bright colours, italic,
  underline, reverse, cursor movement, saved positions, clearing, inserted and deleted lines. The reader is
  the one in Charcoal, checked on 1,168 Amiga art files: the same page, cell for cell (two giant collections
  are cut at 12,000 rows).
- **Animations at their fullest:** one that ends on a clear screen shows its fullest picture.
- **SAUCE:** the record gives the font, the width and the window's title.
- **Colours on any screen:** the colours a piece uses most get exact colours first; on a screen with too
  few, the rest get the nearest the screen has by brightness, and a character that would vanish into its
  background is drawn white or black instead.
- **Black around the art** when the window is larger than the art.
- **Found by what is in the file:** a SAUCE record that names an Amiga font; without one, files with ANSI
  codes whose bytes lean Amiga (LF line ends, Latin-1 art characters, the 8-bit CSI), whatever their name;
  `.ans` and `.asc` files and Amiga `.nfo`/`.diz` art even without codes. PC art goes to its sister,
  [pcansi.datatype](../pcansi/), and no file is taken by both: all 412 files of an Amiga BBS's styles went
  here, none to pcansi. AmigaGuide files and plain text without codes are left alone.
- **Small:** the class is 16 KB, the recognition code under 3 KB, each font 9 KB.

**Needs** Kickstart/Workbench 3.0 or later and any processor from the 68000 up.

**Not yet:** SyncTERM's font switching inside a file (CSI ... SP D) is not followed.

## Install

| File | Goes to |
|---|---|
| `amiansi.datatype` | `SYS:Classes/DataTypes/` |
| `Amiga-ANSI` | `DEVS:DataTypes/` |
| `Amiga-ANSIbin` | `DEVS:DataTypes/` |
| `fonts/ansi_*.font` and their drawers | `FONTS:` |

Then restart (or `AddDataTypes REFRESH` the first time). `Amiga-ANSIbin` is for the files the system takes
for binary; the two descriptors are the same otherwise. With utf8.datatype 0.1b5 or later, long BBS screens
with codes are never taken for UTF-8 text.

## How it works

amiansi.datatype is pcansi.datatype's source (`../pcansi/src`) built with `-DAMIANSI`: the same reader
(`ansi.c`, in its Amiga mode), class and recognition code (`guess.c`'s `ansi_claim_amiga`), with the Amiga
fonts, the Amiga ANSI colours (yellow, not the PC's brown) and the Amiga's idea of a blank cell. On Workbench
3.2 text.datatype draws the pieces in Topaz 1+ (marking and copying work); with another SAUCE font, with
underlined text (3.2's text.datatype loses the underline of a piece far from the left), on Workbench 3.1, or
when memory is short, the class draws the rows itself.

## Tested

In FS-UAE: Workbench 3.2.1 and 3.1 on a 68020 (16-colour screens, one interlaced), and an A600 (68000, 2 MB
chip, Workbench 3.1, 4 colours). The test pages (`tests/mksample.py`: Topaz and MicroKnight by SAUCE, a
screen without a record), real BBS screens and an Amiga NFO; recognition next to pcansi.datatype and
utf8.datatype on the Amiga; opened and closed with `tests/leak.sh`: nothing lost. The reader against
Charcoal's and under AddressSanitizer on the PC (`../pcansi/tests/pagedump.c -a`).

Speed in FS-UAE (an emulator's figures): an 8 KB Amiga NFO in 6 s on the A600's 7 MHz 68000, MultiView's own
start included.

## Building

`make` (bebbo's m68k-amigaos-gcc) gives `build/amiansi.datatype`, `build/Amiga-ANSI`, `build/Amiga-ANSIbin`
and `build/fonts/`. It needs `../pcansi` beside it. `tests/install.sh` puts everything on an Amiga through
Wasabi.

## License

MIT, see [LICENSE](../LICENSE). The fonts are TrueSchool's, GPL with the font exception: see
[fonts/README.md](fonts/README.md).
