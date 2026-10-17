/* expect: on the PC, the 8-bit samples flac.datatype should give for a
 * file, in sndtest's DUMP layout, from the same flacdec.c, stream.c and conv.c.
 *   expect FILE.flac STEREO(0/1) OUT */
#include <stdio.h>
#include <stdlib.h>
#include "memread.h"

int main(int argc, char **argv)
{
    FILE *f;
    UBYTE *b, *win, cbuf[4096];
    long size;
    ULONG k, good, bad, wsize;
    FlacDec d;
    FlStream s;
    Mem m;
    Conv c;
    if (argc != 4 || !(f = fopen(argv[1], "rb"))) return 2;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    b = calloc(size + 16, 1);
    if (fread(b, 1, size, f) != (size_t)size) return 2;
    fclose(f);
    fl_init();
    m.b = b; m.size = size; m.pos = 0;
    s.read = mread; s.skip = mskip; s.h = &m; s.buf = cbuf; s.bufsize = sizeof(cbuf);
    if (!fs_meta(&s, &d)) { printf("refused\n"); return 1; }
    for (k = 0; k < d.channels; k++) d.pcm[k] = malloc(d.maxblock * sizeof(LONG));
    printf("%lu Hz\n", (unsigned long)cv_init(&c, &d, d.samples, atoi(argv[2])));
    c.out[0] = calloc(c.len + 2, 1);
    c.out[1] = calloc(c.len + 2, 1);
    wsize = FS_WINDOW(&d);
    win = calloc(wsize + 16, 1);
    fs_frames(&s, &d, &c, win, &good, &bad);
    f = fopen(argv[3], "wb");
    fwrite(c.out[0], 1, c.len, f);
    if (c.outch == 2) fwrite(c.out[1], 1, c.len, f);
    fclose(f);
    printf("%lu samples, %s, %lu frames, %lu skips, title \"%s\", artist \"%s\"\n", (unsigned long)c.len,
           c.outch == 2 ? "stereo" : "mono", (unsigned long)good, (unsigned long)bad, s.title, s.artist);
    return 0;
}
