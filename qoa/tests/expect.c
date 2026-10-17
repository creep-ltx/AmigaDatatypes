/* expect: on the PC, the 8-bit samples qoa.datatype should give for a
 * file, in sndtest's DUMP layout, from the same qoadec.c and conv.c.
 *   expect FILE.qoa STEREO(0/1) OUT */
#include <stdio.h>
#include <stdlib.h>
#include "qoadec.h"
#include "conv.h"

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *b;
    long size, pos;
    QoaDec d;
    Conv c;
    WORD *pcm;
    if (argc != 4 || !(f = fopen(argv[1], "rb"))) return 2;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(size);
    if (fread(b, 1, size, f) != (size_t)size) return 2;
    fclose(f);
    if (size < QOA_HEADER || !qa_header(&d, b)) { printf("refused\n"); return 1; }
    qa_limit(&d, size);
    printf("%lu Hz\n", (unsigned long)cv_init(&c, &d, atoi(argv[2])));
    c.out[0] = calloc(c.len + 2, 1);
    c.out[1] = calloc(c.len + 2, 1);
    pcm = malloc(QOA_FRAMELEN * d.channels * sizeof(WORD));
    for (pos = QOA_HEADER - 8;;) {
        ULONG fl;
        if (size - pos < 8 || !(fl = qa_frameheader(&d, b + pos)) || d.fsize > (ULONG)(size - pos)) break;
        qa_frame(&d, b + pos, pcm);
        cv_frame(&c, pcm, d.fsamples);
        pos += fl;
        if (d.fsamples != QOA_FRAMELEN || c.pos >= c.len) break;
    }
    f = fopen(argv[3], "wb");
    fwrite(c.out[0], 1, c.pos, f);
    if (c.outch == 2) fwrite(c.out[1], 1, c.pos, f);
    fclose(f);
    printf("%lu samples, %s\n", (unsigned long)c.pos, c.outch == 2 ? "stereo" : "mono");
    return 0;
}
