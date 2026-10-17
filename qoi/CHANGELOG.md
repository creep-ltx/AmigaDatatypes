# qoi.datatype changes

## 0.1b1 (17.10.2026)

- The first version: a picture.datatype subclass for QOI pictures, one build for every Amiga (68000,
  Kickstart 3.0+).
- Full colour with transparency where picture.datatype takes it (V43+); straight into its pixel buffer on
  OS 3.2 (V47). 256 colours with median cut and dithering on OS 3.0/3.1 (V39-42).
- The decoder reads the file 16 KB at a time; a 68020 loop in assembly, chosen at run time.
- Byte for byte the reference decoder's pixels, also for broken files (450 test pictures on the PC and the
  Amiga, fuzzing on the PC).
