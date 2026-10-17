/* enc: raw 16-bit little-endian PCM (as ffmpeg -f s16le writes it) into a
 * QOA file with the reference encoder; qoaconv only reads 1-2 channel WAVs.
 *   enc IN.raw CHANNELS RATE OUT.qoa */
#include <stdio.h>
#include <stdlib.h>
#define QOA_IMPLEMENTATION
#include "ref/qoa.h"

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    long size;
    short *pcm;
    qoa_desc d = { 0 };
    if (argc != 5 || !f) return 2;
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    pcm = malloc(size + 2);
    if (fread(pcm, 1, size, f) != (size_t)size) return 2;
    fclose(f);
    d.channels = atoi(argv[2]);
    d.samplerate = atoi(argv[3]);
    d.samples = size / 2 / d.channels;
    return qoa_write(argv[4], pcm, &d) ? 0 : 1;
}
