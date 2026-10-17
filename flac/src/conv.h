/* conv: decoded FLAC blocks into the 8-bit sound sound.datatype plays:
 * averaged down to a rate Paula can play, all channels mixed into one or
 * the first two kept as stereo, rounded to 8 bits. Plain C, used by the
 * datatype and by the tests on the PC. */
#ifndef CONV_H
#define CONV_H

#include "flacdec.h"

#define PAULAMAX 28000                  /* Hz, sound.datatype's own limit */

typedef struct {
    BYTE *out[2];                       /* left (or the only one), right; the caller allocates len bytes each */
    ULONG pos, len;                     /* samples written, room for */
    ULONG in;                           /* the file's next sample (a channel) */
    ULONG ch, outch, step;              /* file's channels, 1 or 2, file samples averaged into one */
    ULONG shift, div;                   /* sum -> 8 bit: >> shift, or / div then >> shift */
    LONG up;                            /* fewer than 8 bits a sample: << up first */
    ULONG cnt;
    LONG acc[2];
} Conv;

/* set up for the stream: stereo only when STEREO and the file has two
 * channels or more; SAMPLES a channel at most. Returns the rate the sound
 * plays at; c->len is the samples each output channel needs. */
ULONG cv_init(Conv *c, const FlacDec *d, ULONG samples, int stereo);

/* the block fl_frame decoded (d->pcm, d->fsamples from d->fstart); a block
 * further on leaves silence for the missing ones, one already passed is
 * left out */
void cv_block(Conv *c, const FlacDec *d);

#endif
