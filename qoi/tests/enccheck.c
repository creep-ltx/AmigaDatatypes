/* enccheck: qoienc against the reference encoder (ref/qoi.h, MIT, Dominic
 * Szablewski), on the PC. Every byte must be the same. The pixels come from
 * QOI files (decoded by the reference) and from generated pictures (noise,
 * runs, gradients, few colours, alpha), fed in odd-sized pieces so the
 * streaming is tested too.
 *
 *   cc -O2 -Isrc tests/enccheck.c src/qoienc.c -o enccheck && ./enccheck [FILE.qoi...] */
#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "ref/qoi.h"
#include "qoienc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *d; long n, cap; } Out;

static int memwrite(void *ctx, const UBYTE *src, LONG n)
{
    Out *o = ctx;
    if (o->n + n > o->cap) {
        o->cap = (o->n + n) * 2;
        o->d = realloc(o->d, o->cap);
    }
    memcpy(o->d + o->n, src, n);
    o->n += n;
    return 1;
}

static QoiEnc enc;
static unsigned long seed = 12345;
static unsigned rnd(void) { seed = seed * 1103515245 + 12345; return (seed >> 16) & 0x7fff; }

/* rgba: w*h pixels, 4 bytes each. Gives 1 when the same as the reference. */
static int one(const char *name, unsigned char *rgba, unsigned w, unsigned h, int channels)
{
    qoi_desc desc = { w, h, channels, 0 };
    unsigned char *in, *ref;
    int reflen, ok;
    unsigned long i, npx = (unsigned long)w * h, done = 0;
    Out o = { 0, 0, 0 };

    in = rgba;
    if (channels == 3) {
        in = malloc(npx * 3);
        for (i = 0; i < npx; i++)
            memcpy(in + i * 3, rgba + i * 4, 3);
    }
    ref = qoi_encode(in, &desc, &reflen);
    qe_begin(&enc, w, h, channels, memwrite, &o);
    while (done < npx) {                /* pieces of 1..3000 pixels, sometimes whole rows */
        unsigned long k = (rnd() & 1) ? 1 + rnd() % 3000 : w;
        if (k > npx - done) k = npx - done;
        qe_pixels(&enc, rgba + done * 4, k, channels == 3);
        done += k;
    }
    qe_end(&enc);
    ok = ref && o.n == reflen && !memcmp(o.d, ref, reflen);
    if (!ok)
        printf("DIFFERENT %s %ux%u ch%d: ours %ld bytes, reference %d\n", name, w, h, channels, o.n, reflen);
    if (channels == 3) free(in);
    free(ref);
    free(o.d);
    return ok;
}

static unsigned char *gen(int kind, unsigned w, unsigned h)
{
    unsigned char *p = malloc((size_t)w * h * 4 + 4);
    unsigned long i, n = (unsigned long)w * h;
    unsigned char pal[16][4];

    for (i = 0; i < 16; i++) {
        pal[i][0] = rnd(); pal[i][1] = rnd(); pal[i][2] = rnd(); pal[i][3] = (rnd() & 1) ? 255 : rnd();
    }
    for (i = 0; i < n; i++) {
        unsigned char *q = p + i * 4;
        unsigned x = i % w, y = i / w;
        switch (kind) {
        case 0: q[0] = rnd(); q[1] = rnd(); q[2] = rnd(); q[3] = rnd(); break;           /* noise */
        case 1: q[0] = x; q[1] = y; q[2] = x + y; q[3] = 255; break;                     /* gradients */
        case 2: memcpy(q, pal[(x / 7 + y / 5) & 15], 4); break;                          /* few colours, runs */
        case 3: if (i && rnd() % 10) memcpy(q, q - 4, 4);                                /* long runs */
                else { q[0] = rnd(); q[1] = rnd(); q[2] = rnd(); q[3] = rnd() & 1 ? 255 : 0; }
                break;
        case 4: if (i) { memcpy(q, q - 4, 4); q[0] += rnd() % 5 - 2; q[1] += rnd() % 70 - 35; /* small steps */
                         q[2] += rnd() % 20 - 10; if (!(rnd() % 50)) q[3] = rnd(); }
                else { q[0] = q[1] = q[2] = 128; q[3] = 255; }
                break;
        default: memcpy(q, pal[rnd() % 3], 4); break;                                   /* index hits */
        }
    }
    return p;
}

int main(int argc, char **argv)
{
    int i, bad = 0, n = 0;

    for (i = 1; i < argc; i++) {        /* real pictures */
        FILE *f = fopen(argv[i], "rb");
        long size;
        unsigned char *data, *px;
        qoi_desc d;
        if (!f) continue;
        fseek(f, 0, SEEK_END); size = ftell(f); rewind(f);
        data = malloc(size);
        if (fread(data, 1, size, f) != (size_t)size) size = 0;
        fclose(f);
        px = qoi_decode(data, (int)size, &d, 4);
        if (px) {
            n += 2;
            bad += !one(argv[i], px, d.width, d.height, 4);
            bad += !one(argv[i], px, d.width, d.height, 3);
            free(px);
        }
        free(data);
    }
    for (i = 0; i < 600; i++) {         /* generated ones */
        int kind = i % 6, ch = (i / 6) & 1 ? 3 : 4;
        unsigned w = 1 + rnd() % (i < 300 ? 40 : 700), h = 1 + rnd() % (i < 300 ? 40 : 300);
        char name[32];
        unsigned char *p = gen(kind, w, h);
        sprintf(name, "generated-%d-kind%d", i, kind);
        n++;
        bad += !one(name, p, w, h, ch);
        free(p);
    }
    printf("enccheck: %d pictures, %d different from the reference encoder\n", n, bad);
    return bad != 0;
}
