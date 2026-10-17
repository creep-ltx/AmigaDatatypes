/* conv: 16-bit frames into 8-bit sound. See conv.h. */
#include "conv.h"

ULONG cv_init(Conv *c, const QoaDec *d, int stereo)
{
    ULONG n;
    c->ch = d->channels;
    c->outch = stereo && c->ch >= 2 ? 2 : 1;
    for (c->step = 1; d->rate / c->step > PAULAMAX; c->step++) ;
    n = c->step * (c->outch == 2 ? 1 : c->ch);      /* file samples summed into one */
    c->div = 0;
    for (c->shift = 8; n > 1 && !(n & 1); n >>= 1) c->shift++;
    if (n > 1) c->div = c->step * (c->outch == 2 ? 1 : c->ch);   /* not a power of two */
    c->len = d->samples / c->step;
    c->pos = c->cnt = 0;
    c->acc[0] = c->acc[1] = 0;
    return d->rate / c->step;
}

static BYTE to8(LONG a, ULONG shift, ULONG div)
{
    if (div) a = (a / (LONG)div + 128) >> 8;
    else a = (a + (1L << (shift - 1))) >> shift;
    if (a > 127) a = 127;
    else if (a < -128) a = -128;
    return (BYTE)a;
}

/* the values in locals: the compiler keeps them in registers */
void cv_frame(Conv *c, const WORD *s, ULONG n)
{
    BYTE *o0 = c->out[0] + c->pos, *o1 = c->outch == 2 ? c->out[1] + c->pos : 0;
    ULONG room = c->len - c->pos, cnt = c->cnt, step = c->step, ch = c->ch, shift = c->shift, div = c->div, k;
    LONG a0 = c->acc[0], a1 = c->acc[1];

    if (n > room * step - cnt) n = room * step - cnt;      /* no more than there is room for */
    if (c->outch == 2) {
        for (; n; n--, s += ch) {
            a0 += s[0];
            a1 += s[1];
            if (++cnt == step) {
                *o0++ = to8(a0, shift, div);
                *o1++ = to8(a1, shift, div);
                a0 = a1 = cnt = 0;
            }
        }
    } else if (ch == 1 && step == 1) {                      /* the plain case */
        for (; n; n--) {
            LONG v = (*s++ + 128) >> 8;
            *o0++ = v > 127 ? 127 : v;
        }
    } else {
        for (; n; n--, s += ch) {
            if (ch == 1) a0 += s[0];
            else if (ch == 2) a0 += s[0] + s[1];
            else for (k = 0; k < ch; k++) a0 += s[k];
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
