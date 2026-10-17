/* pagedump: ansi.c on the PC, the page as text (for comparing with
 * Charcoal's reader): "cols rows", then a line a row of cells, each
 * character (2 hex digits), foreground, background and style (1 each:
 * 1 italic, 2 underline). -a: read as Amiga ANSI.
 *   cc -O2 -o pagedump pagedump.c ../src/ansi.c && ./pagedump [-a] FILE */
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
    int mode = ANSI_PC, play = 0;
    while (argc > 1 && argv[1][0] == '-') {
        if (argv[1][1] == 'a') mode = ANSI_AMIGA;
        if (argv[1][1] == 'p') play = 1;        /* through the player, 58 bytes a step */
        argv++;
        argc--;
    }
    if (argc < 2 || !(f = fopen(argv[1], "rb"))) return 1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(n + 1);
    if (fread(b, 1, n, f) != (size_t)n) return 1;
    fclose(f);
    end = ansi_sauce(b, n, &s);
    if (play) {
        AnsiPlayer *pl = ansi_play_new(b, end, &s, mode, ANSI_MAXROWS, al, fr);
        long d0, d1, steps = 0;
        if (!pl) return 2;
        while (ansi_play_step(pl, 58)) {
            ansi_play_dirty(pl, &d0, &d1);
            steps++;
        }
        p = *ansi_play_page(pl);
        printf("%d %d\n", p.cols, p.rows);
        for (y = 0; y < p.rows; y++) {
            for (x = 0; x < p.cols; x++) {
                long i = y * p.cols + x;
                printf("%02x%x%x%x", p.ch[i], p.at[i] & 15, p.at[i] >> 4, p.st ? p.st[i] : 0);
            }
            putchar('\n');
        }
        fprintf(stderr, "steps %ld\n", steps);
        ansi_play_free(pl, fr);
        free(b);
        return 0;
    }
    if (!ansi_read(b, end, &s, mode, ANSI_MAXROWS, al, fr, &p)) return 2;
    printf("%d %d\n", p.cols, p.rows);
    for (y = 0; y < p.rows; y++) {
        for (x = 0; x < p.cols; x++) {
            long i = y * p.cols + x;
            printf("%02x%x%x%x", p.ch[i], p.at[i] & 15, p.at[i] >> 4, p.st ? p.st[i] : 0);
        }
        putchar('\n');
    }
    if (argc > 2)
        fprintf(stderr, "sauce %d title '%s' by '%s/%s' font '%s' type %d/%d w %d h %d flags %x pc %d cut %d\n",
                s.present, s.title, s.author, s.group, s.font, s.datatype, s.filetype, s.tinfo1, s.tinfo2,
                s.flags, ansi_sauce_pc(&s), p.cut);
    if (getenv("ANIMSTATS"))
        fprintf(stderr, "anim drawn %ld over %ld wipes %d fullest %d\n", p.drawn, p.over, p.wipes, p.fullest);
    ansi_free(&p, fr);
    free(b);
    return 0;
}
