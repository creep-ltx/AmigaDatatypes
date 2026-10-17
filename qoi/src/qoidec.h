/* qoidec: a QOI decoder that reads the file in pieces and gives pixels a
 * row band at a time, so a picture never needs the whole file in memory.
 * Plain C with no library calls: the same file runs in the datatype and in
 * the tests on the PC (tests/), where it is checked against the reference
 * decoder and fuzzed. */
#ifndef QOIDEC_H
#define QOIDEC_H

#ifdef __amigaos__
#include <exec/types.h>
#else
#include <stdint.h>
typedef uint8_t UBYTE;
typedef uint32_t ULONG;
typedef uint16_t UWORD;
typedef int32_t LONG;
#endif

#define QOI_HEADER 14
#define QOI_PADDING 8
#define QOI_INBUF 16384                 /* bytes read from the file at a time */

/* the pixel layouts qd_pixels() can write */
#define QD_RGBA 0                       /* 4 bytes: red, green, blue, alpha */
#define QD_ARGB 1                       /* 4 bytes: alpha, red, green, blue */
#define QD_RGB  2                       /* 3 bytes: red, green, blue */

/* reads up to n bytes into dst, gives the number read (0 at the end) */
typedef LONG (*QoiRead)(void *ctx, UBYTE *dst, LONG n);

typedef struct {
    /* the 68020 loop (fast020.s) reads and writes these by their offsets:
     * keep them first and in this order */
    ULONG px;                           /*  0: the last pixel, red in the top byte */
    ULONG run;                          /*  4: pixels still to repeat */
    ULONG alphaand;                     /*  8: every alpha ANDed: 255 = no transparency */
    UBYTE *p;                           /* 12: the next unread byte in buf */
    UBYTE *lim;                         /* 16: ops start below lim (see qoidec.c) */
    ULONG h;                            /* 20: px's place in index */
    ULONG index[64];                    /* 24: the format's table of recent colours */
    /* the rest is C's own */
    ULONG width, height;
    UBYTE channels;                     /* 3 or 4, as the file says */
    UBYTE colorspace;                   /* 0 sRGB with linear alpha, 1 all linear */
    ULONG chunks;                       /* bytes of pixel data left before the 8 end bytes, from base */
    UBYTE *end;                         /* the end of the bytes in buf */
    UBYTE *base;                        /* where chunks counts from */
    QoiRead read;
    void *ctx;
    int eof;
    int shortfile;                      /* the data ended before the picture did */
    UBYTE buf[QOI_INBUF + 8];
} QoiDec;

/* the 14 header bytes; filesize is the whole file's size (the end padding
 * is not pixel data). 0 = not a QOI picture this decoder can show. */
int qd_header(QoiDec *d, const UBYTE *h, ULONG filesize, QoiRead read, void *ctx);

/* the next n pixels, in the layout fmt */
void qd_pixels(QoiDec *d, UBYTE *out, ULONG n, int fmt);

/* set to 1 on a 68020 or better (the Amiga build only): the hot loop
 * runs in fast020.s */
extern int qd_use020;

#endif
