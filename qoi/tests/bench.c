/* bench: qoidec alone on the Amiga - decode a file N times into memory,
 * the time per run. Built by `make bench` (build/qoibench).
 *   qoibench FILE.qoi [RUNS] [rgba|argb|rgb] [c]   (c: the C loop even on a 68020) */
#include <exec/types.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <exec/execbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qoidec.h"

struct Device *TimerBase;
static LONG rd(void *fh, UBYTE *dst, LONG n) { return Read((BPTR)fh, dst, n); }

int main(int argc, char **argv)
{
    static QoiDec d;
    struct timerequest tr;
    struct EClockVal t0, t1;
    ULONG freq, ms, best = ~0UL;
    int runs = argc > 2 ? atoi(argv[2]) : 3, fmt = QD_RGBA, i;
    UBYTE h[14], *out;
    BPTR fh;
    LONG size;
    qd_use020 = (SysBase->AttnFlags & AFF_68020) && !(argc > 4 && argv[4][0] == 'c');
    if (argc > 3) fmt = !strcmp(argv[3], "argb") ? QD_ARGB : !strcmp(argv[3], "rgb") ? QD_RGB : QD_RGBA;
    if (OpenDevice("timer.device", UNIT_ECLOCK, (struct IORequest *)&tr, 0)) return 20;
    TimerBase = tr.tr_node.io_Device;
    if (!(fh = Open(argv[1], MODE_OLDFILE))) return 20;
    Seek(fh, 0, OFFSET_END); size = Seek(fh, 0, OFFSET_BEGINNING);
    for (i = 0; i < runs; i++) {
        Seek(fh, 0, OFFSET_BEGINNING);
        Read(fh, h, 14);
        if (!qd_header(&d, h, size, rd, (void *)fh)) { printf("not qoi\n"); return 10; }
        if (!i && !(out = malloc(d.width * d.height * 4))) { printf("no memory\n"); return 10; }
        freq = ReadEClock(&t0);
        qd_pixels(&d, out, d.width * d.height, fmt);
        ReadEClock(&t1);
        ms = (t1.ev_lo - t0.ev_lo) / (freq / 1000);
        if (ms < best) best = ms;
    }
    printf("%s%s %lux%lu: %lu ms (best of %d), %lu ns a pixel\n", qd_use020 ? "[asm] " : "[C] ", argv[1], d.width, d.height, best, runs,
           best * 1000UL / (d.width * d.height / 1000));
    Close(fh);
    return 0;
}
