# markdown.datatype

Markdown files shown formatted, not as raw text, in every program that shows text through datatypes:
MultiView, AmigaGuide, a double-click on Workbench, file managers and viewers.

- **Headings** in bold and colour; the top one as a band across the window
- **Emphasis**: bold, italic, `code` set apart, links in blue (underlined on OS 3.0/3.1, italic on 3.1.4 and 3.2,
  whose text.datatype draws underline wrong)
- **Lists**, numbered and not, nested, and task lists (`[x]`)
- **Quotes**, nested, and GitHub's `> [!NOTE]` alerts
- **Code blocks**, with colours for the common languages (C, assembler, ARexx, E, Python, shell...)
- **Tables**, columns lined up and aligned as the file says
- Wrapped to the window's width, again when the window changes
- **UTF-8 and Latin-1** files both; characters beyond Latin-1 shown as the nearest one the font has
- Search, marking and copying as for any text (text.datatype does those)

**Needs** Kickstart/Workbench 3.0 or later, any processor from the 68000 up. No other files.

## Install

| File | Goes to |
|---|---|
| `markdown.datatype` | `SYS:Classes/DataTypes/` |
| `Markdown` | `DEVS:DataTypes/` |
| `MarkdownBin` | `DEVS:DataTypes/` |

Then `AddDataTypes REFRESH` (or restart). Files ending in `.md`, `.markdown`, `.mkd` or `.mdown` open with it.

`MarkdownBin` is for UTF-8 files: datatypes.library looks at a file's first bytes to decide whether it is text
or binary, and some UTF-8 text looks binary to it. The two descriptors are the same otherwise.

## How it works

A subclass of `text.datatype`, as `ascii.datatype` is. The Markdown is read by the renderer of the
Markdown viewer (the TextTools repository, `markdown/src/md.c`), built in as it is, so the datatype and the viewer show a
file the same way. Its colours become the four Workbench pens (text, white, blue) and its attributes the soft
styles (bold, italic, underline). Pictures in the file are not shown (a text datatype draws text only).

## Tested

In FS-UAE (`tests/`): OS 3.1 on a 68020; Workbench 3.2.1 on a 68020; and Workbench 3.2.1 on an A600 with a
plain 68000, the Kickstart 3.2 A500/A600 ROM and no fast RAM. A 7.7 KB README took about 12 seconds to open on
the 68000, MultiView's own start included. Open and close with `tests/leak.sh`: nothing lost.

Memory: about five times the file while it is shown (an 89 KB README took about 450 KB; the 7.7 KB one about
210 KB with MultiView). Not yet tried on 1 MB: Workbench 3.2.1 left too little for the test tools there.

## Building

`make` with bebbo's m68k-amigaos-gcc. This repository, TextTools and AmigaLibs must sit side by side
(`TEXTTOOLS ?= ../../TextTools`, `TUI ?= ../../AmigaLibs/tui`).

## Versions

See [CHANGELOG.md](CHANGELOG.md).
