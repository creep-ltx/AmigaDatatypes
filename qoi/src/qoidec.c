/* qoidec: see qoidec.h. The format: https://qoiformat.org/qoi-specification.pdf
 *
 * The pixel is kept as one 32-bit number, red in the top byte and alpha in
 * the bottom one. The colour changes add a small number to each byte; they
 * are made into one 32-bit number from a table and added to all four bytes
 * at once without carries between them (swaradd).
 *
 * The table position of a colour, (r*3 + g*5 + b*7 + a*11) mod 64, is not
 * worked out again for every pixel: a byte that wraps past 255 loses 256,
 * and 256 times any of 3, 5, 7, 11 is 0 mod 64, so a change of colour
 * changes the position by the position of the change. The diff and luma
 * ops add a number from a table; an index op's position is the op itself
 * (or 0 for an empty slot); a run keeps it. Only a full colour (rgb, rgba)
 * is looked up byte by byte, in small tables of the four products.
 * Byte for byte the result
 * is the reference decoder's (qoi.h), also for broken files: tests/ checks
 * that. */
#include "qoidec.h"

#define MAXPIXELS 400000000UL          /* as qoi.h: no picture over this */

/* add the four bytes of b to those of a, each on its own (mod 256) */
#define swaradd(a, b) ((((a) & 0x7f7f7f7fUL) + ((b) & 0x7f7f7f7fUL)) ^ (((a) ^ (b)) & 0x80808080UL))

/* the tables, in one block that fast020.s reads by these offsets: positions
 * (h) are stored times 4 there, as fast020.s keeps them */
struct QoiTab {
    ULONG difftab[64];                  /*     0: QOI_OP_DIFF's three changes, by the op's low bits */
    UBYTE diffh[64];                    /*   256: the same as a change of position (x4) */
    ULONG lumagb[1024];                 /*   320: QOI_OP_LUMA by the 6 bits and the low half: green and blue */
    ULONG lumar[1024];                  /*  4416: and by the high half: red (different bytes: ORed) */
    UBYTE lumahgb[1024];                /*  8512: their changes of position (x4) */
    UBYTE lumahr[1024];                 /*  9536 */
    UBYTE h3[256], h5[256], h7[256], h11[256];  /* 10560: each byte's share of a position (x4) */
};
struct QoiTab qd_tab;
#define T qd_tab
int qd_use020;
#ifdef QD_ASM
ULONG qd_fast020(QoiDec *d, UBYTE **out, ULONG n, int rgb);
#endif
static int tables;

#define B3(r, g, b) ((((ULONG)(r) & 255) << 24) | (((ULONG)(g) & 255) << 16) | (((ULONG)(b) & 255) << 8))

static void maketables(void)
{
    int i;
    for (i = 0; i < 64; i++) {
        int dr = ((i >> 4) & 3) - 2, dg = ((i >> 2) & 3) - 2, db = (i & 3) - 2;
        T.difftab[i] = B3(dr, dg, db);
        T.diffh[i] = ((dr * 3 + dg * 5 + db * 7) & 63) << 2;
    }
    for (i = 0; i < 1024; i++) {
        int vg = (i >> 4) - 32, half = i & 15;
        T.lumagb[i] = B3(0, vg, vg - 8 + half);
        T.lumar[i] = B3(vg - 8 + half, 0, 0);
        T.lumahgb[i] = ((vg * 5 + (vg - 8 + half) * 7) & 63) << 2;
        T.lumahr[i] = (((vg - 8 + half) * 3) & 63) << 2;
    }
    for (i = 0; i < 256; i++) {
        T.h3[i] = ((i * 3) & 63) << 2; T.h5[i] = ((i * 5) & 63) << 2;
        T.h7[i] = ((i * 7) & 63) << 2; T.h11[i] = ((i * 11) & 63) << 2;
    }
    tables = 1;
}

/* keep at least 5 unread bytes in buf while the file has them; past its
 * end the bytes read as 0, as the reference decoder's padding would */
static void refill(QoiDec *d)
{
    UBYTE *s = d->p, *t = d->buf;
    LONG n;
    while (s < d->end) *t++ = *s++;
    d->p = d->buf;
    d->end = t;
    while (!d->eof && d->end - d->buf < 5) {
        n = d->read(d->ctx, d->end, QOI_INBUF - (LONG)(d->end - d->buf));
        if (n <= 0) d->eof = 1;
        else d->end += n;
    }
    for (s = d->end; s < d->end + 5; s++) *s = 0;
}

int qd_header(QoiDec *d, const UBYTE *h, ULONG filesize, QoiRead read, void *ctx)
{
    int i;
    if (!tables) maketables();
    if (h[0] != 'q' || h[1] != 'o' || h[2] != 'i' || h[3] != 'f') return 0;
    d->width = ((ULONG)h[4] << 24) | ((ULONG)h[5] << 16) | ((ULONG)h[6] << 8) | h[7];
    d->height = ((ULONG)h[8] << 24) | ((ULONG)h[9] << 16) | ((ULONG)h[10] << 8) | h[11];
    d->channels = h[12];
    d->colorspace = h[13];
    if (!d->width || !d->height || d->channels < 3 || d->channels > 4 || d->colorspace > 1 ||
        d->height >= MAXPIXELS / d->width || filesize < QOI_HEADER + QOI_PADDING)
        return 0;
    for (i = 0; i < 64; i++) d->index[i] = 0;
    d->px = 0x000000ffUL;
    d->h = ((255 * 11) & 63) << 2;
    d->run = 0;
    d->alphaand = 255;
    d->chunks = filesize - QOI_HEADER - QOI_PADDING;
    d->read = read;
    d->ctx = ctx;
    d->eof = 0;
    d->shortfile = 0;
    d->p = d->end = d->base = d->lim = d->buf;
    return 1;
}

/* one copy of the loop for each layout, so the layout is not tested per pixel */
static inline __attribute__((always_inline)) void decode(QoiDec *d, UBYTE *out, ULONG n, const int fmt)
{
    ULONG px = d->px, run = d->run, chunks = d->chunks, alpha = d->alphaand;
    ULONG *index = d->index;
    UBYTE *p = d->p, *lim = d->lim, b1, h = d->h;   /* h: the place times 4, wrapping as a byte */

    while (n) {
        if (run) {
            ULONG k = run < n ? run : n;
            run -= k;
            n -= k;
            if (fmt == QD_RGBA) {
                ULONG *o = (ULONG *)out;
                while (k--) *o++ = px;
                out = (UBYTE *)o;
            } else if (fmt == QD_ARGB) {
                ULONG v = (px >> 8) | (px << 24), *o = (ULONG *)out;
                while (k--) *o++ = v;
                out = (UBYTE *)o;
            } else {
                while (k--) { out[0] = px >> 24; out[1] = px >> 16; out[2] = px >> 8; out += 3; }
            }
            continue;
        }
        if (p >= lim) {                 /* the one check in the loop: see below */
            ULONG used = p - d->base, avail;
            chunks = chunks > used ? chunks - used : 0;
            d->p = p;
            refill(d);
            p = d->p;
            if (p >= d->end) chunks = 0;    /* the file was shorter than it said */
            if (!chunks) {              /* the data is used up: the last pixel again, as qoi.h */
                d->shortfile = 1;
                run = n;
                d->base = lim = p;
                continue;
            }
            /* ops may start while there is pixel data left (chunks) and
             * while the 5 bytes the longest op reads are in buf */
            avail = d->end - p > 5 ? d->end - p - 4 : 1;
            lim = p + (avail < chunks ? avail : chunks);
            d->base = p;
        }
#ifdef QD_ASM
        if (qd_use020 && fmt != QD_ARGB) {  /* ops until lim or n: fast020.s */
            d->px = px; d->run = run; d->alphaand = alpha; d->p = p; d->lim = lim; d->h = h;
            n = qd_fast020(d, &out, n, fmt == QD_RGB);
            px = d->px; run = d->run; alpha = d->alphaand; p = d->p; h = d->h;
            continue;
        }
#endif
        b1 = *p++;
        if (b1 < 0x40) {                /* QOI_OP_INDEX */
            px = index[b1];
            alpha &= px;
            h = px ? b1 << 2 : 0;       /* an empty slot holds 0, whose place is 0 */
        } else if (b1 < 0x80) {         /* QOI_OP_DIFF */
            px = swaradd(px, T.difftab[b1 & 63]);
            h += T.diffh[b1 & 63];
        } else if (b1 < 0xc0) {         /* QOI_OP_LUMA */
            UBYTE b2 = *p++;
            UWORD g = (b1 & 63) << 4, lo = g | (b2 & 15), hi = g | (b2 >> 4);
            px = swaradd(px, T.lumagb[lo] | T.lumar[hi]);
            h += T.lumahgb[lo] + T.lumahr[hi];
        } else if (b1 < 0xfe) {         /* QOI_OP_RUN */
            run = (b1 & 63) + 1;
            index[h >> 2] = px;
            continue;
        } else if (b1 == 0xfe) {        /* QOI_OP_RGB */
            px = ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | (px & 255);
            h = T.h3[p[0]] + T.h5[p[1]] + T.h7[p[2]] + T.h11[px & 255];
            p += 3;
        } else {                        /* QOI_OP_RGBA */
            px = ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
            h = T.h3[p[0]] + T.h5[p[1]] + T.h7[p[2]] + T.h11[p[3]];
            alpha &= px;
            p += 4;
        }
        index[h >> 2] = px;
        if (fmt == QD_RGBA) {
            *(ULONG *)out = px; out += 4;
        } else if (fmt == QD_ARGB) {
            *(ULONG *)out = (px >> 8) | (px << 24); out += 4;
        } else {
            out[0] = px >> 24; out[1] = px >> 16; out[2] = px >> 8; out += 3;
        }
        n--;
    }
    d->px = px;
    d->run = run;
    d->chunks = chunks;
    d->alphaand = alpha;
    d->h = h;
    d->lim = lim;
    d->p = p;
}

void qd_pixels(QoiDec *d, UBYTE *out, ULONG n, int fmt)
{
    if (fmt == QD_RGBA) decode(d, out, n, QD_RGBA);
    else if (fmt == QD_ARGB) decode(d, out, n, QD_ARGB);
    else decode(d, out, n, QD_RGB);
}
