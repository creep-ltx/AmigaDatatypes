/* guess.c: Amiga or PC, for a text art file without a SAUCE record.
 * Charcoal's guessAmiga (core.go), in whole numbers: each byte 0x80-0xFF
 * has a weight learned from 1374 files whose machine was known (above 0
 * Amiga, below 0 PC; here in hundredths), the line ends count a little,
 * and the Amiga's 8-bit CSI decides. 98% right on files it had not seen.
 * Shared by the recognition code (hook.c) and the PC test, as is
 * ansi_claim(), which decides from the name, the record and the bytes. */
#include "guess.h"

static const short weight[128] = {
    -16, 15, 14, 15, 79, 148, 18, 16, 37, 55, 14, 15, 13, 15, 18, 21,
    17, 55, 18, 52, 24, 15, 32, 42, 18, 17, 14, 14, 16, 31, 34, 17,
    224, 315, 87, 28, 63, -6, 276, 29, 113, 179, 22, 363, 288, 98, 127, 473,
    -135, -315, -218, -36, 101, 83, 72, 546, 95, 93, -12, 365, 208, 208, 230, -16,
    -103, 13, 54, 45, -88, 17, 68, 12, -5, 38, 32, 20, 19, -49, 0, 34,
    29, 201, 14, 10, 30, 19, 41, 82, 284, -94, -66, -435, -394, -264, -248, -400,
    84, 16, 28, 16, 55, 72, 78, 31, 14, 17, 26, 31, 17, 21, 48, 59,
    6, 13, 82, 33, 18, 5, 40, 253, 251, 50, -17, 19, 61, -28, -138, -2,
};

static int letter(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* the Amiga's CSI: 0x9B, digits and ;, then m H J or K */
static int amigacsi(const unsigned char *b, long n, long i)
{
    for (i++; i < n && ((b[i] >= '0' && b[i] <= '9') || b[i] == ';'); i++) ;
    return i < n && (b[i] == 'm' || b[i] == 'H' || b[i] == 'J' || b[i] == 'K');
}

long ansi_guess(const unsigned char *b, long n)
{
    long h[129], i, hi = 0, s = 0, lf = 0, crlf = 0, ff = 0;
    int csi = 0, esc = 0;
    for (i = 0; i < 129; i++) h[i] = 0;
    for (i = 0; i < n; i++) {
        unsigned char c = b[i];
        if (c >= 0x80) {
            h[c - 0x80]++;
            hi++;
            if (c == 0x9B && !csi) csi = amigacsi(b, n, i);
            /* a byte 0x80-0x9F between letters is a CP437 letter (ä ö å):
             * on an Amiga it is a control code, never inside a word */
            if (c < 0xA0 && c != 0x9B && i > 0 && i + 1 < n && letter(b[i - 1]) &&
                (letter(b[i + 1]) || b[i + 1] == ' '))
                s -= 200;
        } else if (c == '\n') {
            lf++;
            if (i > 0 && b[i - 1] == '\r') crlf++;
        } else if (c == 0x0C)
            ff++;
        else if (c == 0x1B && i + 1 < n && b[i + 1] == '[')
            esc = 1;
    }
    if (hi > 0) {
        long w = 0;
        for (i = 0; i < 128; i++) w += h[i] * weight[i];
        s += w * (hi < 200 ? hi : 200) / (hi * 20);
    }
    if (lf > 4) s += crlf * 4 < lf ? 50 : -30;
    if (csi) s += 1000;
    if (ff && !esc) s += 100;
    return s;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* the name's extension is one of the list ("ans\0ice\0\0") */
static int ext(const char *name, const char *list)
{
    const char *dot = 0, *p;
    for (p = name; *p; p++)
        if (*p == '.') dot = p;
    if (!dot) return 0;
    while (*list) {
        const char *a = dot + 1, *b = list;
        while (*a && *b && lower(*a) == *b) {
            a++;
            b++;
        }
        if (!*a && !*b) return 1;
        while (*list) list++;
        list++;
    }
    return 0;
}

/* 1 PC art, 0 not ours, 2 character art with no font named (the bytes
 * decide), -1 no record */
static int sauce(const unsigned char *r)
{
    static const char id[] = "SAUCE00", amifont[] = "Amiga";
    int i;
    for (i = 0; i < 7; i++)
        if (r[i] != (unsigned char)id[i]) return -1;
    if (r[94] != 1 || r[95] > 2) return 0;      /* not character art */
    if (r[106] == 0 || r[106] == ' ') return 2;
    for (i = 0; i < 5; i++)
        if (r[106 + i] != (unsigned char)amifont[i]) return 1;
    return 0;
}

int ansi_claim(const char *name, const unsigned char *tail, const unsigned char *b, long n)
{
    int rec = tail ? sauce(tail) : -1, esc = 0, high = 0;
    long pc;
    long i;
    /* the record decides, when there is one */
    if (rec == 0 || rec == 1) return rec;
    /* the art ends at the first EOF byte; a NUL before it is not text art
     * (in a file with a record it may be a glyph) */
    for (i = 0; i < n; i++) {
        if (b[i] == 0x1A) break;
        if (!b[i] && rec < 0) return 0;
        if (b[i] == 0x1B && i + 1 < n && b[i + 1] == '[') esc = 1;
        if (b[i] >= 0x80) high = 1;
    }
    if (!i) return 0;
    n = i;
    pc = ansi_guess(b, n);
    /* .ans: unless it leans Amiga (an Amiga BBS screen: LF line ends) */
    if (rec == 2 || ext(name, "ans\0ice\0")) return pc <= 0;
    pc = pc < 0;
    if (ext(name, "nfo\0diz\0asc\0")) return high && pc;
    return high && pc && esc;
}
