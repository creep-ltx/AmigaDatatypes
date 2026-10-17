/* qoienc: a QOI encoder that takes pixels a few rows at a time and hands
 * the bytes out in pieces, so neither the picture nor the file has to be in
 * memory whole. Plain C with no library calls: the same file runs in ToQOI
 * and in the tests on the PC (tests/enccheck.c), where every byte is checked
 * against the reference encoder. */
#ifndef QOIENC_H
#define QOIENC_H

#include "qoidec.h"                     /* the types, QOI_HEADER, QOI_PADDING */

#define QOI_OUTBUF 16384                /* bytes handed out at a time */

/* writes n bytes, gives 0 on failure */
typedef int (*QoiWrite)(void *ctx, const UBYTE *src, LONG n);

typedef struct {
    ULONG index[64];                    /* the format's table of recent colours */
    ULONG px;                           /* the last pixel, red in the top byte */
    ULONG run;                          /* repeats of px not yet written */
    ULONG left;                         /* pixels still to come */
    ULONG alphaand;                     /* every alpha ANDed: 255 = no transparency */
    QoiWrite write;
    void *ctx;
    int failed;                         /* a write failed; nothing more is written */
    UBYTE *o;                           /* the next free byte in buf */
    UBYTE buf[QOI_OUTBUF];
} QoiEnc;

/* starts a picture: writes the header. channels 3 or 4. */
void qe_begin(QoiEnc *e, ULONG width, ULONG height, int channels, QoiWrite write, void *ctx);

/* the next n pixels, 4 bytes each (red, green, blue, alpha); src must be
 * 2-byte aligned. With opaque set, the alpha bytes are ignored and taken as
 * 255 (a picture without transparency). */
void qe_pixels(QoiEnc *e, const UBYTE *src, ULONG n, int opaque);

/* writes what is left and the end marker. 0 = a write failed. */
int qe_end(QoiEnc *e);

#endif
