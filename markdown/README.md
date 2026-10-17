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

In FS-UAE (`tests/`): OS 3.1 on a 68020; Workbench 3.2.1 on a 68020; and a stock A600 (68000, 1 MB, Kickstart
and Workbench 3.1). A 7.7 KB README took about 12 seconds to open on
the 68000, MultiView's own start included. Open and close with `tests/leak.sh`: nothing lost.

Memory: the file is laid out a part at a time, so what counts is what is shown: about 350 bytes a row of
text on screen. On a stock A600 (68000, 1 MB, Kickstart 3.1, no fast RAM) a 7.7 KB README opens with 519 KB
still free; a 96 KB one shows its first 593 rows and says the rest did not fit. On Amigas with under 3 MB, table
borders and code frames are drawn in the text's colour (fewer pieces to keep).

## Building

`make` with bebbo's m68k-amigaos-gcc. This repository, TextTools and AmigaLibs must sit side by side:

    git clone https://github.com/creep-ltx/AmigaDatatypes
    git clone https://github.com/creep-ltx/texttools TextTools
    git clone https://github.com/creep-ltx/AmigaLibs
    cd AmigaDatatypes/markdown && make

(Elsewhere: `make TEXTTOOLS=... TUI=.../AmigaLibs/tui`.)

## Versions

See [CHANGELOG.md](CHANGELOG.md).
