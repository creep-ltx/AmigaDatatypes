# qoa.datatype

QOA sounds (the "Quite OK Audio" format, [qoaformat.org](https://qoaformat.org)) in every program that plays
sounds through datatypes: MultiView, a double-click on Workbench, file managers, AmigaGuide, browsers.

- **Every Amiga with OS 3.0 or later**, any processor from the 68000 up, one file for all of them
- **Stereo on OS 3.5, 3.9 and 3.2** (sound.datatype 44 and later); files with more channels play their first
  two, front left and right
- **Mono on OS 3.0 and 3.1**, whose sound.datatype has no stereo: all the channels are mixed into one
- **44.1 and 48 kHz files play at the right speed:** Paula plays at most about 28 kHz, so faster sound is
  averaged down to half (or a third, a quarter...) while it is decoded, instead of playing slow
- **Fast:** the per-sample loop is hand-written 68000 assembly (2.8x the C loop on a 68000, 5.7x on the real
  A1200). On the real A1200 (Emu68, OS 3.2) a minute of 44.1 kHz stereo opens in 0.8 seconds
- **Little memory:** the file is read a frame (at most 16 KB) at a time and decoded straight into the 8-bit
  sound; only that is held, about 2.5 times the file's size in stereo, half that in mono
- **Save as 8SVX:** MultiView's *Save As* writes an 8SVX with the right rate and length in its header
- **Safe with broken files:** every frame is checked before it is decoded; a damaged file plays up to the
  damage, a file that only *says* it is long cannot make it ask for more memory than the file could hold.
  The same samples as the reference decoder (`qoa.h`) on 1,100 test files, whole and damaged
- **Small:** about 6 KB
- Recognised by its first bytes (`qoaf`), whatever the file is called

Not played: "streaming" QOA files (no length in the header; rate and channels may change on the way) and files
with more than 8 channels.

## Install

| File | Goes to |
|---|---|
| `qoa.datatype` | `SYS:Classes/DataTypes/` |
| `QOA` | `DEVS:DataTypes/` |

Then `AddDataTypes REFRESH`, or restart the Amiga.

## Making QOA files

With the reference converter `qoaconv` from [github.com/phoboslab/qoa](https://github.com/phoboslab/qoa) on a PC
(WAV in; MP3 and FLAC too when built with dr_libs). ffmpeg plays QOA but does not write it. **For the Amiga,
encode at 22,050 Hz or lower:** Paula cannot play more anyway, the file is half the size, and a 68000 or 68020
opens it twice as fast. Mono halves it again. `qoaconv` keeps the rate it is given, so make the WAV first:

```
ffmpeg -i song.flac -ar 22050 song.wav       (add -ac 1 for mono)
qoaconv song.wav song.qoa
```

## Speed

Every sample has to be decoded, at any playback rate: QOA predicts each sample from the four before it. Opening
a sound, the datatype's own part:

| Sound | Real A1200, Emu68, OS 3.2 |
|---|---|
| 10 s, 44.1 kHz stereo | 0.14 s |
| 60 s, 44.1 kHz stereo | 0.78 s |
| 5 s, 22 kHz mono | 0.02 s |

On the real A1200 the decoder alone does 11 million samples a second. A plain 68000 or 68020 is much slower:
there a sound takes longer to open than to play, the more so at 44.1 kHz in stereo. Not yet measured on real
68000 or 68020 hardware; in FS-UAE (emulated, not real hardware) a 7 MHz A600 decoded about 7,600 samples a
second.

## How it works

A subclass of `sound.datatype`. `src/qoadec.c` checks and decodes a frame at a time (5,120 samples a channel);
`src/slice.s` is its per-sample loop for every 68k, used while the predictor's weights fit 16 bits (nearly
always; the C loop takes over when they do not). `src/conv.c` turns the 16-bit samples into the 8-bit sound
as they come: averaged over 2, 3 or more samples above 28 kHz, channels mixed or kept, rounded. Then:

| sound.datatype | OS | What it gets |
|---|---|---|
| 44 and later | 3.5, 3.9, 3.2 | `SDTA_LeftSample` + `SDTA_RightSample` (or `SDTA_Sample`), `SDTA_SamplesPerSec`, a filled-in VoiceHeader |
| 40 | 3.0, 3.1 | `SDTA_Sample` (mono, even length), `SDTA_Period` from the machine's own clock (PAL or NTSC) |

## Tested

- **The decoder, on the PC:** every sample against the reference decoder (`tests/ref/qoa.h`) on about 1,100
  files: ffmpeg's sine, noise, sweeps, a clipped square and mixes at 8-48 kHz and 96 kHz, 1-8 channels, lengths
  around the slice and frame sizes, through the reference encoder; and damaged copies (bytes changed, cut short,
  wrong length, channels, rate, frame size). With the address and undefined-behaviour checkers on.
- **On the Amiga:** six test sounds opened through datatypes, their 8-bit samples fetched back and compared
  with the PC's, byte for byte: OS 3.2.1 (68020) in FS-UAE, a 68000 A600 with OS 3.1 in FS-UAE, and the real
  A1200 (Emu68, OS 3.2.3). The assembly loop gives the same samples as the C loop on all three. Opening and
  closing three times loses no memory. MultiView shows and saves them (OS 3.2.1); a non-QOA file is refused.

## Building

`make` with bebbo's m68k-amigaos-gcc. `make check` compares the decoder with the reference one on the PC
(needs ffmpeg; `tests/ref/qoa.h`, MIT, Dominic Szablewski). `make amigatest` with `WASABI_HOST`/`WASABI_PORT`
pointing at a test Amiga runs the same test sounds there (`STEREO=0` on OS 3.0/3.1). `make bench` builds
`bench`, which times the decoder alone and compares the assembly and C loops.

## Versions

See [CHANGELOG.md](CHANGELOG.md).
