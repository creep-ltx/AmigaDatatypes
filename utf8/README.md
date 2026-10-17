# utf8.datatype

UTF-8 text files shown with the right letters in every program that shows text through datatypes:
MultiView, AmigaGuide, a double-click on Workbench, file managers and viewers. Without it, each letter
beyond plain ASCII shows as two or three wrong ones ("RÃ¤ksmÃ¶rgÃ¥s"), and a file that starts with
one may not open at all, taken for a binary file.

- **Recognised by what is in the file**, not its name: any file that is good UTF-8 with at least one
  character beyond ASCII (or starts with a byte order mark). Plain ASCII and Latin-1 files are left to the
  system's own text datatype, exactly as before. A long file that is plain ASCII at the start is looked
  through to the end, so a changelog that names "Zoë" on its last line still shows her right.
- **Latin-1 letters exactly:** å ä ö é ü ß ñ ç and the rest, as the Amiga's fonts have them.
- **The nearest stand-in for the rest:** letters lose only what Latin-1 lacks (Ł L, Š S, ř r), curly quotes
  become straight, – — dashes become -, … becomes ..., € EUR, ™ TM, ≤ <=, box lines - | +.
- **Other scripts spelt in Latin letters:** Russian and the other Cyrillic languages ("Привет, мир" shows as
  "Privet, mir"), Greek ("Ωμέγα" Omega), Japanese kana ("こんにちは" konnichiha) and Korean ("서울" seoul).
  Chinese characters, the kanji in Japanese and emoji show as ?: they have no spelling without a
  dictionary.
- **Windows files:** the CR of each CR LF goes, so no box at the end of every line. The byte order mark goes.
- **Everything else as for any text:** scrolling, tabs, search, marking, copying (you copy the letters
  as shown), printing, saving. It is the system's ascii.datatype doing the work, given the converted text.
- **Leaves the files other datatypes do better:** `.md`/`.markdown` (markdown.datatype), `.html`/`.htm`,
  `.guide` and any file starting `@database` (AmigaGuide), `.ans` and `.ice` and any file with ANSI codes (pcansi.datatype, amiansi.datatype).

**Needs** Kickstart/Workbench 3.0 or later, any processor from the 68000 up, and
[utf8.library](https://github.com/creep-ltx/AmigaLibs/tree/main/utf8) in `LIBS:`, which does the converting:
0.1b2 or later spells out the other scripts (0.1b1 shows them as ?). Without the library, files are
recognised but shown as they are. The class is under 3 KB.

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

The descriptors carry a small piece of recognition code (`src/hook.c`, under 700 bytes) that reads up to
the first 16 KB of a file. It says yes for valid UTF-8 (RFC 3629: no stray bytes, no overlong forms, no
surrogates, no 0 bytes) with something beyond ASCII, and no at the first byte that is not good UTF-8, so
Latin-1 and binary files are turned away at once. A file of plain ASCII that ends within the 16 KB is left to
ascii.datatype; a longer one that is plain ASCII so far is taken, since its end may hold UTF-8.

The class is a subclass of `ascii.datatype`. When text.datatype has loaded the file, the class looks through
it four bytes at a time; if nothing is beyond ASCII, the file is shown as it is. Otherwise it is turned into
Latin-1 where it lies, line by line: plain ASCII lines are copied as they are, lines with more go through
utf8.library's `Utf8ToCharset` and are written back over their own UTF-8, which nearly always ends further
on. So opening a file needs the file and one line of memory, not two copies of the file. Korean and a few
Russian letters are spelt longer than their UTF-8: if the text would run past what is still to be read, the
rest goes into a new buffer, which replaces the old one (the class frees it: text.datatype does not). A
Latin-1 file taken this way comes out unchanged, since utf8.library reads a byte that is not UTF-8 as its
Latin-1 letter. If memory runs out part way, what is converted is shown and the rest as it was.

## Tested

In FS-UAE: Workbench 3.1 on a 68020, Workbench 3.2.1 on a 68020, and an A600 (68000, 2 MB chip, Kickstart
and Workbench 3.1). Recognition of UTF-8, UTF-8 with CR LF, a byte order mark, UTF-8 that looks binary,
ASCII, Latin-1, UTF-8 Markdown and AmigaGuide (`tests/`); the shown text copied out of MultiView matches
utf8.library's conversion of the whole file byte for byte (100 KB, with and without CR LF, files whose Latin-1
is longer, `tests/grows.txt` and `growstart.txt`, a 34 KB file with UTF-8 only on its last line,
`tests/tail.txt`, and 65 KB of Latin-1 unchanged); opened and closed with `tests/leak.sh`: nothing lost. The
conversion was also run on the PC against 75 files, with memory running out at each allocation in turn.

Speed on the 68000 (A600 in FS-UAE, 7 MHz, 2 MB chip RAM, so an emulator's figures), from starting MultiView
to its window, against the same text through ascii.datatype, with utf8.library 0.1b2: a 100 KB file of plain
English 16.6 s (16.0 s: the check costs about 0.6 s in all); English and Western European letters 18.9 s
(15.5 s); every line full of accents, Greek, Cyrillic, Japanese, Korean and emoji 23 s (14.5 s; 40 s with
utf8.library 0.1b1).

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
