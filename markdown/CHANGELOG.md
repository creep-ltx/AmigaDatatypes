# markdown.datatype changes

## 0.1b1 (16.10.2026)

- The first version: a text.datatype subclass that shows Markdown formatted, with the Markdown viewer's
  renderer built in. One build for every Amiga (68000, Kickstart 3.0+).
- Links italic instead of underlined where text.datatype is 45 or later (3.1.4, 3.2): it loses the underline
  of a piece 256 pixels or more from the left and carries it into the next piece.
- md.c's work buffers are freed when the datatype is unloaded (were lost at every Avail FLUSH).
- The Markdown runs on a 32 KB stack of its own (a datatype runs on its caller's).
