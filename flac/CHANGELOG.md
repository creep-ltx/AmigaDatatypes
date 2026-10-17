# flac.datatype changes

## 0.1b1 (17.10.2026)

- The first version: a sound.datatype subclass for FLAC sounds, one build for every Amiga (68000,
  Kickstart 3.0+). 4 to 24 bits a sample, 1 to 8 channels, any rate, fixed and variable block sizes.
- Stereo where sound.datatype has it (V44+, OS 3.5/3.9/3.2), all channels mixed into mono on OS 3.0/3.1.
  Sound above 28 kHz averaged down to a rate Paula plays.
- Read through a small window and decoded a frame at a time into the 8-bit sound; the Rice loop in 68000
  assembly, the predictor's multiplies 16x16 only (no 32-bit or 64-bit multiply calls), also for 24-bit sound.
- Every frame checked (header CRC-8, frame CRC-16); a damaged frame is skipped, silence there, and the next
  one found. The same samples as ffmpeg's decoder on 533 test files (and 1,600 damaged ones came through).
- The TITLE and ARTIST tags become the sound's name and author.
