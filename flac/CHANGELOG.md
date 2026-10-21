# flac.datatype changes

## 0.1b2 (21.10.2026)

- **Big files were sometimes cut to a fraction of a second:** the sanity limit on the length (so that a
  damaged header cannot ask for all the memory) overflowed 32 bits for files over about 10 MB (2.6 MB with
  16,384-sample blocks) and could wrap round below the real length. A 2,621,440-byte test file opened as
  0.37 s; now as its whole 16 s. Found by an audit.
- **FLAC files with ID3v2 tags in front** (some taggers write them) are recognised and played: a second
  descriptor, `FLAC-ID3` (mask "ID3" and 404 bytes of recognition code that looks past the tags for
  `fLaC`), and the decoder skips up to 4 tags. MP3s and other files with ID3 tags are left alone.
- rice.s: its comment said it reads up to 4 bytes past a frame's end; it is up to 12 (the buffer has 16).
- Tested: the PC corpus with 79 ID3-tagged files (612 files the same as ffmpeg, 1,606 damaged ones came
  through); on OS 3.2.1 and 3.1 (FS-UAE) the big file, a tagged FLAC, a tagged MP3 (not taken) and a plain
  file, no memory lost.

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
