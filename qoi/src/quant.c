/* quant: see quant.h */
#include "quant.h"

#define C15(v) ((((v) >> 17) & 0x7c00) | (((v) >> 14) & 0x03e0) | (((v) >> 11) & 0x001f))
#define EXHASH(c) ((((c) >> 16) * 7 + ((c) >> 8) * 3 + (c)) & 511)

void qu_init(Quant *q)
{
    ULONG i;
    for (i = 0; i < 32768; i++) q->hist[i] = 0;
    for (i = 0; i < 512; i++) q->invok[i] = 0;
    for (i = 0; i < 512; i++) q->exact[i] = 0xffffffffUL;
    q->nexact = q->overflow = q->ncolors = q->flip = 0;
}

/* x / 255 rounded, for x up to 65280, without a divide: the 68000 has no
 * 32-bit one, and gcc's would be a library call per pixel */
#if QU_BG != 0xaa
#error qu_flatten multiplies by 0xaa in shifts
#endif
/* 16 x 16 -> 32 bits: one mulu.w. gcc calls its library multiply for some
 * of these on the 68000 even when both sides are 16-bit */
#ifdef __mc68000__
static inline ULONG mul16(UWORD a, UWORD b)
{
    ULONG r = a;
    __asm__("mulu.w %1,%0" : "+d"(r) : "dmi"(b));
    return r;
}
#else
#define mul16(a, b) ((ULONG)(a) * (b))
#endif
#define DIV255(x) (((x) + 128 + (((x) + 128) >> 8)) >> 8)

void qu_flatten(ULONG *px, ULONG n)
{
    while (n--) {
        ULONG v = *px;
        UWORD a = v & 255;
        if (!a)                         /* fully transparent: just the grey */
            *px = ((ULONG)QU_BG << 24) | ((ULONG)QU_BG << 16) | ((ULONG)QU_BG << 8) | 255;
        else if (a != 255) {
            UWORD na = 255 - a;
            ULONG bg = ((ULONG)na << 7) + (na << 5) + (na << 3) + (na << 1), r, g, bl;  /* na * 0xaa */
            r = DIV255(mul16(v >> 24, a) + bg);
            g = DIV255(mul16((v >> 16) & 255, a) + bg);
            bl = DIV255(mul16((v >> 8) & 255, a) + bg);
            *px = (r << 24) | (g << 16) | (bl << 8) | 255;
        }
        px++;
    }
}

void qu_count(Quant *q, const ULONG *px, ULONG n)
{
    ULONG last = 0xffffffffUL;
    while (n--) {
        ULONG v = *px++, c = v >> 8, k;
        UWORD *h;
        if (v == last) {                /* runs are common: count them cheaply */
            h = &q->hist[C15(v)];
            if (*h != 0xffff) (*h)++;
            continue;
        }
        last = v;
        h = &q->hist[C15(v)];
        if (*h != 0xffff) (*h)++;
        if (q->overflow) continue;
        for (k = EXHASH(c); q->exact[k] != 0xffffffffUL; k = (k + 1) & 511)
            if (q->exact[k] == c) break;
        if (q->exact[k] == 0xffffffffUL) {
            if (q->nexact == 256) { q->overflow = 1; continue; }
            q->exact[k] = c;
            q->exactix[k] = q->nexact;
            q->pal[q->nexact][0] = c >> 16;
            q->pal[q->nexact][1] = c >> 8;
            q->pal[q->nexact][2] = c;
            q->nexact++;
        }
    }
}

static ULONG sq4[511], sq3[511], sq2[511];  /* sqK[d + 255] = d * d * K, for nearest() */
static UBYTE clamptab[1280];            /* clamptab[512 + v] = v held to 0..255, v from -512 */

/* median cut over the used 5:5:5 colours (cells), as Heckbert: split the
 * box with the most pixels along its longest side at the median pixel */
#define Box QuBox

static int side(const ULONG *cells, struct Box *b, int *axis)
{
    ULONG i;
    int lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 }, k, best = -1;
    for (i = b->first; i < b->first + b->n; i++) {
        int c = cells[i] >> 16, v[3];
        v[0] = (c >> 10) & 31; v[1] = (c >> 5) & 31; v[2] = c & 31;
        for (k = 0; k < 3; k++) {
            if (v[k] < lo[k]) lo[k] = v[k];
            if (v[k] > hi[k]) hi[k] = v[k];
        }
    }
    for (k = 0; k < 3; k++)             /* green counts most to the eye, then red */
        if (hi[k] - lo[k] > best) { best = hi[k] - lo[k]; *axis = k; }
    return best;
}

/* sort a box's cells by one axis in place (32 buckets, swapped into
 * place: no scratch space) */
static void sortbox(ULONG *cells, struct Box *b, int axis)
{
    ULONG start[33], next[32], *c = cells + b->first, i;
    int shift = axis == 0 ? 26 : axis == 1 ? 21 : 16, k;
    for (k = 0; k <= 32; k++) start[k] = 0;
    for (i = 0; i < b->n; i++) start[((c[i] >> shift) & 31) + 1]++;
    for (k = 0; k < 32; k++) { start[k + 1] += start[k]; next[k] = start[k]; }
    for (k = 0; k < 32; k++)
        while (next[k] < start[k + 1]) {
            ULONG v = c[next[k]];
            int kk = (v >> shift) & 31;
            while (kk != k) {
                ULONG t = c[next[kk]];
                c[next[kk]++] = v;
                v = t;
                kk = (v >> shift) & 31;
            }
            c[next[k]++] = v;
        }
}

void qu_palette(Quant *q)
{
    struct Box *box = q->box;
    ULONG i, n = 0;
    int nbox = 1, b, k;
    if (!q->overflow) {                 /* 256 colours or fewer: exactly those */
        q->ncolors = q->nexact;
        return;
    }
    for (i = 0; i < 32768; i++)
        if (q->hist[i]) q->cells[n++] = (i << 16) | q->hist[i];
    box[0].first = 0; box[0].n = n; box[0].pixels = 0;
    for (i = 0; i < n; i++) box[0].pixels += q->cells[i] & 0xffff;
    while (nbox < 256) {
        int axis = 0, pick = -1;
        ULONG most = 0, half, sum, j;
        for (b = 0; b < nbox; b++)
            if (box[b].n > 1 && box[b].pixels > most) { most = box[b].pixels; pick = b; }
        if (pick < 0) break;
        if (side(q->cells, &box[pick], &axis) <= 0) { box[pick].pixels = 0; continue; }
        sortbox(q->cells, &box[pick], axis);
        half = box[pick].pixels / 2;
        for (sum = 0, j = 0; j < box[pick].n - 1; j++) {
            sum += q->cells[box[pick].first + j] & 0xffff;
            if (sum >= half) break;
        }
        box[nbox].first = box[pick].first + j + 1;
        box[nbox].n = box[pick].n - j - 1;
        box[pick].n = j + 1;
        box[nbox].pixels = box[pick].pixels - sum;
        box[pick].pixels = sum;
        nbox++;
    }
    for (b = 0; b < nbox; b++) {        /* each box's colour: the mean of its pixels */
        ULONG s[3] = { 0, 0, 0 }, w = 0;
        for (i = box[b].first; i < box[b].first + box[b].n; i++) {
            ULONG c = q->cells[i] >> 16, cnt = q->cells[i] & 0xffff;
            s[0] += (UWORD)(((c >> 10) & 31) * 8 + 4) * (UWORD)cnt;
            s[1] += (UWORD)(((c >> 5) & 31) * 8 + 4) * (UWORD)cnt;
            s[2] += (UWORD)((c & 31) * 8 + 4) * (UWORD)cnt;
            w += cnt;
        }
        if (!w) w = 1;
        for (k = 0; k < 3; k++) q->pal[b][k] = s[k] / w;
    }
    q->ncolors = nbox;
    for (i = 0; i < 512; i++) q->invok[i] = 0;
    for (i = 0; i < 1280; i++) clamptab[i] = i < 512 ? 0 : i > 767 ? 255 : i - 512;
    for (i = 0; i < 511; i++) {
        UWORD dd = i < 255 ? 255 - i : i - 255;
        ULONG v = (ULONG)dd * dd;
        sq4[i] = v << 2; sq3[i] = v + v + v; sq2[i] = v + v;
    }
    for (b = 0; b < nbox; b++) q->ord[b] = b;      /* by green: insertion sort, 256 at most */
    for (b = 1; b < nbox; b++) {
        UBYTE t = q->ord[b];
        for (k = b; k > 0 && q->pal[q->ord[k - 1]][1] > q->pal[t][1]; k--) q->ord[k] = q->ord[k - 1];
        q->ord[k] = t;
    }
    for (b = 0, k = 0; b < 256; b++) {     /* gstart[g]: the first entry with green >= g */
        while (k < nbox && q->pal[q->ord[k]][1] < b) k++;
        q->gstart[b] = k;
    }
}

/* the nearest palette colour (distance weighted 3:4:2 for red, green,
 * blue). The palette is sorted by green (q->ord, q->gstart says where each
 * green starts); the search goes both ways from the nearest green until
 * green alone is too far, and drops a candidate as soon as part of its
 * distance is. Weighted squares come from tables: no multiply. */
static int nearest(Quant *q, int r, int g, int b)
{
    int lo, hi, best = 0, n = q->ncolors;
    ULONG bestd = 0xffffffffUL;
    const ULONG *s4 = sq4 + 255, *s3 = sq3 + 255, *s2 = sq2 + 255;
    hi = q->gstart[g];
    lo = hi - 1;
    while (lo >= 0 || hi < n) {
        if (hi < n) {
            const UBYTE *c = q->pal[q->ord[hi]];
            ULONG d = s4[c[1] - g];
            if (d >= bestd) hi = n;
            else {
                if ((d += s3[c[0] - r]) < bestd && (d += s2[c[2] - b]) < bestd) { bestd = d; best = q->ord[hi]; }
                hi++;
            }
        }
        if (lo >= 0) {
            const UBYTE *c = q->pal[q->ord[lo]];
            ULONG d = s4[c[1] - g];
            if (d >= bestd) lo = -1;
            else {
                if ((d += s3[c[0] - r]) < bestd && (d += s2[c[2] - b]) < bestd) { bestd = d; best = q->ord[lo]; }
                lo--;
            }
        }
    }
    return best;
}


/* the 4:4:4 colour k's palette entry, found and kept: dither.s calls this
 * when its look-up misses */
int qu_miss(Quant *q, int k)
{
    q->inv[k] = nearest(q, ((k >> 8) << 4) | 8, (k & 0xf0) | 8, ((k & 15) << 4) | 8);
    q->invok[k >> 3] |= 1 << (k & 7);
    return q->inv[k];
}

#ifdef QU_ASM
void qu_dither(const ULONG *px, UBYTE *out, ULONG n, WORD *cur, WORD *nx, Quant *q, const UBYTE *cl);
_Static_assert(__builtin_offsetof(Quant, inv) == 1024 && __builtin_offsetof(Quant, invok) == 5120, "dither.s offsets");
#endif

void qu_map(Quant *q, const ULONG *px, UBYTE *out, ULONG n, WORD *err)
{
    ULONG i;
    if (!q->overflow) {                 /* exact colours: look each up */
        ULONG last = 0xffffffffUL;
        UBYTE ix = 0;
        for (i = 0; i < n; i++) {
            ULONG c = px[i] >> 8, k;
            if (c != last) {
                for (k = EXHASH(c); q->exact[k] != c; k = (k + 1) & 511) ;
                ix = q->exactix[k];
                last = c;
            }
            out[i] = ix;
        }
        return;
    }
    {
        /* Floyd-Steinberg, kept lean for the 68000/020. err holds two rows of
         * n + 2 cells (a spare at each end), used in turn (q->flip): this
         * row's incoming errors and the next row's. A cell of the next row
         * is first set (down right of a pixel), then added to twice: no
         * clearing, no copying. The error going right stays in registers;
         * clamping is a table look-up. */
        WORD *cur = err + (q->flip ? 3 * (n + 2) : 0) + 3, *nx = err + (q->flip ? 0 : 3 * (n + 2)) + 3;
        const UBYTE *cl = clamptab + 512;
        nx[-3] = nx[-2] = nx[-1] = nx[0] = nx[1] = nx[2] = 0;
#ifdef QU_ASM
        qu_dither(px, out, n, cur, nx - 3, q, cl);
#else
        WORD er = 0, eg = 0, eb = 0;
        for (i = 0; i < n; i++, cur += 3, nx += 3) {
            ULONG v = px[i];
            int r = cl[(int)(v >> 24) + cur[0] + er], g = cl[(int)((v >> 16) & 255) + cur[1] + eg],
                b = cl[(int)((v >> 8) & 255) + cur[2] + eb], k = ((r >> 4) << 8) | (g & 0xf0) | (b >> 4), ix;
            WORD e, e3, e5;
            if (!(q->invok[k >> 3] & (1 << (k & 7)))) {
                q->inv[k] = nearest(q, (r & 0xf0) | 8, (g & 0xf0) | 8, (b & 0xf0) | 8);
                q->invok[k >> 3] |= 1 << (k & 7);
            }
            ix = q->inv[k];
            out[i] = ix;
            e = r - q->pal[ix][0]; e3 = e + e + e; e5 = e3 + e + e;
            nx[-3] += e3 >> 4; nx[0] += e5 >> 4; nx[3] = e >> 4; er = (e5 + e + e) >> 4;
            e = g - q->pal[ix][1]; e3 = e + e + e; e5 = e3 + e + e;
            nx[-2] += e3 >> 4; nx[1] += e5 >> 4; nx[4] = e >> 4; eg = (e5 + e + e) >> 4;
            e = b - q->pal[ix][2]; e3 = e + e + e; e5 = e3 + e + e;
            nx[-1] += e3 >> 4; nx[2] += e5 >> 4; nx[5] = e >> 4; eb = (e5 + e + e) >> 4;
        }
#endif
        q->flip ^= 1;
    }
}
