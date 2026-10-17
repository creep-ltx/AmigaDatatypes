/* conv: FLAC blocks into 8-bit sound. See conv.h. */
#include "conv.h"

ULONG cv_init(Conv *c, const FlacDec *d, ULONG samples, int stereo)
{
    ULONG n, sh = d->bps > 8 ? d->bps - 8 : 0;
    c->ch = d->channels;
    c->outch = stereo && c->ch >= 2 ? 2 : 1;
    c->up = d->bps < 8 ? 8 - d->bps : 0;
    for (c->step = 1; d->rate / c->step > PAULAMAX; c->step++) ;
    n = c->step * (c->outch == 2 ? 1 : c->ch);      /* file samples summed into one */
    c->div = 0;
    for (c->shift = sh; n > 1 && !(n & 1); n >>= 1) c->shift++;
    if (n > 1) {                                    /* not a power of two */
        c->div = c->step * (c->outch == 2 ? 1 : c->ch);
        c->shift = sh;
    }
    c->len = samples / c->step;
    c->pos = c->cnt = c->in = 0;
    c->acc[0] = c->acc[1] = 0;
    return d->rate / c->step;
}

static BYTE to8(LONG a, ULONG shift, ULONG div)
{
    if (div) a /= (LONG)div;
    if (shift) a = (a + (1L << (shift - 1))) >> shift;
    if (a > 127) a = 127;
    else if (a < -128) a = -128;
    return (BYTE)a;
}

/* the values in locals: the compiler keeps them in registers */
void cv_block(Conv *c, const FlacDec *d)
{
    ULONG n = d->fsamples, room, cnt, step = c->step, ch = c->ch, shift = c->shift, div = c->div, k, i;
    const LONG *s0 = d->pcm[0], *s1 = d->pcm[1];
    BYTE *o0, *o1;
    LONG a0, a1;

    if (d->fstart < c->in) return;                  /* already had it */
    if (d->fstart > c->in) {                        /* frames lost: silence there */
        c->in = d->fstart;
        c->pos = c->in / step;
        c->cnt = c->in % step;
        c->acc[0] = c->acc[1] = 0;
        if (c->pos >= c->len) { c->pos = c->len; return; }
    }
    c->in += n;
    room = c->len - c->pos;
    cnt = c->cnt;
    if (n > room * step - cnt) n = room * step - cnt;      /* no more than there is room for */
    if (c->up)
        for (k = 0; k < ch; k++)
            for (i = 0; i < n; i++) d->pcm[k][i] <<= c->up;
    o0 = c->out[0] + c->pos;
    o1 = c->outch == 2 ? c->out[1] + c->pos : 0;
    a0 = c->acc[0];
    a1 = c->acc[1];
    if (c->outch == 2) {
        for (i = 0; i < n; i++) {
            a0 += s0[i];
            a1 += s1[i];
            if (++cnt == step) {
                *o0++ = to8(a0, shift, div);
                *o1++ = to8(a1, shift, div);
                a0 = a1 = cnt = 0;
            }
        }
    } else if (ch == 1 && step == 1 && !div) {      /* the plain case */
        LONG r = shift ? 1L << (shift - 1) : 0;
        for (i = 0; i < n; i++) {
            LONG v = (s0[i] + r) >> shift;
            *o0++ = v > 127 ? 127 : v < -128 ? -128 : v;
        }
    } else {
        for (i = 0; i < n; i++) {
            if (ch == 1) a0 += s0[i];
            else if (ch == 2) a0 += s0[i] + s1[i];
            else for (k = 0; k < ch; k++) a0 += d->pcm[k][i];
            if (++cnt == step) {
                *o0++ = to8(a0, shift, div);
                a0 = cnt = 0;
            }
        }
    }
    c->pos = o0 - c->out[0];
    c->cnt = cnt;
    c->acc[0] = a0;
    c->acc[1] = a1;
}
