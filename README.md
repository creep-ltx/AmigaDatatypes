# AmigaDatatypes

Datatypes for classic AmigaOS 3.x: the classes in `SYS:Classes/DataTypes` and their descriptors in
`DEVS:DataTypes` that teach every datatypes program (MultiView, AmigaGuide, browsers, viewers) to open
a new kind of file. One folder per datatype, each with its own README, version and change log.

| Datatype | What | Version |
|---|---|---|
| [markdown/](markdown/) | **Markdown** (`README.md`) formatted in MultiView and every datatypes program: headings, lists, quotes, emphasis, code with colours, tables | 0.1b2 |
| [utf8/](utf8/) | **UTF-8 text** with the right letters (Latin-1 and the nearest stand-in for the rest) instead of two or three wrong ones per letter; Russian, Greek, Japanese kana and Korean spelt in Latin letters; found by what is in the file, not its name. Needs utf8.library | 0.1b4 |
| [pcansi/](pcansi/) | **PC ANSI art** (`.ans`, BBS screens, PC `.nfo`) with its block characters, the IBM VGA font and the sixteen PC colours, as text, not a picture; SAUCE titles and widths; found by SAUCE record or by its bytes | 0.1b1 |
| [qoi/](qoi/) | **QOI pictures** in full colour with transparency (OS 3.5+, straight into the picture's buffer on 3.2) or 256 dithered colours (OS 3.0/3.1); a 68020 loop in assembly; 2.4-3.6x faster than the older QOI datatype. With **ToQOI**, a command that turns any picture datatypes can read into QOI | 0.1b1 |

All for a plain 68000 and Kickstart 3.0 or later.

Only datatypes live here. Libraries are in AmigaLibs; handlers, devices and programs each have their
own repository.

## Building

Each datatype builds on its own with bebbo's m68k-amigaos-gcc: `make` inside its folder.
Its README says which files come out and where to install them.

## License

MIT, see [LICENSE](LICENSE).
