# AmigaDatatypes

Datatypes for classic AmigaOS 3.x: the classes in `SYS:Classes/DataTypes` and their descriptors in
`DEVS:DataTypes` that teach every datatypes program (MultiView, AmigaGuide, browsers, viewers) to open
a new kind of file. One folder per datatype, each with its own README, version and change log.

| Datatype | What | Version |
|---|---|---|
| [markdown/](markdown/) | **Markdown** (`README.md`) formatted in MultiView and every datatypes program: headings, lists, quotes, emphasis, code with colours, tables | 0.1b2 |
| [utf8/](utf8/) | **UTF-8 text** with the right letters (Latin-1 and the nearest stand-in for the rest) instead of two or three wrong ones per letter; found by what is in the file, not its name. Needs utf8.library | 0.1b1 |
| *(planned)* ansi | **ANSI art** (`.ans`, BBS screens) in colour, as text | - |

All for a plain 68000 and Kickstart 3.0 or later.

Only datatypes live here. Libraries are in AmigaLibs; handlers, devices and programs each have their
own repository.

## Building

Each datatype builds on its own with bebbo's m68k-amigaos-gcc: `make` inside its folder.
Its README says which files come out and where to install them.

## License

MIT, see [LICENSE](LICENSE).
