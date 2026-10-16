/* What md.c and syntax.c (the Markdown viewer's, TextTools/markdown/src)
 * expect from the rest of that program and from a C library. The datatype
 * links no C library (-nostdlib), so the string calls are here; there is
 * no terminal, so tui_box gives ASCII (as tui.library's glue does before a
 * terminal is open). */
#include <exec/types.h>
#include "tui.h"
#include "shim.h"

struct Tui *tui_h;              /* never set: no terminal in a datatype */
int tabsize = 8;

/* tui.library's glue.c */
ULONG tui_nobox(int which)
{
    static const char asc[] = "-|++++>.";
    if (which == BX_DOT) return 0xB7;
    return which >= 0 && which < BX_DOT ? (UBYTE)asc[which] : '?';
}

/* never called (tui_h is NULL), but tui_box's macro names it */
ULONG TuiBox(struct Tui *t, LONG which)
{
    (void)t;
    return tui_nobox(which);
}

/* the Markdown viewer's file.c */
const char *basename_(const char *p)
{
    const char *s = p, *q;
    for (q = p; *q; q++)
        if (*q == '/' || *q == ':') s = q + 1;
    return s;
}

/* valid UTF-8 with at least one character beyond ASCII (file.c) */
int isutf8(const unsigned char *t, long n)
{
    long i = 0;
    int high = 0;
    while (i < n) {
        unsigned char c = t[i];
        int k, m;
        if (c < 0x80) {
            i++;
            continue;
        }
        if (c >= 0xC2 && c <= 0xDF) m = 1;
        else if (c >= 0xE0 && c <= 0xEF) m = 2;
        else if (c >= 0xF0 && c <= 0xF4) m = 3;
        else return 0;
        for (k = 1; k <= m; k++)
            if (i + k >= n || (t[i + k] & 0xC0) != 0x80) return 0;
        high = 1;
        i += m + 1;
    }
    return high;
}

/* A character beyond Latin-1, for the Amiga's Latin-1 fonts: the nearest
 * it has, else '?'. From tui.library's core.c (fallback()). */
static const char latinA[] =
    "AaAaAaCcCcCcCcDd" "DdEeEeEeEeEeGgGg" "GgGgHhHhIiIiIiIi" "IiIiJjKkkLlLlLlL"
    "lLlNnNnNnnNnOoOo" "OoOoRrRrRrSsSsSs" "SsTtTtTtUuUuUuUu" "UuUuWwYyYZzZzZzs";

UBYTE latin1(ULONG c)
{
    if (c == '\t') return ' ';
    if (c < 0x20 || (c >= 0x7F && c < 0xA0)) return '?';
    if (c <= 0xFF) return (UBYTE)c;
    if (c >= 0x100 && c < 0x180) return (UBYTE)latinA[c - 0x100];
    if (c >= 0x2500 && c < 0x2580) {            /* box lines */
        switch (c) {
        case 0x2500: case 0x2501: case 0x2504: case 0x2505: case 0x2508: case 0x2509:
        case 0x254C: case 0x254D: case 0x2550: case 0x2574: case 0x2576: case 0x2578:
        case 0x257A: case 0x257C: case 0x257E:
            return '-';
        case 0x2502: case 0x2503: case 0x2506: case 0x2507: case 0x250A: case 0x250B:
        case 0x254E: case 0x254F: case 0x2551: case 0x2575: case 0x2577: case 0x2579:
        case 0x257B: case 0x257D: case 0x257F:
            return '|';
        case 0x2571: return '/';
        case 0x2572: return '\\';
        case 0x2573: return 'X';
        }
        return '+';
    }
    if (c >= 0x2580 && c < 0x25A0) {            /* blocks and shades */
        if (c == 0x2591) return ':';
        if (c == 0x2580 || c == 0x2594) return '"';
        if (c == 0x2584 || c == 0x2581 || c == 0x2582 || c == 0x2583) return '_';
        return '#';
    }
    if (c >= 0x2800 && c < 0x2900) return c == 0x2800 ? ' ' : ':';  /* braille */
    switch (c) {
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015:
    case 0x2212: case 0x2043:
        return '-';
    case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0x2032:
        return '\'';
    case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0x2033:
        return '"';
    case 0x2039: case 0x2190: case 0x25C0: case 0x25C2: case 0x25C4: case 0x2264:
        return '<';
    case 0x203A: case 0x2192: case 0x25B6: case 0x25B8: case 0x25BA: case 0x2265:
    case 0x279C: case 0x27A4:
        return '>';
    case 0x2191: case 0x25B2: case 0x25B4: return '^';
    case 0x2193: case 0x25BC: case 0x25BE: return 'v';
    case 0x2194: case 0x2195: return '+';
    case 0x2022: case 0x2219: case 0x22C5: case 0x2027: case 0x25E6: return 0xB7;
    case 0x2026: return '.';
    case 0x25CF: case 0x25CB: case 0x25C9: case 0x25EF: return 'o';
    case 0x25A0: case 0x25A1: case 0x25AA: case 0x25AB: return '#';
    case 0x2605: case 0x2606: case 0x2731: case 0x273B: return '*';
    case 0x2713: case 0x2714: case 0x2611: return 'v';
    case 0x2717: case 0x2718: case 0x2612: return 'x';
    case 0x20AC: return 'E';
    case 0x2122: return 'T';
    case 0x2248: return '~';
    case 0x2260: return '#';
    case 0x221E: return '8';
    case 0x2000: case 0x2001: case 0x2002: case 0x2003: case 0x2004: case 0x2005:
    case 0x2006: case 0x2007: case 0x2008: case 0x2009: case 0x200A: case 0x202F:
    case 0x205F: case 0x3000:
        return ' ';
    }
    return '?';
}

/* ---- no C library in here: what gcc, md.c and syntax.c call ------------- */

void *memcpy(void *d, const void *s, unsigned long n)
{
    UBYTE *a = d;
    const UBYTE *b = s;
    while (n--) *a++ = *b++;
    return d;
}

void *memmove(void *d, const void *s, unsigned long n)
{
    UBYTE *a = d;
    const UBYTE *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}

void *memset(void *d, int c, unsigned long n)
{
    UBYTE *a = d;
    while (n--) *a++ = (UBYTE)c;
    return d;
}

int memcmp(const void *x, const void *y, unsigned long n)
{
    const UBYTE *a = x, *b = y;
    for (; n; n--, a++, b++)
        if (*a != *b) return *a - *b;
    return 0;
}

void *memchr(const void *s, int c, unsigned long n)
{
    const UBYTE *p = s;
    for (; n; n--, p++)
        if (*p == (UBYTE)c) return (void *)p;
    return 0;
}

unsigned long strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return p - s;
}

char *strcat(char *d, const char *s)
{
    char *p = d;
    while (*p) p++;
    while ((*p++ = *s++)) ;
    return d;
}

char *strcpy(char *d, const char *s)
{
    char *p = d;
    while ((*p++ = *s++)) ;
    return d;
}

char *strncpy(char *d, const char *s, unsigned long n)
{
    char *p = d;
    while (n && *s) { *p++ = *s++; n--; }
    while (n--) *p++ = 0;
    return d;
}

char *strchr(const char *s, int c)
{
    for (; *s; s++)
        if (*s == (char)c) return (char *)s;
    return c ? 0 : (char *)s;
}

char *strrchr(const char *s, int c)
{
    const char *r = 0;
    for (; *s; s++)
        if (*s == (char)c) r = s;
    return c ? (char *)r : (char *)s;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) a++, b++;
    return (UBYTE)*a - (UBYTE)*b;
}

int strncmp(const char *a, const char *b, unsigned long n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (UBYTE)*a - (UBYTE)*b;
        if (!*a) return 0;
    }
    return 0;
}

char *strstr(const char *h, const char *n)
{
    unsigned long k = strlen(n);
    if (!k) return (char *)h;
    for (; *h; h++)
        if (*h == *n && !strncmp(h, n, k)) return (char *)h;
    return 0;
}
