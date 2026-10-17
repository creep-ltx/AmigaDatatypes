/* qoadec: QOA frames into 16-bit samples. See qoadec.h. */
#include "qoadec.h"

static const WORD dequant[16][8] = {
    {    1,    -1,    3,    -3,    5,    -5,     7,     -7 },
    {    5,    -5,   18,   -18,   32,   -32,    49,    -49 },
    {   16,   -16,   53,   -53,   95,   -95,   147,   -147 },
    {   34,   -34,  113,  -113,  203,  -203,   315,   -315 },
    {   63,   -63,  210,  -210,  378,  -378,   588,   -588 },
    {  104,  -104,  345,  -345,  621,  -621,   966,   -966 },
    {  158,  -158,  528,  -528,  950,  -950,  1477,  -1477 },
    {  228,  -228,  760,  -760, 1368, -1368,  2128,  -2128 },
    {  316,  -316, 1053, -1053, 1895, -1895,  2947,  -2947 },
    {  422,  -422, 1405, -1405, 2529, -2529,  3934,  -3934 },
    {  548,  -548, 1828, -1828, 3290, -3290,  5117,  -5117 },
    {  696,  -696, 2320, -2320, 4176, -4176,  6496,  -6496 },
    {  868,  -868, 2893, -2893, 5207, -5207,  8099,  -8099 },
    { 1064, -1064, 3548, -3548, 6386, -6386,  9933,  -9933 },
    { 1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005 },
    { 1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336 },
};

static ULONG be32(const UBYTE *b)
{
    return ((ULONG)b[0] << 24) | ((ULONG)b[1] << 16) | ((ULONG)b[2] << 8) | b[3];
}

int qa_header(QoaDec *d, const UBYTE *b)
{
    if (b[0] != 'q' || b[1] != 'o' || b[2] != 'a' || b[3] != 'f') return 0;
    d->samples = be32(b + 4);
    d->channels = b[8];
    d->rate = be32(b + 8) & 0xffffff;
    /* no length: a "streaming" file, rate and channels may change per frame */
    return d->samples && d->channels && d->channels <= QOA_MAXCH && d->rate;
}

void qa_limit(QoaDec *d, ULONG size)
{
    ULONG most = (size - 8) / (8 * d->channels) * QOA_SLICE;   /* every byte a slice */
    if (d->samples > most) d->samples = most;
}

ULONG qa_frameheader(QoaDec *d, const UBYTE *h)
{
    ULONG ch = h[0], rate = be32(h) & 0xffffff;
    ULONG n = ((ULONG)h[4] << 8) | h[5], size = ((ULONG)h[6] << 8) | h[7];
    ULONG head = 8 + 16 * ch, slices = (n + QOA_SLICE - 1) / QOA_SLICE;
    if (ch != d->channels || rate != d->rate || !n || size < head || slices > 256 ||
        slices * ch > (size - head) / 8)
        return 0;
    d->fsamples = n;
    d->fsize = size;
    return head + slices * ch * 8;      /* what is decoded; the reference reads on from there too */
}

static WORD s16(const UBYTE *b)
{
    return (WORD)(((UWORD)b[0] << 8) | b[1]);
}

/* up to 10 samples of a slice, as slice.s does them: the residuals at the
 * top of BITS, 3 bits each */
static void run(WORD *o, ULONG bits, ULONG n, ULONG stride, const WORD *tab, QoaLms *l)
{
    LONG h0 = l->h[0], h1 = l->h[1], h2 = l->h[2], h3 = l->h[3];
    LONG w0 = l->w[0], w1 = l->w[1], w2 = l->w[2], w3 = l->w[3];
    ULONG i;
    stride /= sizeof(WORD);
    for (i = 0; i < n; i++) {
        LONG pred, r, v, delta;
        pred = (LONG)((ULONG)w0 * h0 + (ULONG)w1 * h1 + (ULONG)w2 * h2 + (ULONG)w3 * h3);
        r = tab[bits >> 29];
        bits <<= 3;
        v = (pred >> 13) + r;
        if (v > 32767) v = 32767;
        else if (v < -32768) v = -32768;
        *o = v;
        o += stride;
        delta = r >> 4;
        if (h0 < 0) w0 -= delta; else w0 += delta;
        if (h1 < 0) w1 -= delta; else w1 += delta;
        if (h2 < 0) w2 -= delta; else w2 += delta;
        if (h3 < 0) w3 -= delta; else w3 += delta;
        h0 = h1; h1 = h2; h2 = h3; h3 = v;
    }
    l->h[0] = h0; l->h[1] = h1; l->h[2] = h2; l->h[3] = h3;
    l->w[0] = w0; l->w[1] = w1; l->w[2] = w2; l->w[3] = w3;
}

#ifdef QA_ASM
void qa_run(WORD *o, ULONG bits, ULONG n, ULONG stride, const WORD *tab, QoaLms *l);
int qa_useasm = 1;

/* slice.s works the weights as WORDs: only when ten samples (896 at most
 * each) cannot take one outside 16 bits. Nearly always so. */
#define SAFE (32767 - 10 * 896)
static void half(WORD *o, ULONG bits, ULONG n, ULONG stride, const WORD *tab, QoaLms *l)
{
    if (qa_useasm && (ULONG)(l->w[0] + SAFE) <= 2 * SAFE && (ULONG)(l->w[1] + SAFE) <= 2 * SAFE &&
        (ULONG)(l->w[2] + SAFE) <= 2 * SAFE && (ULONG)(l->w[3] + SAFE) <= 2 * SAFE)
        qa_run(o, bits, n, stride, tab, l);
    else
        run(o, bits, n, stride, tab, l);
}
#else
#define half run
#endif

void qa_frame(QoaDec *d, const UBYTE *frame, WORD *out)
{
    ULONG ch = d->channels, n = d->fsamples, si, c, i;
    const UBYTE *p = frame + 8;

    for (c = 0; c < ch; c++, p += 16)
        for (i = 0; i < 4; i++) {
            d->lms[c].h[i] = s16(p + i * 2);
            d->lms[c].w[i] = s16(p + 8 + i * 2);
        }

    for (si = 0; si < n; si += QOA_SLICE) {
        ULONG len = n - si < QOA_SLICE ? n - si : QOA_SLICE;
        WORD *o = out + si * ch;
        for (c = 0; c < ch; c++, p += 8, o++) {
            ULONG hi = be32(p), lo = be32(p + 4);
            const WORD *tab = dequant[hi >> 28];
            /* 4 bits of scalefactor, then residuals 0-9 and 10-19 */
            half(o, (hi << 4) | (lo >> 28), len < 10 ? len : 10, ch * sizeof(WORD), tab, &d->lms[c]);
            if (len > 10)
                half(o + 10 * ch, lo << 2, len - 10, ch * sizeof(WORD), tab, &d->lms[c]);
        }
    }
}
