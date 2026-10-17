/* amigacheck: qoidec on the Amiga against the reference decoder's pixels
 * (NAME.ref from mkcorpus). Every file in all three layouts, in bands of
 * odd sizes, with the 68020 loop and with the C loop.
 *   amigacheck LISTFILE [TRACEFILE]   (one NAME per line, NAME.qoi and NAME.ref beside) */
#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qoidec.h"

static QoiDec d;
static LONG rd(void *fh, UBYTE *dst, LONG n) { return Read((BPTR)fh, dst, n); }

static UBYTE *load(const char *name, LONG *size)
{
    BPTR f = Open((STRPTR)name, MODE_OLDFILE);
    UBYTE *b;
    if (!f) return NULL;
    Seek(f, 0, OFFSET_END); *size = Seek(f, 0, OFFSET_BEGINNING);
    b = malloc(*size + 1);
    if (b) Read(f, b, *size);
    Close(f);
    return b;
}

int main(int argc, char **argv)
{
    FILE *list = fopen(argv[1], "r");
    char name[256], path[300];
    int files = 0, bad = 0, refused = 0, use, fmt;
    int has020 = (SysBase->AttnFlags & AFF_68020) != 0;
    while (fgets(name, sizeof name, list)) {
        LONG rsize = 0, qsize;
        UBYTE *ref, h[14], *out;
        ULONG w, hh, npx, i, done, band;
        name[strcspn(name, "\r\n")] = 0;
        if (!name[0]) continue;
        sprintf(path, "%s.ref", name);
        ref = load(path, &rsize);
        files++;
        if (argc > 2) {                 /* a trace: the name before each file, on disk */
            BPTR t = Open((STRPTR)argv[2], MODE_READWRITE);
            if (t) { Seek(t, 0, OFFSET_END); Write(t, name, strlen(name)); Write(t, "\n", 1); Close(t); }
        }
        for (use = has020; use >= 0; use--)
            for (fmt = 0; fmt < 3; fmt++) {
                BPTR f;
                int ok;
                sprintf(path, "%s.qoi", name);
                f = Open((STRPTR)path, MODE_OLDFILE);
                Seek(f, 0, OFFSET_END); qsize = Seek(f, 0, OFFSET_BEGINNING);
                ok = qsize >= 14 && Read(f, h, 14) == 14 && qd_header(&d, h, qsize, rd, (void *)f);
                if (!ok || !ref) {
                    if (ok != (ref != NULL)) { printf("FAIL %s: header %d, reference %d\n", name, ok, ref != NULL); bad++; }
                    else if (!use || !has020) refused += fmt == 0;
                    Close(f);
                    continue;
                }
                qd_use020 = use;
                w = d.width; hh = d.height; npx = w * hh;
                out = malloc(npx * 4 + 8);
                for (done = 0; done < npx; done += band) {
                    band = 1 + (done * 7 + fmt * 13) % (w * 3 + 5);
                    if (band > npx - done) band = npx - done;
                    qd_pixels(&d, out + done * (fmt == QD_RGB ? 3 : 4), band, fmt);
                }
                Close(f);
                for (i = 0; i < npx; i++) {
                    UBYTE *r = ref + 8 + i * 4, *o = out + i * (fmt == QD_RGB ? 3 : 4);
                    int same = fmt == QD_RGBA ? !memcmp(o, r, 4) :
                               fmt == QD_ARGB ? (o[0] == r[3] && !memcmp(o + 1, r, 3)) : !memcmp(o, r, 3);
                    if (!same) {
                        printf("FAIL %s: %s loop, layout %d, pixel %lu\n", name, use ? "68020" : "C", fmt, i);
                        bad++;
                        break;
                    }
                }
                free(out);
            }
        if (ref) free(ref);
    }
    printf("%d files, %d refused by both, %d failures (%s)\n", files, refused, bad,
           has020 ? "68020 and C loops" : "C loop only");
    return bad ? 10 : 0;
}
