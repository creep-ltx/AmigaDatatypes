/* mkcorpus: QOI test files for the Amiga-side check (tests/amigacheck.sh):
 * random pictures of many shapes encoded by the reference encoder, then
 * damaged copies (bytes changed, cut short), and for each the reference
 * decoder's RGBA pixels (NAME.ref: width, height as 4-byte big-endian
 * numbers, then the pixels), or no .ref when it refuses the file.
 *   cc -O2 mkcorpus.c -o mkcorpus && ./mkcorpus DIR COUNT */
#define QOI_IMPLEMENTATION
#include "ref/qoi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned rnd(void) { static unsigned s = 12345; s = s * 1103515245 + 12345; return s >> 8; }

static void put(const char *dir, int k, const char *tag, const unsigned char *data, int size)
{
    char name[300];
    qoi_desc d;
    unsigned char *px;
    FILE *f;
    sprintf(name, "%s/%04d%s.qoi", dir, k, tag);
    f = fopen(name, "wb"); fwrite(data, 1, size, f); fclose(f);
    if ((px = qoi_decode(data, size, &d, 4))) {
        unsigned char h[8] = { d.width >> 24, d.width >> 16, d.width >> 8, d.width,
                               d.height >> 24, d.height >> 16, d.height >> 8, d.height };
        sprintf(name, "%s/%04d%s.ref", dir, k, tag);
        f = fopen(name, "wb"); fwrite(h, 1, 8, f); fwrite(px, 4, d.width * d.height, f); fclose(f);
        free(px);
    }
}

int main(int argc, char **argv)
{
    int k, count = atoi(argv[2]);
    for (k = 0; k < count; k++) {
        qoi_desc d;
        int w = 1 + rnd() % 70, h = 1 + rnd() % 50, ch = 3 + rnd() % 2, i, len, style = rnd() % 5;
        unsigned char *px = malloc(w * h * 4), *enc, *bad;
        unsigned r = rnd() & 255, g = rnd() & 255, b = rnd() & 255, a = 255;
        for (i = 0; i < w * h; i++) {
            /* a mix that gives every op: runs, small changes, repeats, jumps */
            switch (style == 4 ? rnd() % 4 : style) {
            case 0: if (rnd() % 8 == 0) { r = rnd(); g = rnd(); b = rnd(); } break;
            case 1: r += rnd() % 5 - 2; g += rnd() % 5 - 2; b += rnd() % 5 - 2; break;
            case 2: { int vg = rnd() % 64 - 32; g += vg; r += vg + rnd() % 16 - 8; b += vg + rnd() % 16 - 8; } break;
            case 3: r = rnd(); g = rnd(); b = rnd(); if (ch == 4) a = rnd() % 3 ? 255 : rnd(); break;
            }
            px[i*4] = r; px[i*4+1] = g; px[i*4+2] = b; px[i*4+3] = ch == 4 ? (rnd() % 7 ? a : rnd()) : 255;
        }
        d.width = w; d.height = h; d.channels = ch; d.colorspace = rnd() % 2;
        enc = qoi_encode(px, &d, &len);
        put(argv[1], k, "", enc, len);
        bad = malloc(len);
        memcpy(bad, enc, len);
        for (i = 0; i < 1 + (int)(rnd() % 6); i++) bad[14 + rnd() % (len - 14)] = rnd();
        put(argv[1], k, "x", bad, len);                     /* changed bytes */
        put(argv[1], k, "t", enc, 14 + rnd() % (len - 13)); /* cut short */
        free(px); free(enc); free(bad);
    }
    return 0;
}
