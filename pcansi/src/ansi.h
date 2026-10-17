/* ansi.c: PC ANSI art read into a page of cells, as ANSI.SYS and the art
 * viewers of the scene show it. Plain C, no Amiga calls: the class and
 * the PC test (tests/pagedump.c) build the same file. */
#ifndef ANSI_H
#define ANSI_H

/* the most rows a page gets (long Amiga ASCII collections run to 9,500) */
#define ANSI_MAXROWS 12000
#define ANSI_MAXCOLS 1000

typedef struct {
    int present;                /* the file has a SAUCE record */
    char title[36], author[21], group[21];
    char font[23];              /* TInfoS: "IBM VGA", "Amiga Topaz 1+" ... */
    unsigned char datatype, filetype, flags, comments;
    unsigned short tinfo1, tinfo2;
} AnsiSauce;

typedef struct {
    int cols, rows;             /* the page */
    unsigned char *ch;          /* cols * rows characters (CP437) */
    unsigned char *at;          /* their colours: foreground | background << 4 */
    unsigned char *st;          /* their styles (ANSI_ITALIC, ANSI_UNDERLINE), or NULL:
                                 * made at the first italic or underline */
    int cut;                    /* rows past ANSI_MAXROWS or memory were left out */
    int fullest;                /* an animation: its fullest picture, not its empty end */
    long drawn;                 /* characters drawn */
    long over;                  /* of them, drawn over a cell that held something */
    int wipes;                  /* clear screens after something was drawn */
} AnsiPage;

/* memory for the page: the class gives exec's, the test malloc */
typedef void *(*AnsiAlloc)(unsigned long size);
typedef void (*AnsiFree)(void *p);

#define ANSI_ITALIC    1
#define ANSI_UNDERLINE 2

/* how to read the bytes: as a PC (CP437, ANSI.SYS) or as an Amiga
 * (Latin-1, 0x9B is CSI, form feed clears, no glyphs for controls) */
#define ANSI_PC    0
#define ANSI_AMIGA 1

/* the SAUCE record of a whole file; returns where the art ends */
long ansi_sauce(const unsigned char *b, long n, AnsiSauce *s);

/* the art (the file up to ansi_sauce's end) onto a page. maxrows: no more
 * rows than this (memory). Returns 0 out of memory. */
int ansi_read(const unsigned char *art, long n, const AnsiSauce *s, int mode, long maxrows,
              AnsiAlloc alloc, AnsiFree release, AnsiPage *p);
void ansi_free(AnsiPage *p, AnsiFree release);

/* the player: the art fed a few bytes at a time into a page of its own,
 * as a caller saw it arrive. ansi_play_step() takes up to bytes more and
 * returns how many are left (0: the end); ansi_play_dirty() gives the rows
 * changed since it was last asked (to < from: none). */
typedef struct AnsiPlayer AnsiPlayer;
AnsiPlayer *ansi_play_new(const unsigned char *art, long n, const AnsiSauce *s, int mode, long maxrows,
                          AnsiAlloc alloc, AnsiFree release);
long ansi_play_step(AnsiPlayer *pl, long bytes);
AnsiPage *ansi_play_page(AnsiPlayer *pl);
void ansi_play_dirty(AnsiPlayer *pl, long *from, long *to);
void ansi_play_free(AnsiPlayer *pl, AnsiFree release);

/* is the page an animation (to be played rather than shown at once)? it
 * cleared the screen after drawing, or drew over at least 16 cells that
 * held something (and at least 1% of what it drew) */
#define ANSI_ANIMATED(p) ((p)->wipes > 0 || ((p)->over >= 16 && (p)->over * 100 >= (p)->drawn))

/* the 16 colours as 0xRRGGBB: the PC's (VGA), and the Amiga ANSI
 * terminals' (yellow, not brown) */
extern const unsigned long ansi_rgb[16];
extern const unsigned long ansi_amiga_rgb[16];

/* does the record say PC character art? (an Amiga font name is not PC) */
int ansi_sauce_pc(const AnsiSauce *s);

#endif
