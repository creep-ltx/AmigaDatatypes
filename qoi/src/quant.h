/* quant: QOI pictures for picture.datatype before V43 (OS 3.0, 3.1), which
 * takes no 24-bit pixels: at most 256 colours. A picture with 256 colours
 * or fewer keeps them exactly; a bigger one gets 256 by median cut and
 * Floyd-Steinberg dithering. Two passes over the pixels, a row at a time:
 * the picture is decoded twice instead of being kept whole. Plain C, tested
 * on the PC as qoidec is. */
#ifndef QUANT_H
#define QUANT_H

#include "qoidec.h"

#ifndef __amigaos__
typedef int16_t WORD;
#endif

#define QU_BG 0xaa                      /* transparency is shown over Workbench's grey */

typedef struct {
    /* dither.s reads these three by their offsets: keep them first */
    UBYTE pal[256][4];                  /*    0: red, green, blue, unused */
    UBYTE inv[4096];                    /* 1024: 4:4:4 colour -> nearest palette entry, */
    UBYTE invok[512];                   /* 5120: filled in as colours come (the dithering */
                                        /*       makes up for the coarser steps) */
    UBYTE ord[256];                     /* the palette sorted by green, for nearest() */
    UWORD gstart[256];                  /* where each green starts in ord */
    int ncolors;
    int nexact, overflow;
    int flip;                           /* qu_map: which half of err is this row's */
    ULONG exact[512];                   /* the exact colours while there are 256 or fewer: */
    UBYTE exactix[512];                 /* a small hash table, colour -> palette entry */
    UWORD hist[32768];                  /* how often each 5:5:5 colour comes (saturates) */
    ULONG cells[32768];                 /* median cut's work list: colour << 16 | count */
    struct QuBox { ULONG first, n, pixels; } box[256];  /* its boxes: here, not on the
                                        stack (a datatype runs on its caller's, 4 KB for a
                                        MultiView started from a Shell) */
} Quant;

void qu_init(Quant *q);
/* blend a row of RGBA pixels (red in the top byte) over QU_BG, in place */
void qu_flatten(ULONG *px, ULONG n);
/* pass 1: count a row's colours */
void qu_count(Quant *q, const ULONG *px, ULONG n);
/* after pass 1: make the palette (q->pal, q->ncolors) */
void qu_palette(Quant *q);
/* pass 2: a row as palette numbers; err is 6 * (n + 2) WORDs, zeroed
 * before the first row (Floyd-Steinberg's carried error), q->flip 0 */
void qu_map(Quant *q, const ULONG *px, UBYTE *out, ULONG n, WORD *err);

#endif
