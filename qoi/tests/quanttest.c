/* quanttest: the OS 3.0/3.1 path on the PC - decode twice, count, make the
 * palette, map with dithering - and write the result as a PPM to look at.
 *   cc -O2 -I../src quanttest.c ../src/qoidec.c ../src/quant.c -o quanttest
 *   ./quanttest IN.qoi OUT.ppm */
#include "qoidec.h"
#include "quant.h"
#include <stdio.h>
#include <stdlib.h>

static LONG fread_(void *ctx, UBYTE *dst, LONG n) { return (LONG)fread(dst, 1, n, (FILE *)ctx); }

int main(int argc, char **argv)
{
    static QoiDec d;
    static Quant q;
    FILE *f = fopen(argv[1], "rb"), *o;
    UBYTE h[14], *idx;
    ULONG *row, y, x, w, hh;
    WORD *err;
    long size;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    if (fread(h, 1, 14, f) != 14 || !qd_header(&d, h, size, fread_, f)) { puts("not qoi"); return 1; }
    w = d.width; hh = d.height;
    row = malloc(w * 4); idx = malloc(w); err = calloc(6 * (w + 2), 2);
    qu_init(&q);
    for (y = 0; y < hh; y++) {
        qd_pixels(&d, (UBYTE *)row, w, QD_RGBA);
        for (x = 0; x < w; x++) { UBYTE *b = (UBYTE *)&row[x]; row[x] = ((ULONG)b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0]; }
        if (d.alphaand != 255 || d.channels == 4) qu_flatten(row, w);
        qu_count(&q, row, w);
    }
    qu_palette(&q);
    printf("%s: %lu colours, %s\n", argv[1], (unsigned long)q.ncolors, q.overflow ? "median cut + dither" : "exact");
    fseek(f, 14, SEEK_SET);
    qd_header(&d, h, size, fread_, f);
    o = fopen(argv[2], "wb");
    fprintf(o, "P6 %lu %lu 255\n", (unsigned long)w, (unsigned long)hh);
    for (y = 0; y < hh; y++) {
        qd_pixels(&d, (UBYTE *)row, w, QD_RGBA);
        for (x = 0; x < w; x++) { UBYTE *b = (UBYTE *)&row[x]; row[x] = ((ULONG)b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0]; }
        if (d.channels == 4) qu_flatten(row, w);
        qu_map(&q, row, idx, w, err);
        for (x = 0; x < w; x++) fwrite(q.pal[idx[x]], 1, 3, o);
    }
    fclose(o);
    return 0;
}
