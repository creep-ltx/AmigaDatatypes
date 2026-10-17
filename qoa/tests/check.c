/* check: every sample of qoadec.c against the reference decoder
 * (tests/ref/qoa.h), on the PC. For each file: "ok", "both refuse", or
 * where they differ. Exit status 1 if any differ.
 *
 * Damaged files: the reference stops at the first frame that does not fit
 * and keeps what came before; so does qoadec (the datatype's loop below is
 * the same). Samples past the file header's length are not compared (the
 * reference keeps them, the datatype has no room for them). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define QOA_IMPLEMENTATION
#include "ref/qoa.h"
#include "qoadec.h"

/* the datatype's loop (class.c), on a buffer instead of a file */
static WORD *mine(const unsigned char *b, unsigned long size, unsigned long *got, QoaDec *d)
{
    unsigned long pos = QOA_HEADER - 8, total = 0;
    WORD *out, *fb;
    if (size < QOA_HEADER || !qa_header(d, b)) return NULL;
    qa_limit(d, size);
    out = calloc((size_t)d->samples * d->channels + 1, sizeof(WORD));
    fb = malloc(QOA_FRAMELEN * d->channels * sizeof(WORD));
    for (;;) {
        unsigned long n, len;
        if (size - pos < 8 || !(len = qa_frameheader(d, b + pos)) || d->fsize > size - pos) break;
        qa_frame(d, b + pos, fb);
        n = d->fsamples;
        if (n > d->samples - total) n = d->samples - total;
        memcpy(out + total * d->channels, fb, n * d->channels * sizeof(WORD));
        total += n;
        pos += len;
        if (d->fsamples != QOA_FRAMELEN || total >= d->samples) break;
    }
    free(fb);
    *got = total;
    return out;
}

int main(int argc, char **argv)
{
    int i, bad = 0;
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        unsigned char *b;
        long size;
        qoa_desc ref;
        QoaDec d;
        short *rs;
        WORD *ms;
        unsigned long got, want, k;
        if (!f) { perror(argv[i]); return 2; }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        b = malloc(size + 1);
        if (fread(b, 1, size, f) != (size_t)size) { perror(argv[i]); return 2; }
        fclose(f);
        memset(&ref, 0, sizeof ref);
        rs = qoa_decode(b, size, &ref);
        ms = mine(b, size, &got, &d);
        if (!rs || !ms) {
            if (!rs && !ms) printf("both refuse %s\n", argv[i]);
            else if (!rs && ref.samples && (unsigned long long)ref.samples * ref.channels > 0x7fffffff / QOA_FRAMELEN * QOA_FRAMELEN)
                printf("ok (too long for the reference) %s\n", argv[i]);
            else if (!ms && ref.channels > QOA_MAXCH) printf("ok (over 8 channels) %s\n", argv[i]);
            else { printf("DIFFER (%s refuses) %s\n", rs ? "qoadec" : "reference", argv[i]); bad = 1; }
        } else {
            want = ref.samples < d.samples ? ref.samples : d.samples;
            if (got != want) {
                printf("DIFFER length %lu, reference %lu: %s\n", got, want, argv[i]);
                bad = 1;
            } else {
                for (k = 0; k < got * d.channels; k++)
                    if (rs[k] != ms[k]) break;
                if (k < got * d.channels) {
                    printf("DIFFER at sample %lu: %d, reference %d: %s\n", k, ms[k], rs[k], argv[i]);
                    bad = 1;
                } else
                    printf("ok %s (%lu samples, %lu ch, %lu Hz)\n", argv[i], got, (unsigned long)d.channels, (unsigned long)d.rate);
            }
        }
        free(rs);
        free(ms);
        free(b);
    }
    return bad;
}
