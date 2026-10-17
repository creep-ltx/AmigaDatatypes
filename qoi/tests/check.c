/* check: qoidec against the reference decoder (ref/qoi.h, MIT, Dominic
 * Szablewski), on the PC. Every pixel in all three layouts, read through
 * odd-sized pieces so the refilling is tested too. As a fuzzer
 * (-DFUZZ, clang -fsanitize=fuzzer,address) the same on any bytes.
 *
 *   cc -O2 -I../src check.c ../src/qoidec.c -o check && ./check FILE.qoi... */
#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "ref/qoi.h"
#include "qoidec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const unsigned char *d; long n, at; unsigned step; } Mem;

static LONG memread(void *ctx, UBYTE *dst, LONG n)
{
    Mem *m = ctx;
    long k = m->n - m->at;
    if (m->step) {                      /* odd piece sizes: 1..step bytes */
        long s = 1 + (m->at * 7 + 3) % m->step;
        if (n > s) n = s;
    }
    if (k > n) k = n;
    memcpy(dst, m->d + m->at, k);
    m->at += k;
    return k;
}

static QoiDec dec;

/* 0 = the same as the reference, 1 = different, -1 = both refuse */
static int one(const unsigned char *data, long size, unsigned step, int fmt, char *why)
{
    qoi_desc desc;
    unsigned char *ref = qoi_decode(data, (int)size, &desc, 4);
    Mem m = { data, size, QOI_HEADER, step };
    unsigned char *out;
    unsigned long i, npx, bpp = fmt == QD_RGB ? 3 : 4, band, done;
    int ok = size >= QOI_HEADER && qd_header(&dec, data, size, memread, &m);
    if (!ref || !ok) {
        if (ref) free(ref);
        if (!ref && !ok) return -1;
        sprintf(why, "header: reference %s, qoidec %s", ref ? "yes" : "no", ok ? "yes" : "no");
        return 1;
    }
    npx = desc.width * desc.height;
    out = malloc(npx * bpp + 16);
    /* in bands of 1..37 rows, as the datatype asks */
    for (done = 0; done < npx; done += band) {
        band = desc.width * (1 + (done / desc.width) % 37);
        if (band > npx - done) band = npx - done;
        qd_pixels(&dec, out + done * bpp, band, fmt);
    }
    for (i = 0; i < npx; i++) {
        unsigned char r, g, b, a;
        if (fmt == QD_RGB) { r = out[i*3]; g = out[i*3+1]; b = out[i*3+2]; a = ref[i*4+3]; }
        else {
            uint32_t v;
            memcpy(&v, out + i * 4, 4);
            if (fmt == QD_ARGB) v = (v << 8) | (v >> 24);
            r = v >> 24; g = v >> 16; b = v >> 8; a = v;
        }
        if (r != ref[i*4] || g != ref[i*4+1] || b != ref[i*4+2] || a != ref[i*4+3]) {
            sprintf(why, "pixel %lu: %02x%02x%02x%02x, reference %02x%02x%02x%02x", i,
                r, g, b, a, ref[i*4], ref[i*4+1], ref[i*4+2], ref[i*4+3]);
            free(out); free(ref);
            return 1;
        }
    }
    free(out); free(ref);
    return 0;
}

#ifdef FUZZ
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    char why[200];
    if (size > 14) {                    /* keep the pictures small */
        unsigned long w = ((unsigned long)data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        unsigned long h = ((unsigned long)data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
        if (w && h && (w > 4096 || h > 4096 || w * h > 1 << 20)) return 0;
    }
    if (one(data, size, size % 23, size % 3, why) == 1) { fprintf(stderr, "%s\n", why); abort(); }
    return 0;
}
#else
int main(int argc, char **argv)
{
    int i, fmt, bad = 0;
    unsigned step;
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        long n;
        unsigned char *d;
        char why[200];
        if (!f) { perror(argv[i]); return 2; }
        fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
        d = malloc(n + 1);
        if (fread(d, 1, n, f) != (size_t)n) { fclose(f); return 2; }
        fclose(f);
        for (fmt = 0; fmt < 3; fmt++)
            for (step = 0; step < 40; step += 13) {
                int r = one(d, n, step, fmt, why);
                if (r == 1) { printf("FAIL %s layout %d step %u: %s\n", argv[i], fmt, step, why); bad++; goto next; }
                if (r == -1) { printf("both refuse %s\n", argv[i]); goto next; }
            }
        printf("ok %s\n", argv[i]);
    next:
        free(d);
    }
    return bad != 0;
}
#endif
