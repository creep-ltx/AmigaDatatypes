# qoi.datatype changes

## ToQOI 0.1b1 (17.10.2026)

- New: `ToQOI`, a command that turns any picture datatypes can read (PNG, JPEG, GIF, BMP, WebP, ILBM...)
  into a QOI file. Full colour with transparency on OS 3.5+, the picture's own bitmap and palette on 3.0/3.1
  (HAM and Extra-Halfbrite included). Patterns, a TO drawer, FORCE, QUIET, and Workbench start (select
  pictures, shift-double-click ToQOI). Written a few rows at a time; byte for byte the reference encoder's
  output.

## 0.1b1 (17.10.2026)

- The first version: a picture.datatype subclass for QOI pictures, one build for every Amiga (68000,
  Kickstart 3.0+).
- Full colour with transparency where picture.datatype takes it (V43+); straight into its pixel buffer on
  OS 3.2 (V47). 256 colours with median cut and dithering on OS 3.0/3.1 (V39-42).
- The decoder reads the file 16 KB at a time; a 68020 loop in assembly, chosen at run time.
- Byte for byte the reference decoder's pixels, also for broken files (450 test pictures on the PC and the
  Amiga, fuzzing on the PC).
