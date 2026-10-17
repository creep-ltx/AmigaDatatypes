/* ansi.c: PC and Amiga ANSI art read into a page of cells.
 *
 * The rules are Charcoal's reader (CharEdit/CharView, core.go), which was
 * checked against thousands of real files from 16colo.rs: CP437 bytes are
 * glyphs, the cursor wraps at once when the last column is written (as
 * ANSI.SYS), LF goes to the start of the next row, the art ends at the
 * first 0x1A unless a SAUCE record says where, and in an art editor's file
 * (one with a record) 0x00, 0x07, 0x08 and 0x09 are glyphs too. Every CSI
 * number is cut to 9999 and the page to ANSI_MAXROWS, so no file can make
 * it grow without end.
 *
 * An animation that ends on an empty or nearly empty screen is shown at its
 * fullest: before each clear screen the page is counted, and the fullest
 * is kept (as Charcoal does). */
#include "ansi.h"
#ifdef LOOKTRACE
#include <stdio.h>
#endif

const unsigned long ansi_rgb[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAA5500, 0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
};

const unsigned long ansi_amiga_rgb[16] = {
    0x000000, 0xAA0000, 0x00AA00, 0xAAAA00, 0x0000AA, 0xAA00AA, 0x00AAAA, 0xAAAAAA,
    0x555555, 0xFF5555, 0x55FF55, 0xFFFF55, 0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
};

#define NOPARAM  -1
#define MAXPARAM 9999
#define NPARAMS  16
#define SCREEN   25             /* a screen's rows: clears and line inserts work on them */

/* ---- SAUCE --------------------------------------------------------------- */

static int same(const unsigned char *p, const char *s)
{
    while (*s)
        if (*p++ != (unsigned char)*s++) return 0;
    return 1;
}

/* a field, its trailing spaces and NULs gone */
static void field(char *to, const unsigned char *from, int n)
{
    int i;
    for (i = 0; i < n; i++) to[i] = from[i] ? (char)from[i] : ' ';
    for (i = n; i > 0 && to[i - 1] == ' '; i--) ;
    to[i] = 0;
}

long ansi_sauce(const unsigned char *b, long n, AnsiSauce *s)
{
    long end = n, o, i;
    s->present = 0;
    s->title[0] = s->author[0] = s->group[0] = s->font[0] = 0;
    s->datatype = s->filetype = s->flags = s->comments = 0;
    s->tinfo1 = s->tinfo2 = 0;
    if (n >= 128 && same(b + n - 128, "SAUCE00")) {
        o = n - 128;
        s->present = 1;
        field(s->title, b + o + 7, 35);
        field(s->author, b + o + 42, 20);
        field(s->group, b + o + 62, 20);
        s->datatype = b[o + 94];
        s->filetype = b[o + 95];
        s->tinfo1 = b[o + 96] | b[o + 97] << 8;
        s->tinfo2 = b[o + 98] | b[o + 99] << 8;
        s->comments = b[o + 104];
        s->flags = b[o + 105];
        field(s->font, b + o + 106, 22);
        end = o;
        if (s->comments) {
            long c = o - 5 - (long)s->comments * 64;
            if (c >= 0 && same(b + c, "COMNT")) end = c;
            else {
                /* Moebius writes UTF-8 comment lines, longer than 64 bytes:
                 * the block is somewhere before, look back for it */
                long from = o - 5 - (long)s->comments * 64 * 4;
                if (from < 0) from = 0;
                for (c = o - (long)s->comments * 64 - 5; c >= from; c--)
                    if (same(b + c, "COMNT")) {
                        end = c;
                        break;
                    }
            }
        }
        /* the EOF byte(s) and padding before the record are not art */
        while (end > 0 && (b[end - 1] == 0 || b[end - 1] == 0x1A)) end--;
    } else {
        for (i = 0; i < n; i++)
            if (b[i] == 0x1A) {
                end = i;
                break;
            }
    }
    return end;
}

int ansi_sauce_pc(const AnsiSauce *s)
{
    if (!s->present || s->datatype != 1 || s->filetype > 2) return 0;
    return !same((const unsigned char *)s->font, "Amiga");
}

/* ---- the terminal ---------------------------------------------------------- */

typedef struct {
    AnsiPage *p;
    long cap;                   /* rows allocated */
    long maxrows;
    AnsiAlloc alloc;
    AnsiFree release;
    int failed;
    int x, y, sx, sy;
    int fg, bg, bold, blink, reverse, conceal, ital, under;
    int glyphctl;               /* 0x00 0x07 0x08 0x09 are glyphs */
    int amimode;                /* the Amiga's rules (ANSI_AMIGA) */
    int state;                  /* 0 text, 1 after ESC, 2 in CSI, 3 skip one */
    int screens;                /* clear screens seen */
    int params[NPARAMS], np, cur, priv, inter, bad;
    unsigned char a;            /* attr(), worked out when the colours change */
    unsigned char s;            /* the style: ANSI_ITALIC | ANSI_UNDERLINE */
    long cy;                    /* the row rc/ra/rs point at, -1 none */
    unsigned char *rc, *ra, *rs;
    AnsiPage best;              /* the fullest page before a clear (rows 0: none) */
    long bestink;
} Term;

/* y * cols on a 68000 without a 32-bit multiply call: both fit 16 bits */
#define ROWOFF(y, cols) ((unsigned long)(unsigned short)(y) * (unsigned short)(cols))

static void fillcells(unsigned char *ch, unsigned char *at, long n, unsigned char c, unsigned char a)
{
    while (n-- > 0) {
        *ch++ = c;
        *at++ = a;
    }
}

static void zero(unsigned char *p, long n)
{
    while (n-- > 0) *p++ = 0;
}

static void copybytes(unsigned char *to, const unsigned char *from, long n)
{
    if (to < from)
        while (n-- > 0) *to++ = *from++;
    else {
        to += n;
        from += n;
        while (n-- > 0) *--to = *--from;
    }
}

/* n cells from cell i blank: character c, colour a, no style */
static void blanks(AnsiPage *p, long i, long n, unsigned char c, unsigned char a)
{
    fillcells(p->ch + i, p->at + i, n, c, a);
    if (p->st) zero(p->st + i, n);
}

/* n cells moved from cell from to cell to */
static void movecells(AnsiPage *p, long to, long from, long n)
{
    copybytes(p->ch + to, p->ch + from, n);
    copybytes(p->at + to, p->at + from, n);
    if (p->st) copybytes(p->st + to, p->st + from, n);
}

/* make room for n rows; 0 if it cannot be had (past the limit or memory) */
static int ensure(Term *t, long n)
{
    AnsiPage *p = t->p;
    long cap, cols = p->cols;
    unsigned char *ch, *at;
    if (n > t->maxrows) {
        p->cut = 1;
        return 0;
    }
    if (n <= p->rows) return 1;
    if (n > t->cap) {
        cap = t->cap * 2;
        if (cap < n) cap = n;
        if (cap > t->maxrows) cap = t->maxrows;
        ch = t->alloc((unsigned long)cap * cols * 2);
        if (!ch) {
            /* no room to double: just what is needed */
            cap = n;
            ch = t->alloc((unsigned long)cap * cols * 2);
        }
        if (!ch) {
            t->failed = 1;
            p->cut = 1;
            return 0;
        }
        at = ch + cap * cols;
        if (p->st) {
            /* the styles grow too, or are dropped */
            unsigned char *st = t->alloc((unsigned long)cap * cols);
            if (st) copybytes(st, p->st, p->rows * cols);
            t->release(p->st);
            p->st = st;
        }
        if (p->ch) {
            copybytes(ch, p->ch, p->rows * cols);
            copybytes(at, p->at, p->rows * cols);
            t->release(p->ch);
        }
        p->ch = ch;
        p->at = at;
        t->cap = cap;
        t->cy = -1;
    }
    blanks(p, ROWOFF(p->rows, cols), ROWOFF(n - p->rows, cols), ' ', 7);
    p->rows = n;
    return 1;
}

static void set(Term *t, int x, int y, unsigned char c, unsigned char a, unsigned char st)
{
    AnsiPage *p = t->p;
    if (x < 0 || y < 0 || x >= p->cols) return;
    if (y >= p->rows && !ensure(t, y + 1)) return;
    if (y != t->cy) {
        unsigned long o = ROWOFF(y, p->cols);
        t->cy = y;
        t->rc = p->ch + o;
        t->ra = p->at + o;
        t->rs = p->st ? p->st + o : 0;
    }
    t->rc[x] = c;
    t->ra[x] = a;
    if (t->rs) t->rs[x] = st;
}

/* the first italic or underline: room for the styles of every cell (a
 * file without them never needs it) */
static void needstyles(Term *t)
{
    AnsiPage *p = t->p;
    if (p->st || !p->ch) return;
    if ((p->st = t->alloc((unsigned long)t->cap * p->cols))) zero(p->st, (long)t->cap * p->cols);
    t->cy = -1;
}

static unsigned char attr(Term *t)
{
    int f = t->fg, b = t->bg;
    if (t->reverse) {
        f = t->bg;
        b = t->fg;
    }
    if (t->bold) f += 8;
    if (t->blink) b += 8;
    if (t->conceal) f = b & 7;
    return (unsigned char)(f | b << 4);
}

static unsigned char blankattr(Term *t)
{
    return (unsigned char)(7 | (t->reverse ? t->fg : t->bg) << 4);
}

static void resetattrs(Term *t)
{
    t->fg = 7;
    t->bg = 0;
    t->bold = t->blink = t->reverse = t->conceal = t->ital = t->under = 0;
    t->a = 7;
    t->s = 0;
}

static void put(Term *t, unsigned char c)
{
    int cols = t->p->cols;
    if (t->x >= cols) {
        t->x = 0;
        t->y++;
    }
    set(t, t->x, t->y, c, t->a, t->s);
    if (++t->x >= cols) {                       /* wrap at once */
        t->x = 0;
        t->y++;
    }
}

/* the cells with something on them */
static long ink(AnsiPage *p)
{
    long i, n = ROWOFF(p->rows, p->cols), k = 0;
    for (i = 0; i < n; i++)
        if ((p->ch[i] != ' ' && p->ch[i] != 0) || (p->at[i] >> 4) || (p->st && (p->st[i] & ANSI_UNDERLINE)))
            k++;
    return k;
}

/* before a clear screen: keep the page if it is the fullest yet */
static void look(Term *t)
{
    AnsiPage *p = t->p, *b = &t->best;
    long n = ROWOFF(p->rows, p->cols), k = ink(p);
    unsigned char *m;
#ifdef LOOKTRACE
    printf("look %ld %ld\n", k, (long)p->rows);
#endif
    if (k <= t->bestink || !n) return;
    if (!(m = t->alloc(n * (p->st ? 3 : 2)))) return;
    if (b->ch) t->release(b->ch);
    b->ch = m;
    b->at = m + n;
    b->st = p->st ? m + 2 * n : 0;
    copybytes(b->ch, p->ch, n);
    copybytes(b->at, p->at, n);
    if (b->st) copybytes(b->st, p->st, n);
    b->rows = p->rows;
    b->cols = p->cols;
    t->bestink = k;
}

/* a clear screen: the page blank and one screen tall at most, the cursor home */
static void clearall(Term *t, unsigned char a)
{
    AnsiPage *p = t->p;
    look(t);
    t->screens++;
    t->cy = -1;                                 /* the rows below are gone */
    if (p->rows > SCREEN) p->rows = SCREEN;
    blanks(p, 0, ROWOFF(p->rows, p->cols), ' ', a);
    t->x = t->y = 0;
}

static void fill(Term *t, long from, long to)
{
    AnsiPage *p = t->p;
    long all = ROWOFF(p->rows, p->cols);
    if (to > all) to = all;
    if (from < 0) from = 0;
    if (from < to) blanks(p, from, to - from, ' ', blankattr(t));
}

/* characters inserted (n > 0) or deleted (n < 0) at the cursor */
static void shiftrow(Term *t, int n)
{
    AnsiPage *p = t->p;
    long cols = p->cols, row, k;
    if (t->y >= p->rows && !ensure(t, t->y + 1)) return;
    row = ROWOFF(t->y, cols);
    if (t->x >= cols) return;
    if (n > 0) {
        k = n < cols - t->x ? n : cols - t->x;
        movecells(p, row + t->x + k, row + t->x, cols - t->x - k);
        blanks(p, row + t->x, k, ' ', blankattr(t));
    } else {
        k = -n < cols - t->x ? -n : cols - t->x;
        movecells(p, row + t->x, row + t->x + k, cols - t->x - k);
        blanks(p, row + cols - k, k, ' ', blankattr(t));
    }
}

/* lines inserted (n > 0) or deleted at the cursor's row, on its screen */
static void shiftrows(Term *t, int n)
{
    AnsiPage *p = t->p;
    long cols = p->cols, top = t->y, end, k;
    if (n > SCREEN) n = SCREEN;
    if (n < -SCREEN) n = -SCREEN;
    if (!n) return;
    if (top >= p->rows && !ensure(t, top + 1)) return;
    end = p->rows < top + SCREEN ? p->rows : top + SCREEN;
    if (n > 0) {
        long want = p->rows + n < top + SCREEN ? p->rows + n : top + SCREEN;
        ensure(t, want);
        end = p->rows < top + SCREEN ? p->rows : top + SCREEN;
        k = n < end - top ? n : end - top;
        movecells(p, (top + k) * cols, top * cols, (end - top - k) * cols);
        blanks(p, top * cols, k * cols, ' ', 7);
    } else {
        k = -n < end - top ? -n : end - top;
        movecells(p, top * cols, (top + k) * cols, (end - top - k) * cols);
        blanks(p, (end - k) * cols, k * cols, ' ', 7);
    }
}

static long sq(long v) { return v * v; }

static int nearest16(unsigned long rgb)
{
    int i, best = 0;
    long bd = -1, d;
    for (i = 0; i < 16; i++) {
        unsigned long c = ansi_rgb[i];
        d = sq((long)(rgb >> 16 & 255) - (long)(c >> 16 & 255)) +
            sq((long)(rgb >> 8 & 255) - (long)(c >> 8 & 255)) +
            sq((long)(rgb & 255) - (long)(c & 255));
        if (bd < 0 || d < bd) {
            best = i;
            bd = d;
        }
    }
    return best;
}

static unsigned long xterm256(int n)
{
    unsigned long r, g, b;
    if (n < 0 || n > 255) return 0;
    if (n < 16) return ansi_rgb[n];
    if (n < 232) {
        n -= 16;
        r = n / 36;
        g = n / 6 % 6;
        b = n % 6;
        r = r ? 55 + r * 40 : 0;
        g = g ? 55 + g * 40 : 0;
        b = b ? 55 + b * 40 : 0;
        return r << 16 | g << 8 | b;
    }
    g = 8 + (n - 232) * 10;
    return g << 16 | g << 8 | g;
}

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static void sgr(Term *t)
{
    int i, v, c;
    for (i = 0; i < t->np; i++) {
        v = t->params[i];
        if (v == NOPARAM) v = 0;
        if (v == 0) resetattrs(t);
        else if (v == 1) t->bold = 1;
        else if (v == 3) t->ital = 1;
        else if (v == 4) t->under = 1;
        else if (v == 23) t->ital = 0;
        else if (v == 24) t->under = 0;
        else if (v == 5 || v == 6) t->blink = 1;
        else if (v == 7) t->reverse = 1;
        else if (v == 8) t->conceal = 1;
        else if (v == 21 || v == 22) t->bold = 0;
        else if (v == 25) t->blink = 0;
        else if (v == 27) t->reverse = 0;
        else if (v == 28) t->conceal = 0;
        else if (v >= 30 && v <= 37) t->fg = v - 30;
        else if (v == 39) t->fg = 7;
        else if (v >= 40 && v <= 47) t->bg = v - 40;
        else if (v == 49) t->bg = 0;
        else if (v >= 90 && v <= 97) {
            t->fg = v - 90;
            t->bold = 1;
        } else if (v >= 100 && v <= 107) {
            t->bg = v - 100;
            t->blink = 1;
        } else if (v == 38 || v == 48) {
            /* 256 and 24-bit colours, as the nearest of the 16 */
            c = -1;
            if (i + 2 < t->np && t->params[i + 1] == 5) {
                c = nearest16(xterm256(t->params[i + 2]));
                i += 2;
            } else if (i + 4 < t->np && t->params[i + 1] == 2) {
                c = nearest16((unsigned long)clamp255(t->params[i + 2]) << 16 |
                              (unsigned long)clamp255(t->params[i + 3]) << 8 |
                              (unsigned long)clamp255(t->params[i + 4]));
                i += 4;
            } else
                i = t->np;
            if (c >= 0 && v == 38) {
                t->fg = c & 7;
                t->bold = c > 7;
            } else if (c >= 0) {
                t->bg = c & 7;
                t->blink = c > 7;
            }
        }
    }
    t->a = attr(t);
    t->s = (t->ital ? ANSI_ITALIC : 0) | (t->under ? ANSI_UNDERLINE : 0);
    if (t->s) needstyles(t);
}

/* parameter i, or def if it is missing (or 0 where the default is 1) */
static int param(Term *t, int i, int def)
{
    int v;
    if (i >= t->np) return def;
    v = t->params[i];
    if (v == NOPARAM || (v == 0 && def == 1)) return def;
    return v;
}

static int min(int a, int b) { return a < b ? a : b; }
static int max(int a, int b) { return a > b ? a : b; }

static void csi(Term *t, unsigned char f)
{
    AnsiPage *p = t->p;
    int cols = p->cols, m, a, b, x;
    if (t->inter || t->priv) return;            /* ?7h and friends, SP sequences */
    switch (f) {
    case 'A': t->y = max(0, t->y - param(t, 0, 1)); break;
    case 'B': t->y = min(ANSI_MAXROWS + 1, t->y + param(t, 0, 1)); break;
    case 'C': t->x = min(cols - 1, t->x + param(t, 0, 1)); break;
    case 'D': t->x = max(0, min(cols - 1, t->x) - param(t, 0, 1)); break;
    case 'E': t->x = 0; t->y = min(ANSI_MAXROWS + 1, t->y + param(t, 0, 1)); break;
    case 'F': t->x = 0; t->y = max(0, t->y - param(t, 0, 1)); break;
    case 'G': t->x = min(cols - 1, param(t, 0, 1) - 1); break;
    case 'd': t->y = param(t, 0, 1) - 1; break;
    case 'H':
    case 'f':
        t->y = param(t, 0, 1) - 1;
        t->x = min(cols - 1, param(t, 1, 1) - 1);
        break;
    case 'J':
        m = param(t, 0, 0);
        if (m == 2 || m == 3) clearall(t, blankattr(t));
        else if (m == 0) {
            long rows = p->rows > t->y + 1 ? p->rows : t->y + 1;
            fill(t, (long)t->y * cols + t->x, (long)cols * rows);
        } else
            fill(t, 0, (long)t->y * cols + t->x + 1);
        break;
    case 'K':
        m = param(t, 0, 0);
        a = m == 0 ? t->x : 0;
        b = m == 1 ? t->x + 1 : cols;
        for (x = a; x < b; x++) set(t, x, t->y, ' ', blankattr(t), 0);
        break;
    case 'X':
        for (x = t->x; x < min(cols, t->x + param(t, 0, 1)); x++) set(t, x, t->y, ' ', blankattr(t), 0);
        break;
    case '@': shiftrow(t, param(t, 0, 1)); break;
    case 'P': shiftrow(t, -param(t, 0, 1)); break;
    case 'L': shiftrows(t, param(t, 0, 1)); break;
    case 'M': shiftrows(t, -param(t, 0, 1)); break;
    case 's': t->sx = t->x; t->sy = t->y; break;
    case 'u':
        if (t->np == 1 && t->params[0] == NOPARAM) {
            t->x = t->sx;
            t->y = t->sy;
        }
        break;
    case 'm': sgr(t); break;
    }
}

static void startcsi(Term *t)
{
    t->state = 2;
    t->np = 0;
    t->cur = NOPARAM;
    t->priv = t->inter = t->bad = 0;
}

static void csibyte(Term *t, unsigned char c)
{
    if (c >= 0x30 && c <= 0x3F) {
        if (c >= '0' && c <= '9') {
            if (!t->bad) {
                t->cur = t->cur == NOPARAM ? c - '0' : t->cur * 10 + c - '0';
                if (t->cur > MAXPARAM) t->cur = MAXPARAM;
            }
        } else if (c == ';') {
            if (t->np < NPARAMS) t->params[t->np++] = t->bad ? NOPARAM : t->cur;
            t->cur = NOPARAM;
            t->bad = 0;
        } else if (t->np == 0 && t->cur == NOPARAM && !t->bad && c >= '<')
            t->priv = 1;                        /* < = > ? before the numbers */
        else
            t->bad = 1;                         /* ':' and the like: not a number */
        return;
    }
    if (c >= 0x20 && c <= 0x2F) {
        t->inter = 1;
        return;
    }
    t->state = 0;
    if (c >= 0x40 && c <= 0x7E) {
        if (t->np < NPARAMS) t->params[t->np++] = t->bad ? NOPARAM : t->cur;
        csi(t, c);
    } else if (c == 0x1B)
        t->state = 1;
}

/* ESC [ (or the Amiga's 0x9B: len 1) numbers ; numbers final, read in one
 * go (most of a colourful file is these): 1 done and *at moved to the
 * final byte, 0 anything else, for byte() to take one at a time */
static int fastcsi(Term *t, const unsigned char *b, long n, long *at, int len)
{
    long k = *at + len;
    int np = 0, cur = NOPARAM;
    for (; k < n; k++) {
        unsigned char d = b[k];
        if (d >= '0' && d <= '9') {
            cur = cur == NOPARAM ? d - '0' : cur * 10 + d - '0';
            if (cur > MAXPARAM) cur = MAXPARAM;
        } else if (d == ';') {
            if (np < NPARAMS) t->params[np++] = cur;
            cur = NOPARAM;
        } else if (d >= 0x40 && d <= 0x7E) {
            if (np < NPARAMS) t->params[np++] = cur;
            t->np = np;
            t->priv = t->inter = 0;
            csi(t, d);
            *at = k;
            return 1;
        } else
            return 0;
    }
    return 0;
}

static void esc(Term *t, unsigned char c)
{
    t->state = 0;
    switch (c) {
    case '[': startcsi(t); break;
    case 'c': resetattrs(t); clearall(t, 7); break;
    case 'D': t->y++; break;
    case 'E': t->x = 0; t->y++; break;
    case 'M': if (t->y > 0) t->y--; break;
    case '7': t->sx = t->x; t->sy = t->y; break;
    case '8': t->x = t->sx; t->y = t->sy; break;
    case '(': case ')': case '#': case '%': t->state = 3; break;
    }
}

static void byte(Term *t, unsigned char c)
{
    switch (t->state) {
    case 1: esc(t, c); return;
    case 2: csibyte(t, c); return;
    case 3: t->state = 0; return;
    }
    if (c == 0x1B) {
        t->state = 1;
        return;
    }
    if (t->amimode)
        switch (c) {
        case 0x9B: startcsi(t); return;                 /* the Amiga's 8-bit CSI */
        case 0x84: t->y++; return;                      /* IND */
        case 0x85: t->x = 0; t->y++; return;            /* NEL */
        case 0x8D: if (t->y > 0) t->y--; return;        /* RI */
        case 0x0C: clearall(t, 7); return;              /* form feed: clear and home */
        }
    if (t->glyphctl && (c == 0x00 || c == 0x07 || c == 0x08 || c == 0x09)) {
        put(t, c);
        return;
    }
    switch (c) {
    case 0x0A: t->x = 0; t->y++; return;
    case 0x0D: t->x = 0; return;
    case 0x08: if (t->x > 0) t->x--; return;
    case 0x09: t->x = min(t->p->cols - 1, (t->x & ~7) + 8); return;
    case 0x07: case 0x00: return;
    }
    if (t->amimode && (c < 0x20 || (c >= 0x7F && c < 0xA0))) return;     /* no glyphs in Latin-1 */
    put(t, c);
}

/* the rows up to the last one with something on it */
static long usedrows(AnsiPage *p)
{
    long y, x, i;
    for (y = p->rows - 1; y >= 0; y--)
        for (x = 0, i = ROWOFF(y, p->cols); x < p->cols; x++, i++)
            if ((p->ch[i] != ' ' && p->ch[i] != 0) || (p->at[i] >> 4) || (p->st && (p->st[i] & ANSI_UNDERLINE)))
                return y + 1;
    return 0;
}

int ansi_read(const unsigned char *art, long n, const AnsiSauce *s, int mode, long maxrows,
              AnsiAlloc alloc, AnsiFree release, AnsiPage *p)
{
    Term t;
    long i, lines = 0, want, rows, step, next;
    int cols = 80;
    if (s->present && s->datatype == 1 && s->tinfo1 >= 1 && s->tinfo1 <= ANSI_MAXCOLS) cols = s->tinfo1;
    p->cols = cols;
    p->rows = 0;
    p->ch = p->at = p->st = 0;
    p->cut = 0;
    t.p = p;
    t.cap = 0;
    t.maxrows = maxrows < ANSI_MAXROWS ? maxrows : ANSI_MAXROWS;
    if (t.maxrows < 1) t.maxrows = 1;
    t.alloc = alloc;
    t.release = release;
    t.failed = 0;
    t.x = t.y = t.sx = t.sy = 0;
    resetattrs(&t);
    t.amimode = mode == ANSI_AMIGA;
    t.glyphctl = s->present && s->datatype == 1 && s->filetype <= 1 && !t.amimode;
    t.state = 0;
    t.screens = 0;
    t.np = 0;
    t.cy = -1;
    t.best.ch = t.best.at = t.best.st = 0;
    t.best.rows = 0;
    t.bestink = 0;
    p->fullest = 0;
    /* room for the rows the file seems to have, so the page seldom grows */
    for (i = 0; i < n; i++)
        if (art[i] == '\n') lines++;
    want = lines + n / cols / 4 + SCREEN;
    if (want > t.maxrows) want = t.maxrows;
    if (!ensure(&t, want)) return 0;
    p->rows = want < SCREEN ? want : SCREEN;    /* a screen; the rest reserved */
    /* an animation draws and clears a screen again and again: look at the
     * page some 64 times on the way, as Charcoal does (only while it is a
     * screen or two: counting a long page that often would be slow) */
    step = n / 64 > 256 ? n / 64 : 256;
    next = n > 2 * step ? step : n + 1;
    for (i = 0; i < n && !t.failed; i++) {
        unsigned char c = art[i];
        if (i >= next) {                        /* (a code read in one go may pass it) */
            if (p->rows <= 2 * SCREEN) look(&t);
            while (next <= i) next += step;
        }
        /* the common case at once: a character to draw in the current row */
        if (!t.state && c >= 0x20 && c != 0x1B && t.x < cols && t.y == t.cy &&
            (!t.amimode || c < 0x7F || c >= 0xA0)) {
            t.rc[t.x] = c;
            t.ra[t.x] = t.a;
            if (t.rs) t.rs[t.x] = t.s;
            if (++t.x >= cols) {
                t.x = 0;
                t.y++;
            }
        } else if (!t.state && c == 0x1B && i + 2 < n && art[i + 1] == '[' && fastcsi(&t, art, n, &i, 2))
            ;
        else if (!t.state && c == 0x9B && t.amimode && i + 1 < n && fastcsi(&t, art, n, &i, 1))
            ;
        else
            byte(&t, c);
    }
    if (n > 2 * step && p->rows <= 2 * SCREEN) look(&t);
    /* an animation that ended (nearly) empty: its fullest picture instead,
     * as tall as its art */
    if (t.best.ch && ink(p) * 2 < t.bestink) {
        unsigned char *m = t.best.ch;
        long n = ROWOFF(t.best.rows, t.best.cols);
        if (p->ch) t.release(p->ch);
        if (p->st) t.release(p->st);
        p->st = 0;
        if (t.best.st && (p->st = t.alloc(n))) copybytes(p->st, t.best.st, n);
        /* the kept block is the page's now (its styles, inside it, were
         * copied out: the page's styles are a block of their own) */
        p->ch = m;
        p->at = t.best.at;
        p->rows = t.best.rows;
        t.cap = t.best.rows;
        t.best.ch = 0;
        p->fullest = 1;
        t.x = t.y = 0;
    }
    if (t.best.ch) t.release(t.best.ch);
    /* as tall as its art or the rows the cursor went through */
    rows = usedrows(p);
    i = t.x > 0 ? t.y + 1 : t.y;
    if (i > rows) rows = i;
    if (rows < 1) rows = 1;
    if (rows > p->rows) ensure(&t, rows);
    if (rows < p->rows) p->rows = rows;
    if (!p->fullest && s->present && s->datatype == 1 && s->filetype <= 1 && (s->flags & 0x18) &&
        s->tinfo2 > p->rows && s->tinfo2 <= t.maxrows && t.screens <= 1)
        ensure(&t, s->tinfo2);                  /* the record's height (Moebius) */
    if (!p->rows) ensure(&t, 1);
    return p->ch != 0;
}

void ansi_free(AnsiPage *p, AnsiFree release)
{
    if (p->ch) release(p->ch);
    if (p->st) release(p->st);
    p->ch = p->at = p->st = 0;
}
