/* qoienc: see qoienc.h. Follows the reference encoder (qoi.h) step by step,
 * so the bytes come out the same; tests/enccheck.c holds it to that. */
#include "qoienc.h"

#ifdef __amigaos__
#define GETPX(p) (*(const ULONG *)(p))  /* big-endian, and the 68000 takes longs at even addresses */
#else
#define GETPX(p) (((ULONG)(p)[0] << 24) | ((ULONG)(p)[1] << 16) | ((ULONG)(p)[2] << 8) | (p)[3])
#endif

/* the format's hash, (r*3 + g*5 + b*7 + a*11) % 64, as four table lookups:
 * no multiplies, which are slow on a 68000 */
static UBYTE h3[256], h5[256], h7[256], h11[256];

static void flush(QoiEnc *e)
{
    LONG n = e->o - e->buf;
    if (n && !e->failed && !e->write(e->ctx, e->buf, n))
        e->failed = 1;
    e->o = e->buf;
}

static void put32(UBYTE *o, ULONG v)
{
    o[0] = v >> 24; o[1] = v >> 16; o[2] = v >> 8; o[3] = v;
}

void qe_begin(QoiEnc *e, ULONG width, ULONG height, int channels, QoiWrite write, void *ctx)
{
    int i;

    for (i = 0; i < 256; i++) {
        h3[i] = (i * 3) & 63; h5[i] = (i * 5) & 63;
        h7[i] = (i * 7) & 63; h11[i] = (i * 11) & 63;
    }
    for (i = 0; i < 64; i++)
        e->index[i] = 0;
    e->px = 255;                        /* r, g, b 0 and alpha 255 */
    e->run = 0;
    e->left = width * height;
    e->alphaand = 255;
    e->write = write;
    e->ctx = ctx;
    e->failed = 0;
    e->o = e->buf;
    e->o[0] = 'q'; e->o[1] = 'o'; e->o[2] = 'i'; e->o[3] = 'f';
    put32(e->o + 4, width);
    put32(e->o + 8, height);
    e->o[12] = channels;
    e->o[13] = 0;                       /* sRGB with linear alpha */
    e->o += QOI_HEADER;
}

void qe_pixels(QoiEnc *e, const UBYTE *src, ULONG n, int opaque)
{
    ULONG prev = e->px, run = e->run, left = e->left, aand = e->alphaand;
    UBYTE *o = e->o, *lim = e->buf + QOI_OUTBUF - 5;

    if (n > left)
        n = left;
    left -= n;
    while (n--) {
        ULONG px = GETPX(src);
        src += 4;
        if (opaque)
            px |= 255;
        else
            aand &= px;
        if (o > lim) {
            e->o = o;
            flush(e);
            o = e->o;
        }
        if (px == prev) {
            run++;
            if (run == 62 || (n == 0 && left == 0)) {
                *o++ = 0xc0 | (run - 1);
                run = 0;
            }
            continue;
        }
        if (run) {
            *o++ = 0xc0 | (run - 1);
            run = 0;
        }
        {
            UBYTE r = px >> 24, g = px >> 16, b = px >> 8, a = px;
            int h = (h3[r] + h5[g] + h7[b] + h11[a]) & 63;
            if (e->index[h] == px) {
                *o++ = h;
            } else {
                e->index[h] = px;
                if (a == (UBYTE)prev) {
                    signed char vr = r - (UBYTE)(prev >> 24);
                    signed char vg = g - (UBYTE)(prev >> 16);
                    signed char vb = b - (UBYTE)(prev >> 8);
                    signed char vgr = vr - vg, vgb = vb - vg;  /* wraps as in qoi.h */
                    if ((UBYTE)(vr + 2) < 4 && (UBYTE)(vg + 2) < 4 && (UBYTE)(vb + 2) < 4) {
                        *o++ = 0x40 | ((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2);
                    } else if ((UBYTE)(vgr + 8) < 16 && (UBYTE)(vg + 32) < 64 && (UBYTE)(vgb + 8) < 16) {
                        o[0] = 0x80 | (vg + 32);
                        o[1] = ((vgr + 8) << 4) | (vgb + 8);
                        o += 2;
                    } else {
                        o[0] = 0xfe; o[1] = r; o[2] = g; o[3] = b;
                        o += 4;
                    }
                } else {
                    o[0] = 0xff; o[1] = r; o[2] = g; o[3] = b; o[4] = a;
                    o += 5;
                }
            }
        }
        prev = px;
    }
    e->px = prev;
    e->run = run;
    e->left = left;
    e->alphaand = aand & 255;
    e->o = o;
}

int qe_end(QoiEnc *e)
{
    int i;

    if (e->o > e->buf + QOI_OUTBUF - 9)
        flush(e);
    if (e->run) {                       /* only if fewer pixels came than promised */
        *e->o++ = 0xc0 | (e->run - 1);
        e->run = 0;
    }
    for (i = 0; i < 7; i++)
        *e->o++ = 0;
    *e->o++ = 1;
    flush(e);
    return !e->failed;
}
