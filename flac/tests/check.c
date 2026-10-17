/* check: on the PC, flacdec.c against a reference decoder's samples.
 *   check FILE.flac [REF.s32]
 * REF: ffmpeg's samples, signed 32-bit little-endian, interleaved, each
 * shifted to the top (ffmpeg -f s32le). Every frame flacdec takes must give
 * the same samples, and every frame of the file must be taken. Without REF
 * (damaged files) it only has to come through: run with the address and
 * undefined-behaviour checkers. Then the whole stream.c / conv.c path in
 * mono and stereo, for the checkers too. */
#include <stdio.h>
#include <stdlib.h>
#include "memread.h"

int main(int argc, char **argv)
{
    FILE *f;
    UBYTE *b, *ref = NULL, *win;
    long size, rsize = 0, off, refn = 0;
    ULONG good = 0, bad = 0, k, i, total = 0, g2, b2;
    FlacDec d;
    FlStream s;
    Mem m;
    Conv c;
    int st, wrong = 0;
    UBYTE cbuf[4096];

    int list = argc > 2 && !strcmp(argv[1], "-f");     /* -f FILE: where the frames are */
    if (list) argv++, argc--;
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) return 2;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    b = calloc(size + 16, 1);
    if (fread(b, 1, size, f) != (size_t)size) return 2;
    fclose(f);
    if (argc > 2) {
        if (!(f = fopen(argv[2], "rb"))) return 2;
        fseek(f, 0, SEEK_END); rsize = ftell(f); fseek(f, 0, SEEK_SET);
        ref = malloc(rsize + 1);
        if (fread(ref, 1, rsize, f) != (size_t)rsize) return 2;
        fclose(f);
    }
    fl_init();
    m.b = b; m.size = size; m.pos = 0;
    s.read = mread; s.skip = mskip; s.h = &m; s.buf = cbuf; s.bufsize = sizeof(cbuf);
    if (!fs_meta(&s, &d)) {
        printf("%s: refused\n", argv[1]);
        return ref ? 1 : 0;
    }
    for (k = 0; k < d.channels; k++) d.pcm[k] = malloc(d.maxblock * sizeof(LONG));
    if (ref) refn = rsize / 4 / d.channels;

    /* every frame, in memory */
    for (off = m.pos; off < size;) {
        ULONG used = fl_frame(&d, b + off, size - off);
        if (!used) {
            bad++;
            off += fl_sync(&d, b + off, size - off);
            continue;
        }
        good++;
        total += d.fsamples;
        if (list) printf("%ld %lu %lu %lu\n", off, (unsigned long)used, (unsigned long)d.fstart, (unsigned long)d.fsamples);
        if (ref) {
            for (i = 0; i < d.fsamples && !wrong; i++)
                for (k = 0; k < d.channels; k++) {
                    long at = ((long)(d.fstart + i) * d.channels + k) * 4;
                    LONG want, got = (LONG)((ULONG)d.pcm[k][i] << (32 - d.bps));
                    if (at + 4 > rsize) { wrong = 1; break; }
                    want = (LONG)((ULONG)ref[at] | (ULONG)ref[at + 1] << 8 | (ULONG)ref[at + 2] << 16 | (ULONG)ref[at + 3] << 24);
                    if (got != want) {
                        printf("%s: sample %lu channel %lu: %ld, should be %ld\n", argv[1],
                               (unsigned long)(d.fstart + i), (unsigned long)k, (long)got, (long)want);
                        wrong = 1;
                        break;
                    }
                }
        }
        off += used;
    }
    if (list) return 0;
    if (ref) {
        if (!wrong && (bad || total != (ULONG)refn || total != d.samples)) {
            printf("%s: %lu samples in %lu frames (%lu not taken), reference %ld, header %lu\n", argv[1],
                   (unsigned long)total, (unsigned long)good, (unsigned long)bad, refn, (unsigned long)d.samples);
            wrong = 1;
        }
        if (!wrong) printf("ok %s: %lu frames, %lu samples\n", argv[1], (unsigned long)good, (unsigned long)total);
    } else printf("damaged %s: %lu frames taken, %lu skips\n", argv[1], (unsigned long)good, (unsigned long)bad);

    /* the datatype's path, from the start, through the window */
    for (st = 0; st < 2; st++) {
        ULONG wsize = FS_WINDOW(&d), n;
        m.pos = 0;
        fs_meta(&s, &d);
        n = d.samples;
        if (n > 50000000) n = 50000000;
        cv_init(&c, &d, n, st);
        c.out[0] = malloc(c.len + 2);
        c.out[1] = malloc(c.len + 2);
        memset(c.out[0], 0, c.len + 2);
        memset(c.out[1], 0, c.len + 2);
        win = calloc(wsize + 16, 1);
        fs_frames(&s, &d, &c, win, &g2, &b2);
        if (ref && g2 != good && c.pos < c.len) {   /* it may stop when the sound is full */
            printf("%s: the window took %lu frames, memory %lu\n", argv[1], (unsigned long)g2, (unsigned long)good);
            wrong = 1;
        }
        free(win); free(c.out[0]); free(c.out[1]);
    }
    return wrong;
}
