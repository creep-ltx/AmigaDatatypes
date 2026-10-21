# AmigaDatatypes

Datatypes for classic AmigaOS 3.x: the classes in `SYS:Classes/DataTypes` and their descriptors in
`DEVS:DataTypes` that teach every datatypes program (MultiView, AmigaGuide, browsers, viewers) to open
a new kind of file. One folder per datatype, each with its own README, version and change log.

| Datatype | What | Version |
|---|---|---|
| [markdown/](markdown/) | **Markdown** (`README.md`) formatted in MultiView and every datatypes program: headings, lists, quotes, emphasis, code with colours, tables | 0.1b2 |
| [utf8/](utf8/) | **UTF-8 text** with the right letters (Latin-1 and the nearest stand-in for the rest) instead of two or three wrong ones per letter; Russian, Greek, Japanese kana and Korean spelt in Latin letters; found by what is in the file, not its name. Needs utf8.library | 0.1b5 |
| [pcansi/](pcansi/) | **PC ANSI art** (`.ans`, BBS screens, PC `.nfo`) with its block characters, the IBM VGA font and the sixteen PC colours, as text, not a picture; SAUCE titles and widths; found by SAUCE record or by its bytes | 0.1b2 |
| [amiansi/](amiansi/) | **Amiga ANSI art** (BBS screens, `.ans`, Amiga `.asc`/`.nfo`) in the Amiga's art fonts (Topaz, MicroKnight, P0T-NOoDLE, mO'sOul, from SAUCE), its colours, italic and underline, on black, with every cursor move; found by SAUCE record or by its bytes | 0.1b1 |
| [qoi/](qoi/) | **QOI pictures** in full colour with transparency (OS 3.5+, straight into the picture's buffer on 3.2) or 256 dithered colours (OS 3.0/3.1); a 68020 loop in assembly; 2.4-3.6x faster than the older QOI datatype. With **ToQOI**, a command that turns any picture datatypes can read into QOI | 0.1b1 |
| [qoa/](qoa/) | **QOA sounds** in stereo (OS 3.5+) or mixed to mono (OS 3.0/3.1), 44.1/48 kHz averaged down to what Paula plays; a 68000 loop in assembly; a minute of 44.1 kHz stereo opens in 0.8 s on the real A1200 (Emu68) | 0.1b1 |
| [flac/](flac/) | **FLAC sounds**, every kind (4-24 bits, 1-8 channels, any rate, every encoder setting), in stereo (OS 3.5+) or mixed to mono (OS 3.0/3.1), averaged down to what Paula plays; title and artist from the tags; damaged frames skipped; the bit reader in 68000 assembly, only 16x16 multiplies | 0.1b2 |

All for a plain 68000 and Kickstart 3.0 or later.

Only datatypes live here. Libraries are in AmigaLibs; handlers, devices and programs each have their
own repository.

## Building

Each datatype builds on its own with bebbo's m68k-amigaos-gcc: `make` inside its folder.
Its README says which files come out and where to install them.

## License

MIT, see [LICENSE](LICENSE).
