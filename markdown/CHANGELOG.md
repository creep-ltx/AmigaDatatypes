# markdown.datatype changes

## 0.1b2 (17.10.2026)

- **Big files on small Amigas:** the file is laid out a part at a time (about 4 KB each, cut where a block
  ends; lists stay whole unless over 16 KB), so it no longer needs some 8 bytes a character in one block. A part
  that does not fit is tried again in smaller parts. If memory still runs out, what fitted is shown with a line
  saying so, instead of an empty window.
- **Less memory:** on Amigas with under 3 MB, table borders, code frames and rules are drawn in the text's pen,
  so a table row is one piece instead of one per border. Long-lived memory comes from the top (MEMF_REVERSE),
  leaving big blocks free below. A 16 KB stack instead of 32 KB (nesting deeper than 10 is shown flat).
- Tested on a stock A600 (68000, 1 MB, Kickstart 3.1): a 7.7 KB README shows with 519 KB still free; a 96 KB
  one shows its first 593 rows and the note (it would need about 750 KB for all of it).

## 0.1b1 (16.10.2026)

- The first version: a text.datatype subclass that shows Markdown formatted, with the Markdown viewer's
  renderer built in. One build for every Amiga (68000, Kickstart 3.0+).
- Links italic instead of underlined where text.datatype is 45 or later (3.1.4, 3.2): it loses the underline
  of a piece 256 pixels or more from the left and carries it into the next piece.
- md.c's work buffers are freed when the datatype is unloaded (were lost at every Avail FLUSH).
- The Markdown runs on a 32 KB stack of its own (a datatype runs on its caller's).
