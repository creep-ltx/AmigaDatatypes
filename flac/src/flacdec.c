/* flacdec: a FLAC decoder, a frame at a time. See flacdec.h.
 *
 * Made for a 68000: no 32x32 multiply where 16x16 will do. The predictor's
 * coefficients have at most 15 bits; samples of up to 16 bits (17 for a
 * side channel) are multiplied with muls.w, and the sum is kept in 32 bits
 * when the coefficients cannot carry it further, in 64 otherwise. */
#include "flacdec.h"

#ifdef FL_BENCH
int fl_bench;                           /* tests/bench.c: 1 no CRC, 2 no predictors either */
#define BENCH(n) (fl_bench >= (n))
#else
#define BENCH(n) 0
#endif

static UBYTE crc8tab[256];
static UWORD crc16tab[256];
UBYTE fl_clz8[256];                     /* zero bits above the first 1 in a byte (rice.s too) */

void fl_init(void)
{
    ULONG i, k, c;
    for (i = 0; i < 256; i++) {
        for (c = i, k = 0; k < 8; k++) c = c & 0x80 ? (c << 1) ^ 0x07 : c << 1;
        crc8tab[i] = (UBYTE)c;
        for (c = i << 8, k = 0; k < 8; k++) c = c & 0x8000 ? (c << 1) ^ 0x8005 : c << 1;
        crc16tab[i] = (UWORD)c;
        for (k = 0; k < 8 && !(i & (0x80 >> k)); k++) ;
        fl_clz8[i] = (UBYTE)k;
    }
}

/* ---- metadata -------------------------------------------------------- */

ULONG fl_blockheader(const UBYTE *b, ULONG *len, int *last)
{
    *last = b[0] >> 7;
    *len = (ULONG)b[1] << 16 | (ULONG)b[2] << 8 | b[3];
    return b[0] & 0x7f;
}

int fl_streaminfo(FlacDec *d, const UBYTE *b)
{
    ULONG hi;
    d->minblock = (ULONG)b[0] << 8 | b[1];
    d->maxblock = (ULONG)b[2] << 8 | b[3];
    d->maxframe = (ULONG)b[7] << 16 | (ULONG)b[8] << 8 | b[9];
    d->rate = (ULONG)b[10] << 12 | (ULONG)b[11] << 4 | b[12] >> 4;
    d->channels = ((b[12] >> 1) & 7) + 1;
    d->bps = ((b[12] & 1) << 4 | b[13] >> 4) + 1;
    hi = b[13] & 15;                    /* the length's top 4 of 36 bits */
    d->samples = (ULONG)b[14] << 24 | (ULONG)b[15] << 16 | (ULONG)b[16] << 8 | b[17];
    if (hi) d->samples = 0xffffffff;    /* more than any Amiga holds: the caller's limit cuts it */
    d->fixed = d->minblock == d->maxblock ? d->maxblock : 0;
    return d->rate && d->bps >= 4 && d->bps <= 24 && d->maxblock >= 1 && d->minblock <= d->maxblock;
}

ULONG fl_framemax(const FlacDec *d)
{
    ULONG v = 16 + 2 + 64 + d->channels * (2 + (d->maxblock * (d->bps + 1) + 7) / 8);
    return d->maxframe > v && d->maxframe < 0x1000000 ? d->maxframe + 64 : v;
}

static void latin1(const UBYTE *s, ULONG n, char *out, ULONG size)
{
    ULONG o = 0;
    while (n && o + 1 < size) {
        ULONG c = *s++;
        n--;
        if (c >= 0x80) {
            if ((c == 0xc2 || c == 0xc3) && n && (*s & 0xc0) == 0x80) {
                c = (c & 0x1f) << 6 | (*s++ & 0x3f);
                n--;
            } else {
                while (n && (*s & 0xc0) == 0x80) s++, n--;     /* not in Latin-1 */
                c = '?';
            }
        }
        if (c < 0x20 || (c >= 0x7f && c < 0xa0)) c = ' ';
        out[o++] = (char)c;
    }
    out[o] = 0;
}

static ULONG le32(const UBYTE *b)
{
    return (ULONG)b[3] << 24 | (ULONG)b[2] << 16 | (ULONG)b[1] << 8 | b[0];
}

static int key(const UBYTE *s, ULONG n, const char *k)
{
    for (; *k; k++, s++, n--)
        if (!n || (*s & 0xdf) != *k) return 0;
    return n && *s == '=';
}

void fl_comment(const UBYTE *b, ULONG len, char *title, char *artist, ULONG size)
{
    ULONG pos, n, count;
    title[0] = artist[0] = 0;
    if (len < 8 || (n = le32(b)) > len - 8) return;
    pos = 4 + n;
    count = le32(b + pos);
    pos += 4;
    while (count-- && len - pos >= 4) {
        n = le32(b + pos);
        pos += 4;
        if (n > len - pos) return;
        if (!title[0] && key(b + pos, n, "TITLE")) latin1(b + pos + 6, n - 6, title, size);
        else if (!artist[0] && key(b + pos, n, "ARTIST")) latin1(b + pos + 7, n - 7, artist, size);
        pos += n;
    }
}

/* ---- the frame header ------------------------------------------------ */

typedef struct {
    ULONG block, assign, start;
} Hdr;

static const ULONG rates[12] = { 0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };
static const UBYTE sizes[8] = { 0, 8, 12, 0, 16, 20, 24, 32 };

/* the header at b: its length, 0 if it is not one of this stream's */
static ULONG header(const FlacDec *d, const UBYTE *b, ULONG len, Hdr *h)
{
    ULONG i, k, x, num, bc, rc, crc;
    if (len < 6 || b[0] != 0xff || (b[1] & 0xfe) != 0xf8 || (b[3] & 1)) return 0;
    bc = b[2] >> 4;
    rc = b[2] & 15;
    h->assign = b[3] >> 4;
    k = (b[3] >> 1) & 7;
    if (h->assign >= 11 || (h->assign >= 8 ? 2 : h->assign + 1) != d->channels) return 0;
    if (k ? sizes[k] != d->bps : 0) return 0;
    if (rc == 15) return 0;

    /* the frame or sample number, coded as in UTF-8 */
    x = b[4];
    if (x < 0x80) k = 0;
    else if (x < 0xc0 || x == 0xff) return 0;
    else for (k = 1; x & (0x40 >> k); k++) ;
    if (k > (b[1] & 1 ? 6U : 5U) || 5 + k > len) return 0;
    num = k ? x & (0x3f >> k) : x;
    for (i = 5; i < 5 + k; i++) {
        if ((b[i] & 0xc0) != 0x80 || num >> 26) return 0;
        num = num << 6 | (b[i] & 0x3f);
    }

    if (bc == 0) return 0;
    else if (bc == 1) h->block = 192;
    else if (bc <= 5) h->block = 576UL << (bc - 2);
    else if (bc == 6) {
        if (i >= len) return 0;
        h->block = b[i++] + 1;
    } else if (bc == 7) {
        if (i + 1 >= len) return 0;
        h->block = ((ULONG)b[i] << 8 | b[i + 1]) + 1;
        i += 2;
    } else h->block = 256UL << (bc - 8);
    if (h->block > d->maxblock) return 0;

    if (rc >= 12) {
        if (i + (rc > 12) >= len) return 0;
        x = rc == 12 ? b[i] * 1000UL : ((ULONG)b[i] << 8 | b[i + 1]) * (rc == 14 ? 10 : 1);
        i += rc == 12 ? 1 : 2;
    } else x = rates[rc];
    if (x && x != d->rate) return 0;

    if (i >= len) return 0;
    for (crc = 0, k = 0; k < i; k++) crc = crc8tab[crc ^ b[k]];
    if (crc != b[i]) return 0;

    if (b[1] & 1) h->start = num;       /* variable blocks: the sample's number */
    else {                              /* fixed blocks: the frame's */
        ULONG bs = d->fixed ? d->fixed : num ? d->maxblock : h->block;
        if (num > 0xffffffffUL / bs) return 0;
        h->start = num * bs;
    }
    return i + 1;
}

ULONG fl_sync(const FlacDec *d, const UBYTE *b, ULONG len)
{
    ULONG i;
    Hdr h;
    for (i = 1; i + 1 < len; i++)
        if (b[i] == 0xff && (b[i + 1] & 0xfe) == 0xf8 && header(d, b + i, len - i, &h)) return i;
    return len;
}

/* ---- bits ------------------------------------------------------------ */

/* at least 25 valid bits in c; past the end only zeros come */
#define FILL(p, end, c, n) \
    while (n <= 24) { c |= (ULONG)(p < end ? *p : 0) << (24 - n); p++; n += 8; }

static ULONG bits(FlacDec *d, ULONG k)             /* 0 to 32 bits */
{
    ULONG v = 0, t;
    while (k) {
        t = k > 16 ? 16 : k;
        FILL(d->p, d->end, d->c, d->n);
        v = v << t | d->c >> (32 - t);
        d->c <<= t;
        d->n -= t;
        k -= t;
    }
    return v;
}

static LONG sbits(FlacDec *d, ULONG k)
{
    ULONG v;
    if (!k) return 0;
    v = bits(d, k);
    return k < 32 ? (LONG)(v << (32 - k)) >> (32 - k) : (LONG)v;
}

#define PAST(d) ((d)->p > (d)->end + 4)

static int unary(FlacDec *d, ULONG *q)              /* zeros up to a 1 */
{
    ULONG z = 0;
    while (!bits(d, 1)) {
        if (PAST(d)) return 0;
        z++;
    }
    *q = z;
    return 1;
}

#ifdef FL_ASM
int fl_useasm = 1;
int fl_rice(FlacDec *d, LONG *r, ULONG cnt, ULONG k);  /* rice.s */
#endif

/* CNT Rice-coded values with parameter K into r: the loop that takes the
 * time, its reader in locals (and rice.s on the Amiga) */
static int rice(FlacDec *d, LONG *r, ULONG cnt, ULONG k)
{
    const UBYTE *p = d->p, *end = d->end;
    ULONG c = d->c, q, z, u;
    LONG n = d->n;

#ifdef FL_ASM
    if (k <= 24 && fl_useasm) return fl_rice(d, r, cnt, k);
#endif
    if (k > 24) {                       /* never from a sensible encoder */
        while (cnt--) {
            if (!unary(d, &q)) return 0;
            u = q << k | bits(d, k);
            *r++ = (LONG)(u >> 1) ^ -(LONG)(u & 1);
        }
        return 1;
    }
    while (cnt--) {
        FILL(p, end, c, n);
        q = 0;
        while (!c) {                    /* all of the n bits are zero */
            q += n;
            n = 0;
            if (p > end + 4) goto bad;
            FILL(p, end, c, n);
        }
        if (c >= 0x1000000) z = fl_clz8[c >> 24];
        else if (c >= 0x10000) z = 8 + fl_clz8[c >> 16];
        else if (c >= 0x100) z = 16 + fl_clz8[c >> 8];
        else z = 24 + fl_clz8[c];
        q += z;
        c = c << z << 1;
        n -= z + 1;
        if (k) {
            if (n < (LONG)k) FILL(p, end, c, n);
            u = q << k | c >> (32 - k);
            c <<= k;
            n -= k;
        } else u = q;
        *r++ = (LONG)(u >> 1) ^ -(LONG)(u & 1);
    }
    d->p = p; d->c = c; d->n = n;
    return 1;
bad:
    d->p = p; d->c = c; d->n = n;
    return 0;
}

/* the residual after ORDER warm-up samples, into r[order..block-1] */
static int residual(FlacDec *d, LONG *r, ULONG block, ULONG order)
{
    ULONG method = bits(d, 2), pb, esc, po, parts, size, k, i, cnt, prm;
    if (method > 1) return 0;
    pb = method ? 5 : 4;
    esc = method ? 31 : 15;
    po = bits(d, 4);
    parts = 1UL << po;
    size = block >> po;
    if (size << po != block || size < order) return 0;
    r += order;
    for (k = 0; k < parts; k++) {
        cnt = k ? size : size - order;
        prm = bits(d, pb);
        if (prm == esc) {               /* plain numbers of so many bits */
            prm = bits(d, 5);
            for (i = 0; i < cnt; i++) *r++ = sbits(d, prm);
        } else {
            if (!rice(d, r, cnt, prm)) return 0;
            r += cnt;
        }
        if (PAST(d)) return 0;
    }
    return 1;
}

/* ---- predictors ------------------------------------------------------ */

static void fixed(LONG *s, ULONG n, ULONG order)
{
    ULONG i;
    switch (order) {
    case 1: for (i = 1; i < n; i++) s[i] += s[i - 1]; break;
    case 2: for (i = 2; i < n; i++) s[i] += 2 * s[i - 1] - s[i - 2]; break;
    case 3: for (i = 3; i < n; i++) s[i] += 3 * (s[i - 1] - s[i - 2]) + s[i - 3]; break;
    case 4: for (i = 4; i < n; i++) s[i] += 4 * (s[i - 1] + s[i - 3]) - 6 * s[i - 2] - s[i - 4]; break;
    }
}

/* s[i] += (sum of cf[j] * s[i-1-j]) >> sh, four ways */

/* up to 16 bits a sample, the sum in 32: the usual case. Orders 1-12 (all
 * that flac -8 uses) written out: on a 68020 the loop around a few taps
 * cost as much as the multiplies */
#define TAP(k) (LONG)cf[k] * (WORD)h[-1 - (LONG)(k)]
#define CASE(n, sum) case n: for (; h < e; h++) *h += (sum) >> sh; return;
static void lpc16(LONG *s, ULONG n, const WORD *cf, ULONG order, ULONG sh)
{
    LONG *h = s + order, *e = s + n;
    ULONG j;
    switch (order) {
    CASE(1, TAP(0))
    CASE(2, TAP(0) + TAP(1))
    CASE(3, TAP(0) + TAP(1) + TAP(2))
    CASE(4, TAP(0) + TAP(1) + TAP(2) + TAP(3))
    CASE(5, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4))
    CASE(6, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5))
    CASE(7, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5) + TAP(6))
    CASE(8, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5) + TAP(6) + TAP(7))
    CASE(9, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5) + TAP(6) + TAP(7) + TAP(8))
    CASE(10, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5) + TAP(6) + TAP(7) + TAP(8) + TAP(9))
    CASE(11, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5) + TAP(6) + TAP(7) + TAP(8) + TAP(9) + TAP(10))
    CASE(12, TAP(0) + TAP(1) + TAP(2) + TAP(3) + TAP(4) + TAP(5) + TAP(6) + TAP(7) + TAP(8) + TAP(9) + TAP(10) + TAP(11))
    }
    for (; h < e; h++) {
        LONG sum = 0;
        for (j = 0; j < order; j++) sum += TAP(j);
        *h += sum >> sh;
    }
}
#undef CASE

/* up to 17 bits (a side channel of 16-bit sound): x = 2 * (x >> 1) + (x & 1),
 * still muls.w; the sum in 32 or 64 bits */
static void lpc17(LONG *s, ULONG n, const WORD *cf, ULONG order, ULONG sh, int wide)
{
    ULONG i, j;
    for (i = order; i < n; i++) {
        const LONG *h = s + i;
        if (!wide) {
            LONG sum = 0;
            for (j = 0; j < order; j++) {
                LONG x = h[-1 - (LONG)j];
                sum += (LONG)cf[j] * (WORD)(x >> 1) * 2 + (x & 1 ? cf[j] : 0);
            }
            s[i] += sum >> sh;
        } else {
            long long sum = 0;
            for (j = 0; j < order; j++) {
                LONG x = h[-1 - (LONG)j];
                sum += (LONG)cf[j] * (WORD)(x >> 1) * 2 + (x & 1 ? cf[j] : 0);
            }
            s[i] += (LONG)(sum >> sh);
        }
    }
}

/* more bits (24-bit sound): x = (x >> 15) * 2^15 + (x & 0x7fff), two muls.w
 * and no 32x32 multiply; exact up to 30 bits. The sum in 32 bits where it
 * fits, in 64 where not (only the low parts' sum needs them). */
static void lpcwide(LONG *s, ULONG n, const WORD *cf, ULONG order, ULONG sh, int wide)
{
    ULONG i, j;
    for (i = order; i < n; i++) {
        const LONG *h = s + i;
        LONG hi = 0;
        if (!wide) {
            LONG lo = 0;
            for (j = 0; j < order; j++) {
                LONG x = h[-1 - (LONG)j];
                hi += (LONG)cf[j] * (WORD)(x >> 15);
                lo += (LONG)cf[j] * (WORD)(x & 0x7fff);
            }
            s[i] += (LONG)(((ULONG)hi << 15) + (ULONG)lo) >> sh;
        } else {
            long long lo = 0;
            for (j = 0; j < order; j++) {
                LONG x = h[-1 - (LONG)j];
                hi += (LONG)cf[j] * (WORD)(x >> 15);
                lo += (LONG)cf[j] * (WORD)(x & 0x7fff);
            }
            s[i] += (LONG)((((long long)hi << 15) + lo) >> sh);
        }
    }
}

/* ---- subframes and frames ------------------------------------------- */

static int subframe(FlacDec *d, LONG *s, ULONG block, ULONG bs)
{
    ULONG t = bits(d, 8), wasted = 0, i, order;
    if (t & 0x80) return 0;
    if (t & 1) {
        if (!unary(d, &wasted) || ++wasted >= bs) return 0;
        bs -= wasted;
    }
    t >>= 1;
    if (t == 0) {                       /* one value throughout */
        LONG v = sbits(d, bs);
        for (i = 0; i < block; i++) s[i] = v;
    } else if (t == 1) {                /* plain samples */
        for (i = 0; i < block; i++) s[i] = sbits(d, bs);
    } else if (t >= 8 && t <= 12) {     /* the fixed predictors */
        order = t - 8;
        if (order > block) return 0;
        for (i = 0; i < order; i++) s[i] = sbits(d, bs);
        if (!residual(d, s, block, order)) return 0;
        if (!BENCH(2)) fixed(s, block, order);
    } else if (t >= 32) {               /* linear prediction, order 1-32 */
        WORD cf[32];
        ULONG prec, sum = 0;
        LONG sh;
        order = t - 31;
        if (order > block) return 0;
        for (i = 0; i < order; i++) s[i] = sbits(d, bs);
        prec = bits(d, 4) + 1;
        if (prec == 16) return 0;
        if ((sh = sbits(d, 5)) < 0) return 0;
        for (i = 0; i < order; i++) {
            cf[i] = (WORD)sbits(d, prec);
            sum += cf[i] < 0 ? -cf[i] : cf[i];
        }
        if (!residual(d, s, block, order)) return 0;
        /* the sum fits 32 bits if sum(|cf|) * 2^(bs-1) does */
        if (BENCH(2)) ;
        else if (bs <= 16 && sum < 1UL << (32 - bs)) lpc16(s, block, cf, order, sh);
        else if (bs <= 17) lpc17(s, block, cf, order, sh, sum >= 1UL << (32 - bs));
        else lpcwide(s, block, cf, order, sh, sum >= 1UL << (32 - bs));
    } else return 0;
    if (PAST(d)) return 0;
    if (wasted)
        for (i = 0; i < block; i++) s[i] = (LONG)((ULONG)s[i] << wasted);
    return 1;
}

/* right = left - side. Written as r[i] = l[i] - r[i], bebbo's gcc 6.5 (-O2,
 * 68000) made it sub.l l,(r)+, that is r - l: the right channel came out
 * upside down. Kept out of line and with the negation spelt out. */
static __attribute__((noinline)) void leftside(const LONG *l, LONG *r, ULONG n)
{
    while (n--) {
        LONG side = *r;
        *r++ = *l++ + -side;
    }
}

ULONG fl_frame(FlacDec *d, const UBYTE *b, ULONG len)
{
    Hdr h;
    ULONG hl = header(d, b, len, &h), k, i, used, crc;
    LONG *l = d->pcm[0], *r = d->pcm[1];
    if (!hl) return 0;
    d->p = b + hl;
    d->end = b + len;
    d->c = 0;
    d->n = 0;
    for (k = 0; k < d->channels; k++) {
        ULONG side = (h.assign == 8 && k == 1) || (h.assign == 9 && k == 0) || (h.assign == 10 && k == 1);
        if (!subframe(d, d->pcm[k], h.block, d->bps + side)) return 0;
    }
    d->c <<= d->n & 7;                  /* to the byte */
    d->n &= ~7;
    used = (ULONG)(d->p - b) - (ULONG)(d->n >> 3);
    if (used + 2 > len) return 0;
    if (BENCH(1)) ;
    else {
        for (crc = 0, i = 0; i < used; i++) crc = (crc << 8 & 0xff00) ^ crc16tab[(crc >> 8) ^ b[i]];
        if (crc != ((ULONG)b[used] << 8 | b[used + 1])) return 0;
    }

    switch (h.assign) {
    case 8:                             /* left, side */
        leftside(l, r, h.block);
        break;
    case 9:                             /* side, right */
        for (i = 0; i < h.block; i++) l[i] += r[i];
        break;
    case 10:                            /* mid, side */
        for (i = 0; i < h.block; i++) {
            LONG m = (LONG)((ULONG)l[i] << 1) | (r[i] & 1), sd = r[i];
            l[i] = (m + sd) >> 1;
            r[i] = (m - sd) >> 1;
        }
        break;
    }
    if (!d->fixed && !(b[1] & 1) && !h.start) d->fixed = h.block;
    d->fstart = h.start;
    d->fsamples = h.block;
    return used + 2;
}
