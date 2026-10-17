# utf8.datatype

UTF-8 text files shown with the right letters in every program that shows text through datatypes:
MultiView, AmigaGuide, a double-click on Workbench, file managers and viewers. Without it, each letter
beyond plain ASCII shows as two or three wrong ones ("RÃ¤ksmÃ¶rgÃ¥s"), and a file that starts with
one may not open at all, taken for a binary file.

- **Recognised by what is in the file**, not its name: any file that is good UTF-8 with at least one
  character beyond ASCII (or starts with a byte order mark). Plain ASCII and Latin-1 files are left to the
  system's own text datatype, exactly as before.
- **Latin-1 letters exactly:** å ä ö é ü ß ñ ç and the rest, as the Amiga's fonts have them.
- **The nearest stand-in for the rest:** letters lose only what Latin-1 lacks (Ł L, Š S, ř r), curly quotes
  become straight, – — dashes become -, … becomes ..., € EUR, ™ TM, ≤ <=, box lines - | +. What has nothing
  near (Greek, Cyrillic, Chinese, Japanese, emoji) shows as ?.
- **Windows files:** the CR of each CR LF goes, so no box at the end of every line. The byte order mark goes.
- **Everything else as for any text:** scrolling, tabs, search, marking, copying (you copy the letters
  as shown), printing, saving. It is the system's ascii.datatype doing the work, given the converted text.
- **Leaves the files other datatypes do better:** `.md`/`.markdown` (markdown.datatype), `.html`/`.htm`,
  `.guide` and any file starting `@database` (AmigaGuide).

**Needs** Kickstart/Workbench 3.0 or later, any processor from the 68000 up, and
[utf8.library](https://github.com/creep-ltx/AmigaLibs/tree/main/utf8) in `LIBS:`, which does the converting.
Without the library, files are recognised but shown as they are. The class is 2 KB.

## Install

| File | Goes to |
|---|---|
| `utf8.datatype` | `SYS:Classes/DataTypes/` |
| `UTF-8` | `DEVS:DataTypes/` |
| `UTF-8bin` | `DEVS:DataTypes/` |
| `utf8.library` (AmigaLibs) | `LIBS:` |

Then `AddDataTypes REFRESH` (or restart).

`UTF-8bin` is for UTF-8 files the system takes for binary: datatypes.library looks at a file's first bytes
to decide whether it is text or binary, and text full of accented letters looks binary to it. The two
descriptors are the same otherwise.

## How it works

The descriptors carry a small piece of recognition code (`src/hook.c`, under 600 bytes) that reads up to
the first 16 KB of a file and says yes only for valid UTF-8 (RFC 3629: no stray bytes, no overlong forms, no
surrogates, no 0 bytes) with something beyond ASCII. The first byte that is not good UTF-8 ends the check, so
Latin-1 and binary files are turned away at once.

The class is a subclass of `ascii.datatype`. When text.datatype has loaded the file, the class turns it into
Latin-1 in the same buffer: plain ASCII lines are copied as they are, lines with more go through
utf8.library's `Utf8ToCharset`. The Latin-1 text is never longer than the UTF-8 (checked for every Unicode
character), so it always fits.

## Tested

In FS-UAE: Workbench 3.1 on a 68020, Workbench 3.2.1 on a 68020, and an A600 (68000, 2 MB chip, Kickstart
and Workbench 3.1). Recognition of UTF-8, UTF-8 with CR LF, a byte order mark, UTF-8 that looks binary,
ASCII, Latin-1, UTF-8 Markdown and AmigaGuide (`tests/`); the shown text copied out of MultiView matches
utf8.library's conversion of the whole file byte for byte (100 KB, with and without CR LF); opened and
closed with `tests/leak.sh`: nothing lost.

Speed on the 68000 (A600 in FS-UAE, 7 MHz, so an emulator's figures), from starting MultiView to its
window, against the same text in Latin-1 through ascii.datatype: a 100 KB file of English with a byte order
mark 18.5 s (16.5 s); English and Western European letters mixed 19.3 s (15.5 s); every line full of
accents, Greek, Cyrillic, Japanese and emoji 40 s (14.5 s). Recognising a file takes about 0.3 s there.

## Building

`make` with bebbo's m68k-amigaos-gcc. This repository and AmigaLibs must sit side by side (only
utf8.library's headers are used):

    git clone https://github.com/creep-ltx/AmigaDatatypes
    git clone https://github.com/creep-ltx/AmigaLibs
    cd AmigaDatatypes/utf8 && make

(Elsewhere: `make UTF8LIB=.../AmigaLibs/utf8`.) `tests/install.sh` puts everything on an Amiga through
Wasabi.

## Versions

See [CHANGELOG.md](CHANGELOG.md).
