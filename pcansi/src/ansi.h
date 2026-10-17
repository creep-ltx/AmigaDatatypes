/* ansi.c: PC ANSI art read into a page of cells, as ANSI.SYS and the art
 * viewers of the scene show it. Plain C, no Amiga calls: the class and
 * the PC test (tests/pagedump.c) build the same file. */
#ifndef ANSI_H
#define ANSI_H

/* the most rows a page gets: a struct Line's YOffset is 16 bits, so with
 * the 16-row font 4095 rows is the end anyway */
#define ANSI_MAXROWS 4000
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
    int cut;                    /* rows past ANSI_MAXROWS or memory were left out */
} AnsiPage;

/* memory for the page: the class gives exec's, the test malloc */
typedef void *(*AnsiAlloc)(unsigned long size);
typedef void (*AnsiFree)(void *p);

/* the SAUCE record of a whole file; returns where the art ends */
long ansi_sauce(const unsigned char *b, long n, AnsiSauce *s);

/* the art (the file up to ansi_sauce's end) onto a page. maxrows: no more
 * rows than this (memory). Returns 0 out of memory. */
int ansi_read(const unsigned char *art, long n, const AnsiSauce *s, long maxrows,
              AnsiAlloc alloc, AnsiFree release, AnsiPage *p);
void ansi_free(AnsiPage *p, AnsiFree release);

/* the 16 colours as 0xRRGGBB */
extern const unsigned long ansi_rgb[16];

/* does the record say PC character art? (an Amiga font name is not PC) */
int ansi_sauce_pc(const AnsiSauce *s);

#endif
