# AmigaDatatypes

Datatypes for classic AmigaOS 3.x: the classes in `SYS:Classes/DataTypes` and their descriptors in
`DEVS:DataTypes` that teach every datatypes program (MultiView, AmigaGuide, browsers, viewers) to open
a new kind of file. One folder per datatype, each with its own README, version and change log.

| Datatype | What | Version |
|---|---|---|
| *(planned)* ansi | **ANSI art** (`.ans`, BBS screens) in colour, as text | - |
| *(planned)* markdown | **Markdown** (`README.md`) formatted: headings, lists, emphasis, code | - |

Both run on a plain 68000 and Kickstart 3.0 or later.

Only datatypes live here. Libraries are in AmigaLibs; handlers, devices and programs each have their
own repository.

## Building

Each datatype builds on its own with bebbo's m68k-amigaos-gcc: `make` inside its folder.
Its README says which files come out and where to install them.

## License

MIT, see [LICENSE](LICENSE).
