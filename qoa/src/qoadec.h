/* qoadec: a QOA ("Quite OK Audio") decoder for the Amiga, a frame at a
 * time. Plain C, no C library, no 64-bit numbers; every frame is checked
 * before it is decoded, so a damaged file stops at the damage. The same
 * samples as the reference decoder (tests/ref/qoa.h, checked by make check). */
#ifndef QOADEC_H
#define QOADEC_H

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

#define QOA_HEADER    16                /* file header + the first frame header */
#define QOA_MAXCH     8
#define QOA_SLICE     20                /* samples in a slice (8 bytes) */
#define QOA_FRAMELEN  (256 * QOA_SLICE) /* samples a channel in a full frame */
#define QOA_FRAMEMAX(ch) (8 + 16 * (ch) + 256 * 8 * (ch))

typedef struct {
    LONG h[4], w[4];                    /* history (most recent last), weights */
} QoaLms;

typedef struct {
    ULONG channels, rate, samples;      /* samples a channel, from the file header */
    ULONG fsamples, fsize;              /* the frame qa_frameheader checked */
    QoaLms lms[QOA_MAXCH];
} QoaDec;

/* the first 16 bytes of the file: 1 if it is a QOA file we can play
 * (a known length, 1-8 channels) */
int qa_header(QoaDec *d, const UBYTE *b);

/* no more samples than a file of SIZE bytes can hold (a damaged length
 * would otherwise ask for gigabytes) */
void qa_limit(QoaDec *d, ULONG size);

/* a frame's 8-byte header: the bytes qa_frame reads (header included), or
 * 0 if it does not fit the file (other channels or rate, slices that do not
 * fit its size, no samples). Sets d->fsamples and d->fsize, the size the
 * header gives, which must still be in the file; the next frame starts
 * right after the bytes read, as in the reference decoder. */
ULONG qa_frameheader(QoaDec *d, const UBYTE *h);

/* the frame (as many bytes as qa_frameheader said, header first) into d->fsamples samples
 * a channel, interleaved like the file's channels */
void qa_frame(QoaDec *d, const UBYTE *frame, WORD *out);

#ifdef QA_ASM
/* 1: slice.s does the samples (the default), 0: the C loop */
extern int qa_useasm;
#endif

#endif
