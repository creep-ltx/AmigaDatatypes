/* bench: qoadec.c and conv.c alone on the Amiga, the file already in
 * memory: how long decoding takes with slice.s and with the C loop, and
 * decoding + 8-bit conversion; and whether slice.s gives the same samples.
 *   bench FILE.qoa */
#include <stdio.h>
#include <stdlib.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "qoadec.h"
#include "conv.h"

static ULONG ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (ULONG)d.ds_Minute * 3000 + d.ds_Tick;
}

static ULONG sum;                       /* of every sample, to compare the loops */

static ULONG run(UBYTE *b, LONG size, int conv, ULONG *samples)
{
    QoaDec d;
    Conv c;
    WORD *pcm;
    LONG pos;
    ULONG t = ticks();
    qa_header(&d, b);
    qa_limit(&d, size);
    cv_init(&c, &d, 1);
    c.out[0] = AllocVec(c.len + 2, MEMF_ANY);
    c.out[1] = AllocVec(c.len + 2, MEMF_ANY);
    pcm = AllocVec(QOA_FRAMELEN * d.channels * 2, MEMF_ANY);
    *samples = 0;
    for (pos = 8;;) {
        ULONG fl;
        if (size - pos < 8 || !(fl = qa_frameheader(&d, b + pos))) break;
        qa_frame(&d, b + pos, pcm);
        if (conv) cv_frame(&c, pcm, d.fsamples);
        else {
            ULONG k;
            for (k = 0; k < d.fsamples * d.channels; k++) sum = sum * 31 + (UWORD)pcm[k];
        }
        *samples += d.fsamples * d.channels;
        pos += fl;
        if (d.fsamples != QOA_FRAMELEN) break;
    }
    FreeVec(pcm); FreeVec(c.out[1]); FreeVec(c.out[0]);
    return ticks() - t;
}

int main(int argc, char **argv)
{
    BPTR fh;
    LONG size;
    UBYTE *b;
    ULONG n, t1, t2, t3, s1;
    if (argc != 2 || !(fh = Open(argv[1], MODE_OLDFILE))) return 10;
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    b = AllocVec(size, MEMF_ANY);
    Read(fh, b, size);
    Close(fh);
    printf("file in %s RAM\n", TypeOfMem(b) & MEMF_FAST ? "fast" : "chip");
    qa_useasm = 1;
    sum = 0;
    t1 = run(b, size, 0, &n);
    s1 = sum;
    t2 = run(b, size, 1, &n);
    qa_useasm = 0;
    sum = 0;
    t3 = run(b, size, 0, &n);
    printf("%lu samples: slice.s %lu.%02lu s (%lu samples/s), + 8-bit %lu.%02lu s; C loop %lu.%02lu s; %s\n",
           (unsigned long)n, (unsigned long)(t1 / 50), (unsigned long)(t1 % 50 * 2),
           t1 ? (unsigned long)(n * 50 / t1) : 0, (unsigned long)(t2 / 50), (unsigned long)(t2 % 50 * 2),
           (unsigned long)(t3 / 50), (unsigned long)(t3 % 50 * 2), s1 == sum ? "same samples" : "SAMPLES DIFFER");
    FreeVec(b);
    return 0;
}
