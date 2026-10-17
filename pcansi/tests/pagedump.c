/* pagedump: ansi.c on the PC, the page as text (for comparing with
 * Charcoal's reader): "cols rows", then a line a row of cells, each
 * character (2 hex digits), foreground and background (1 each).
 *   cc -O2 -o pagedump pagedump.c ../src/ansi.c && ./pagedump FILE */
#include <stdio.h>
#include <stdlib.h>
#include "../src/ansi.h"

static void *al(unsigned long n) { return malloc(n); }
static void fr(void *p) { free(p); }

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *b;
    long n, end, y, x;
    AnsiSauce s;
    AnsiPage p;
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) return 1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(n + 1);
    if (fread(b, 1, n, f) != (size_t)n) return 1;
    fclose(f);
    end = ansi_sauce(b, n, &s);
    if (!ansi_read(b, end, &s, ANSI_MAXROWS, al, fr, &p)) return 2;
    printf("%d %d\n", p.cols, p.rows);
    for (y = 0; y < p.rows; y++) {
        for (x = 0; x < p.cols; x++) {
            long i = y * p.cols + x;
            printf("%02x%x%x", p.ch[i], p.at[i] & 15, p.at[i] >> 4);
        }
        putchar('\n');
    }
    if (argc > 2)
        fprintf(stderr, "sauce %d title '%s' by '%s/%s' font '%s' type %d/%d w %d h %d flags %x pc %d cut %d\n",
                s.present, s.title, s.author, s.group, s.font, s.datatype, s.filetype, s.tinfo1, s.tinfo2,
                s.flags, ansi_sauce_pc(&s), p.cut);
    ansi_free(&p, fr);
    free(b);
    return 0;
}
