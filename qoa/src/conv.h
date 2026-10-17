/* conv: decoded 16-bit frames into the 8-bit sound sound.datatype plays:
 * averaged down to a rate Paula can play, all channels mixed into one or
 * the first two kept as stereo, rounded to 8 bits. Plain C, used by the
 * datatype and by the tests on the PC. */
#ifndef CONV_H
#define CONV_H

#include "qoadec.h"

#define PAULAMAX 28000                  /* Hz, sound.datatype's own limit */

typedef struct {
    BYTE *out[2];                       /* left (or the only one), right; the caller allocates len bytes each */
    ULONG pos, len;                     /* samples written, room for */
    ULONG ch, outch, step;              /* file's channels, 1 or 2, file samples averaged into one */
    ULONG shift, div;                   /* sum -> 8 bit: >> shift, or / div then >> 8 */
    ULONG cnt;
    LONG acc[2];
} Conv;

/* set up for the file qa_header read: stereo only when STEREO and the file
 * has two channels or more. Returns the rate the sound plays at; c->len is
 * the samples each output channel needs. */
ULONG cv_init(Conv *c, const QoaDec *d, int stereo);

/* N samples a channel, interleaved as qa_frame gives them */
void cv_frame(Conv *c, const WORD *s, ULONG n);

#endif
