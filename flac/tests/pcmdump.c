/* pcmdump: every frame's samples as flacdec.c gives them, for comparing the
 * Amiga build with the PC's: per frame its start, size and channel layout,
 * then the samples a channel at a time, big-endian longs.
 *   pcmdump FILE.flac OUT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flacdec.h"

static void put(FILE *f, unsigned long v)
{
    putc(v >> 24, f); putc(v >> 16, f); putc(v >> 8, f); putc(v, f);
}

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *b;
    long size, off;
    unsigned long k, i;
    FlacDec d;
    if (argc != 3 || !(f = fopen(argv[1], "rb"))) return 10;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    b = calloc(size + 16, 1);
    if (fread(b, 1, size, f) != (size_t)size) return 10;
    fclose(f);
    fl_init();
    for (off = 4;;) {
        ULONG len;
        int last;
        if (fl_blockheader(b + off, &len, &last) == 0) fl_streaminfo(&d, b + off + 4);
        off += 4 + len;
        if (last) break;
    }
    for (k = 0; k < d.channels; k++) d.pcm[k] = malloc(d.maxblock * 4);
    f = fopen(argv[2], "wb");
    while (off < size) {
        ULONG used = fl_frame(&d, b + off, size - off);
        if (!used) { off += fl_sync(&d, b + off, size - off); continue; }
        put(f, d.fstart); put(f, d.fsamples); put(f, b[off + 3] >> 4);
        for (k = 0; k < d.channels; k++)
            for (i = 0; i < d.fsamples; i++) put(f, (unsigned long)d.pcm[k][i]);
        off += used;
    }
    fclose(f);
    return 0;
}
