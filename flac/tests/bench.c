/* bench: on the Amiga, the decoder alone on a file in memory: the time for
 * all its frames with rice.s and with the C loop (and a sum of all the
 * samples, which must be the same), then without the frame CRC and without
 * the predictors (to see what each costs; the samples are then wrong).
 *   bench FILE.flac */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "stream.h"

extern int fl_bench;                    /* flacdec.c with -DFL_BENCH: 1 no CRC, 2 no predictors either */

static ULONG ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (ULONG)d.ds_Minute * 3000 + d.ds_Tick;
}

int main(int argc, char **argv)
{
    FILE *f;
    UBYTE *b;
    long size, off, start;
    ULONG k, n, t, mode, frames, i, sum;
    FlacDec d;
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) return 10;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(size + 16);
    if (fread(b, 1, size, f) != (size_t)size) return 10;
    fclose(f);
    fl_init();
    if (memcmp(b, "fLaC", 4)) return 10;
    for (off = 4;;) {
        ULONG len;
        int last;
        if (fl_blockheader(b + off, &len, &last) == 0) fl_streaminfo(&d, b + off + 4);
        off += 4 + len;
        if (last) break;
    }
    start = off;
    for (k = 0; k < d.channels; k++) d.pcm[k] = malloc(d.maxblock * 4);
    for (mode = 0; mode < 4; mode++) {
        fl_bench = mode < 2 ? 0 : mode - 1;
        fl_useasm = mode != 1;
        sum = 0;
        t = ticks();
        for (n = frames = 0, off = start; off < size;) {
            ULONG used = fl_frame(&d, b + off, size - off);
            if (!used) { off += fl_sync(&d, b + off, size - off); continue; }
            off += used;
            n += d.fsamples;
            for (k = 0; k < d.channels; k++)
                for (i = 0; i < d.fsamples; i++) sum = (sum << 1 | sum >> 31) ^ (ULONG)d.pcm[k][i];
            frames++;
        }
        t = ticks() - t;
        printf("%s: %lu frames, %lu samples a channel, %lu.%02lu s, %lu samples/s a channel, sum %08lx\n",
               mode == 0 ? "rice.s" : mode == 1 ? "C loop" : mode == 2 ? "no CRC" : "no CRC, no predictors",
               frames, n, t / 50, t % 50 * 2, t ? n * 50 / t : 0, sum);
    }
    return 0;
}
