/* flacdec: a FLAC decoder for the Amiga, a frame at a time. Plain C, no C
 * library. Every frame is checked (header CRC-8, frame CRC-16) before its
 * samples count, so a damaged frame is skipped and the next one found.
 * 4 to 24 bits a sample, 1 to 8 channels. The same samples as ffmpeg's and
 * libFLAC's decoders (checked by make check). */
#ifndef FLACDEC_H
#define FLACDEC_H

#ifdef __amigaos__
#include <exec/types.h>
#else
#include <stdint.h>
typedef int8_t BYTE;
typedef uint8_t UBYTE;
typedef int16_t WORD;
typedef uint16_t UWORD;
typedef uint32_t ULONG;
typedef int32_t LONG;
#endif

#define FL_MAXCH   8
#define FL_INFO    34                   /* the STREAMINFO block's size */

typedef struct {
    /* from STREAMINFO (fl_streaminfo) */
    ULONG rate, channels, bps, samples; /* samples a channel; 0 if the file does not say */
    ULONG minblock, maxblock, maxframe;
    ULONG fixed;                        /* block size of a fixed-block stream, 0 until known */
    /* the frame fl_frame decoded */
    ULONG fstart, fsamples;             /* its first sample, samples a channel */
    LONG *pcm[FL_MAXCH];                /* the caller's: maxblock samples each */
    /* the bit reader */
    const UBYTE *p, *end;
    ULONG c;                            /* the next bits, from the top */
    LONG n;                             /* how many of c's bits are valid */
} FlacDec;

/* once, before anything else: the CRC tables */
void fl_init(void);

/* a metadata block's 4-byte header: its type (0 STREAMINFO, 4 VORBIS_COMMENT...),
 * *len its length, *last 1 if it is the last one */
ULONG fl_blockheader(const UBYTE *b, ULONG *len, int *last);

/* the 34 bytes of STREAMINFO: 1 if we can play it */
int fl_streaminfo(FlacDec *d, const UBYTE *b);

/* the most bytes one frame of this stream can take */
ULONG fl_framemax(const FlacDec *d);

/* the frame at b (len bytes there, and 16 more that may be read but do
 * not count): its size in bytes if it is whole and
 * right, with its samples in d->pcm, d->fstart and d->fsamples; 0 if not */
ULONG fl_frame(FlacDec *d, const UBYTE *b, ULONG len);

/* where the next frame could start in b, from offset 1 on (a frame sync
 * code whose header checks out); len if none */
ULONG fl_sync(const FlacDec *d, const UBYTE *b, ULONG len);

/* a VORBIS_COMMENT block: the TITLE and ARTIST, in Latin-1, into title and
 * artist (size bytes each, always ended with 0; empty if not there) */
void fl_comment(const UBYTE *b, ULONG len, char *title, char *artist, ULONG size);

#ifdef FL_ASM
/* 1: rice.s reads the residuals (the default), 0: the C loop */
extern int fl_useasm;
#endif

#endif
