# qoa.datatype changes

## 0.1b1 (17.10.2026)

- The first version: a sound.datatype subclass for QOA sounds, one build for every Amiga (68000,
  Kickstart 3.0+).
- Stereo where sound.datatype has it (V44+, OS 3.5/3.9/3.2), all channels mixed into mono on OS 3.0/3.1.
  Sound above 28 kHz averaged down to a rate Paula plays.
- Read and decoded a frame at a time into the 8-bit sound; the per-sample loop in 68000 assembly.
- Every frame checked; damaged files play up to the damage. The same samples as the reference decoder
  (about 1,100 test files on the PC; the 8-bit result byte for byte on OS 3.2.1, OS 3.1 on a 68000 and the
  real A1200).
