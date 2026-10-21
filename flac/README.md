# flac.datatype

FLAC sounds (the Free Lossless Audio Codec, [xiph.org/flac](https://xiph.org/flac/)) in every program that plays
sounds through datatypes: MultiView, a double-click on Workbench, file managers, AmigaGuide, browsers.

- **Every Amiga with OS 3.0 or later**, any processor from the 68000 up, one file for all of them
- **Every kind of FLAC file:** 4 to 24 bits a sample, 1 to 8 channels, any rate, every encoder setting (block
  sizes 16 to 65,535, prediction up to order 32, fixed and variable block sizes)
- **Stereo on OS 3.5, 3.9 and 3.2** (sound.datatype 44 and later); files with more channels play their first
  two, front left and right
- **Mono on OS 3.0 and 3.1**, whose sound.datatype has no stereo: all the channels are mixed into one
- **44.1 and 48 kHz files play at the right speed:** Paula plays at most about 28 kHz, so faster sound is
  averaged down to half (or a third, a quarter...) while it is decoded, instead of playing slow
- **Title and artist:** the file's TITLE and ARTIST tags become the sound's name and author (Latin-1)
- **Made for the 68000:** the bit reader's inner loop is hand-written 68000 assembly, and the predictor only
  ever multiplies 16 by 16 bits (`muls.w`), 24-bit sound too: no 32-bit or 64-bit multiply calls
- **Little memory:** the file is read through a small window (the largest frame there can be and 4 KB more:
  about 24 KB for CD sound) and decoded straight into the 8-bit sound; only that is held. Cover pictures and
  other metadata are skipped
- **Save as 8SVX:** MultiView's *Save As* writes an 8SVX with the right rate and length in its header
- **Safe with broken files:** every frame is checked (header CRC-8 and frame CRC-16) before its samples count;
  a damaged frame is left out (silence there) and the next one is found. A file that only *says* it is long
  cannot make it ask for more memory than the file could hold
- **The same samples as ffmpeg's decoder** on 612 test files from two encoders, and 1,600 damaged files
  came through it; on the Amiga, every sample the same as on the PC
- **Small:** about 14 KB
- Recognised by its first bytes (`fLaC`), whatever the file is called, also **behind ID3 tags**, which some
  taggers write into FLAC files (0.1b2); an MP3 with an ID3 tag is left to its own datatype

Not played: files that do not say their length (some streamed recordings), files over 24 bits a sample and Ogg
FLAC (`.oga`).

## Install

| File | Goes to |
|---|---|
| `flac.datatype` | `SYS:Classes/DataTypes/` |
| `FLAC` | `DEVS:DataTypes/` |
| `FLAC-ID3` | `DEVS:DataTypes/` (FLAC files with ID3 tags in front) |

Then `AddDataTypes REFRESH`, or restart the Amiga (restart when updating from 0.1b1).

On a plain 68000 or 68020 the whole sound is decoded before it plays, which takes several times longer than it
plays: keep long songs for faster Amigas, or make 22 kHz copies (`ffmpeg -i in.flac -ar 22050 out.flac`).

## Speed

A datatype decodes the whole sound before it plays, so the time is waiting time before the sound starts.
Every sample has to be decoded, whatever rate it plays at. Opening a sound, MultiView's own start left out:

| Sound | Real A1200, Emu68, OS 3.2.3 |
|---|---|
| 60 s, 44.1 kHz stereo, 16 bits (`flac -5`) | 1.24 s |
| 10 s, 44.1 kHz stereo, 16 bits (`flac -8`) | 0.18 s |
| 5 s, 48 kHz stereo, 24 bits (`flac -8`) | 0.18 s |
| 5 s, 22 kHz mono, 16 bits (`flac -5`) | 0.02 s |

Not yet measured on a real 68000, 68020 or 68030. In FS-UAE (emulated, not real hardware) a 14 MHz 68020
decoded about 26,000 samples a second a channel and a 7 MHz 68000 about 5,000: there 10 s of CD sound takes
about 17 s on the 68020 to open, and 5 s of 22 kHz mono about 23 s on the 68000.

FLAC is heavier than QOA: each sample is predicted from up to 32 before it (12 with `flac -8`), each a
multiply. On a 68000 or 68020 a sound takes longer to open than to play. **For slow Amigas, encode at
22,050 Hz** (`flac` keeps the rate it is given; Paula cannot play more anyway) **and with fewer predictors:**
`flac -2` and below use only the fixed predictors, which need no multiplies:

```
ffmpeg -i song.flac -ar 22050 song.wav       (add -ac 1 for mono)
flac -3 song.wav
```

## How it works

A subclass of `sound.datatype`. `src/stream.c` reads the metadata and then the file through a window (the
largest frame plus 4 KB); `src/flacdec.c` checks and decodes a frame at a time; `src/rice.s` is the loop that
reads the Rice-coded residuals, for every 68k. The predictor's sums are kept in 32 bits when the coefficients cannot
carry them further and in 64 when they can, with only 16x16-bit multiplies either way (a 17-bit side channel
as 2 * (x >> 1) + (x & 1), 24-bit samples in two halves). `src/conv.c` turns each block into the 8-bit sound
as it comes: averaged over 2, 3 or more samples above 28 kHz, channels mixed or kept, rounded. Then:

| sound.datatype | OS | What it gets |
|---|---|---|
| 44 and later | 3.5, 3.9, 3.2 | `SDTA_LeftSample` + `SDTA_RightSample` (or `SDTA_Sample`), `SDTA_SamplesPerSec`, a filled-in VoiceHeader |
| 40 | 3.0, 3.1 | `SDTA_Sample` (mono, even length), `SDTA_Period` from the machine's own clock (PAL or NTSC) |

## Tested

- **The decoder, on the PC:** every sample against ffmpeg's decoder on 533 files: ffmpeg's sine, noise, pink
  noise, sweeps, a clipped square, mixes and silence through the `flac` command (levels 0-8, exhaustive search,
  block sizes 16, 192, 4608 and 65,535, prediction up to order 32, 15-bit coefficients, 4/8/12/16/20/24 bits,
  wasted bits, 1-8 channels, 8 kHz to 655 kHz, lengths of 1 to 9,000 samples) and through ffmpeg's own encoder
  (its stereo modes, order 32, 16 and 24 bits); variable-block copies of a quarter of them; and 1,600 damaged
  copies (bytes changed, cut short, wrong length, rate, block and frame sizes) that must only come through.
  With the address and undefined-behaviour checkers on.
- **The Amiga build's full samples against the PC's:** 126 of those files decoded on OS 3.2.1 (FS-UAE),
  every sample of every frame the same. (This caught a compiler fault the PC could not show; see below.)
- **Through datatypes:** eight test sounds (16 and 24 bits, mono, stereo, six channels, ffmpeg's encoder, tags
  and a cover picture, a damaged one, a minute long) opened as MultiView does, their 8-bit samples fetched back
  and compared with the PC's, byte for byte: OS 3.2.1 (68020) in FS-UAE, a 68000 A600 with OS 3.1 in FS-UAE
  (mono), and the real A1200 (Emu68, OS 3.2.3). Opening and closing three times loses no memory. MultiView
  shows the title in its window and "FLAC Sound" in its About box; a non-FLAC file is refused.
- bebbo's gcc 6.5 compiled `r[i] = l[i] - r[i]` as `r[i] -= l[i]` for the 68000, which turned the right
  channel of left/side stereo frames upside down on the Amiga only. `src/flacdec.c` writes that loop so it
  cannot happen.

## Building

`make` with bebbo's m68k-amigaos-gcc. `make check` compares the decoder with ffmpeg's on the PC (needs ffmpeg,
flac; makes about 2,100 test files). `make amigatest` with `WASABI_HOST`/`WASABI_PORT` pointing at a test
Amiga runs the test sounds there (`STEREO=0` on OS 3.0/3.1). `make bench` builds `bench`, which times the
decoder alone and compares the assembly and C loops.

## Versions

See [CHANGELOG.md](CHANGELOG.md).
