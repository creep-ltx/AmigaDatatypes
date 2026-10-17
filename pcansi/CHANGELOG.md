# pcansi.datatype changes

## 0.1b2 (17.10.2026)

- **Animations play** once at modem speed (14,400 baud, `ENV:ANSIBaud` to change, 0 to never play), then
  hold their last picture, or their fullest if they end on an empty screen. Still art shows at once.
- Italic and underline (SGR 3 and 4) are shown.
- Long files: up to 12,000 rows (was 4,000).
- The reader and the recognition code are shared with the new amiansi.datatype (Amiga ANSI); the two
  never take the same file.

## 0.1b1 (17.10.2026)

- First version: PC ANSI art (`.ans`, `.ice`, BBS screens, PC `.nfo`/`.diz`/`.asc`) in MultiView and every
  datatypes program, as text in the IBM VGA font (`ibmvga.font`, 8 and 16 rows) with the sixteen PC colours.
- SAUCE: width, iCE colours, title and author in the window's title.
- Recognition by SAUCE record and by the bytes: the Amiga's own BBS screens and ASCII art are left alone.
- Workbench 3.2: text.datatype draws (marking and copying work); Workbench 3.1, or when memory is short:
  the class draws itself.
