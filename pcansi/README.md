# pcansi.datatype

PC ANSI art shown as the artists drew it, in every program that shows text through datatypes: MultiView,
a double-click on Workbench, file managers and viewers. The block and line characters of the PC (░ ▒ ▓ █ ▀
▄ ═ ║ ╔), the IBM VGA font and the sixteen PC colours, straight from `.ans` files, BBS screens and
`.nfo` files. Without it, a PC ANSI is a wall of escape codes and accented letters.

- **The art stays text.** It is not turned into a picture: the IBM VGA font draws it, so it is sharp, needs
  about two bytes a character, and a 2,000-row piece opens on a 2 MB Amiga.
- **The right proportions:** on a screen whose pixels are twice as tall as wide (640x256, 640x200) the font
  is 8 rows high, on an interlaced or graphics card screen 16 rows: either way the art looks as it did on a
  PC's 640x400.
- **Read as a PC showed it:** colours, bold and bright colours, iCE colours (bright backgrounds), 256 and
  24-bit colours as the nearest of the sixteen, cursor movement, saved positions, clearing, inserted and
  deleted lines and characters, wrapping at the 80th column the way ANSI.SYS did, all 256 CP437 characters.
  The reader is the one in Charcoal, checked on 1,712 PC art files from 16colo.rs: the same page, cell for
  cell.
- **SAUCE:** the record at the end of an art file gives the width (art wider than 80 columns), iCE colours
  and the window's title ("Spawn Scroller by FreshRich/Legacy Krew"); its comment block is left out.
- **Colours on any screen:** the colours a piece uses most get exact colours first; on a screen with too
  few, the rest get the nearest the screen has by brightness, and a character that would vanish into its
  background is drawn white or black instead. Best on a Workbench with 16 or more colours.
- **Black around the art**, as on a PC, when the window is larger than the art.
- **Found by what is in the file:** any file with a SAUCE record that says character art and does not name
  an Amiga font; without a record, `.ans` and `.ice` files unless they look like Amiga art, and `.nfo`,
  `.diz`, `.asc` (and other names with ANSI codes) when their characters are the PC's. The Amiga's own BBS
  screens and ASCII art are left to the system's text datatypes: on 412 Amiga BBS files and 600 Amiga ASCII
  pieces it took none and one.
- **Small:** the class is under 14 KB, the recognition code under 2 KB, the font 9 KB.

**Needs** Kickstart/Workbench 3.0 or later and any processor from the 68000 up.

**Not yet:** ANSI animations show as they end, not at their fullest; Amiga ANSI (Topaz art) is left to
the text datatypes; copying text out of MultiView gives the PC's character codes, so block characters come
out as accented letters.

## Install

| File | Goes to |
|---|---|
| `pcansi.datatype` | `SYS:Classes/DataTypes/` |
| `PC-ANSI` | `DEVS:DataTypes/` |
| `PC-ANSIbin` | `DEVS:DataTypes/` |
| `fonts/ibmvga.font` and the drawer `fonts/ibmvga` | `FONTS:` |

Then restart (or `AddDataTypes REFRESH` the first time). `PC-ANSIbin` is for the files the system takes for
binary, which is most PC art: datatypes.library sorts a file into text or binary by its first bytes, and
escape codes and block characters look binary to it. The two descriptors are the same otherwise.

## How it works

The descriptors carry a small piece of recognition code (`src/hook.c` and `src/guess.c`) that reads the
last 128 bytes of a file (a SAUCE record) and up to its first 8 KB. Without a record it weighs the bytes
above 127: each has a weight learned (for Charcoal) from 1,374 files whose machine was known, so █ ▀ ▄ ▒
count for the PC and Ø · ¯ « » for the Amiga; Amiga line ends (LF only) and the Amiga's 8-bit CSI count
too. A file with a NUL in its first bytes is turned away at once.

The class is a subclass of `text.datatype`. When the file is loaded it is read as an ANSI terminal would
(`src/ansi.c`) into a page of cells, a character and a colour byte each. On Workbench 3.2 each row's runs
of one colour become text.datatype's pieces, so text.datatype draws, scrolls, marks and copies. Workbench
3.1's text.datatype keeps its own font whatever it is told, so there the class draws the rows itself with
`Text()`, a run of one colour at a time; it does the same anywhere the pieces would leave less than 192 KB
free (some 40 bytes a piece, and a colourful row has 15 or more).

## Tested

In FS-UAE: Workbench 3.1 and 3.2.1 on a 68020, and an A600 (68000, 2 MB chip, Kickstart and Workbench 3.1);
4-colour, 16-colour and interlaced Workbench screens. The reader against Charcoal's on 1,712 PC art files
(the same page in all but one, where this one is right: Charcoal showed a UTF-8 comment block as art); under
AddressSanitizer on 3,329 art files of every kind and 2,000 random ones. Recognition on the Amiga and on
the PC over the same files (`tests/claimcheck.c`); opened and closed with `tests/leak.sh`: nothing lost.

Speed in FS-UAE (an emulator's figures, not a real Amiga's), from starting MultiView to its window: a 47 KB
piece in 7 s on the A600's 7 MHz 68000; a 257 KB, 2,312-row piece in 12 s on a 14 MHz 68020.

## Building

`make` (bebbo's m68k-amigaos-gcc) gives `build/pcansi.datatype`, `build/PC-ANSI`, `build/PC-ANSIbin` and
`build/fonts/`. One build for every Amiga: plain 68000 code. `make DEBUG=1` traces through RawPutChar
(`wasabi debug`). On the PC: `make pagedump` (the reader, printing the page) and `tests/claimcheck.c`
(the recognition). `tests/mksample.py` writes the test pages, `tests/install.sh` puts everything on an Amiga
through Wasabi.

## License

MIT, see [LICENSE](../LICENSE). The font is VileR's IBM VGA from the Ultimate Oldschool PC Font Pack,
CC BY-SA 4.0: see [fonts/README.md](fonts/README.md).
