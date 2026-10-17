/* pcansi.datatype: PC ANSI art (.ans, BBS screens, .nfo) shown as the
 * artists drew it - CP437 block and line characters in the IBM VGA font,
 * the sixteen PC colours - in MultiView and every other program that
 * shows text through datatypes. The art stays text: no picture is made.
 *
 * Built with -DAMIANSI it is amiansi.datatype (AmigaDatatypes/amiansi):
 * Amiga ANSI art read with the Amiga's rules, in the font its SAUCE
 * record names (Topaz 1/1+/2/2+, MicroKnight, P0T-NOoDLE, mO'sOul; Topaz
 * 1+ without one), with italic and underline, on black. The Makefile
 * gives the datatype's version.h with -include.
 *
 * A subclass of text.datatype, as ascii.datatype is: text.datatype loads
 * the file into TDTA_Buffer and draws a list of struct Line pieces. This
 * class reads the file as an ANSI terminal would (ansi.c) into a page of
 * cells, and makes each row's runs of one colour into pieces that point
 * into the page. The font is FONTS:ibmvga.font, 16 rows on a screen with
 * square pixels, 8 rows where pixels are twice as tall as wide (640x256):
 * then the art has the proportions it had on a PC. The colours are pens
 * the class obtains on the screen, the nearest it has to the PC's.
 *
 * The library: the Resident tag, open/close/expunge and the jump table are
 * written by hand (as markdown.datatype's), with ObtainEngine() as the one
 * call of a datatype class. No C library. Runs on a 68000 and Kickstart
 * 3.0 (datatypes.library 39). */
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <graphics/displayinfo.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/textclass.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/datatypes.h>
#include <proto/diskfont.h>
#include "ansi.h"

#define REG(r, a) a __asm(#r)

/* make DEBUG=1: a trace of the methods through RawPutChar (wasabi debug) */
#ifdef DEBUG
#include <stdarg.h>
void rawputch(void);
static void dbg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    RawDoFmt((STRPTR)fmt, (APTR)ap, (void (*)())rawputch, NULL);
    va_end(ap);
}
#define D(x) dbg x
#else
#define D(x)
#endif

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *DataTypesBase;
struct Library *DiskfontBase;           /* opened at the first file */
static struct Library *TextDTBase;      /* text.datatype, the superclass */
static Class *anclass;
static struct SignalSemaphore anlock;   /* opening diskfont.library */

struct AnBase {
    struct Library lib;
    BPTR seglist;
};

static const char libname[] = AN_NAME;
static const char libid[] = AN_NAME " " AN_VERSTR " (" AN_DATE ")\r\n";
const char verstag[] = "$VER: " AN_NAME " " AN_VERSTR " (" AN_DATE ")";

#ifdef AMIANSI
#define MODE ANSI_AMIGA
#define PALETTE ansi_amiga_rgb
/* the fonts by the names SAUCE gives them; the first is the default */
static const char *const fontnames[][2] = {
    { "Amiga Topaz 1+", "ansi_topaz1p.font" },
    { "Amiga Topaz 1", "ansi_topaz1.font" },
    { "Amiga Topaz 2+", "ansi_topaz2p.font" },
    { "Amiga Topaz 2", "ansi_topaz2.font" },
    { "Amiga MicroKnight+", "ansi_mkp.font" },
    { "Amiga MicroKnight", "ansi_mk.font" },
    { "Amiga P0T-NOoDLE", "ansi_noodle.font" },
    { "Amiga mOsOul", "ansi_mosoul.font" },
    { "Amiga mO'sOul", "ansi_mosoul.font" },
    { NULL, NULL }
};
#define fontname (fontnames[0][1])
#else
#define MODE ANSI_PC
#define PALETTE ansi_rgb
static const char fontname[] = "ibmvga.font";
#endif

/* ---- an object's own data ---------------------------------------------- */

struct AnData {
    APTR pool;                  /* the pieces (struct Line) */
    AnsiPage page;              /* the art as cells; the pieces point into it */
    AnsiSauce sauce;
    int ice;                    /* blink means a bright background */
    int built;                  /* the pieces are made */
    int ownrender;              /* text.datatype draws in its own font (V40): we draw */
    long msgrow;                /* the row that says memory ran out, or -1 */
    long total, widest;
    struct TextFont *font;      /* ours (ibmvga, an Amiga font), or NULL: then the text font */
    struct ColorMap *cm;        /* where the pens are from */
    LONG pens[16];              /* obtained (-1: none) */
    LONG drawpens[16];          /* drawn with */
    char title[96];
    /* playing an animation (play.c's process feeds the player) */
    const UBYTE *art;           /* the art in text.datatype's buffer */
    long artlen;
    long baud;                  /* ENV:ANSIBaud, 14400 if not set; 0: never play */
    AnsiPlayer *pl;             /* while it plays */
    struct SignalSemaphore lock;    /* the player and its rows to draw */
    struct Process *proc;       /* the player's process, NULL when it ended */
    volatile int stop;          /* the process is asked to end */
    long d0, d1;                /* rows the player changed, not yet drawn */
    Object *self;
    struct Window *win;         /* where the object is, for the process's redraws */
    struct Requester *req;
    int playnow;                /* start playing when the layout is done */
};

/* GM_RENDER's gpr_Redraw from the player's process: draw what changed */
#define GREDRAW_PLAY 0x41

/* ---- calling methods without amiga.lib --------------------------------- */

static void newlist(struct List *l)
{
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
}

static ULONG domethod(Object *o, Msg m)
{
    return CallHookPkt((struct Hook *)OCLASS(o), o, m);
}

static ULONG dosuper(Class *cl, Object *o, Msg m)
{
    return CallHookPkt((struct Hook *)cl->cl_Super, o, m);
}

static ULONG getattrs(Object *o, struct TagItem *t)
{
    return GetDTAttrsA(o, t);
}

/* tell the application (MultiView) what changed */
static void notify(Object *o, struct GadgetInfo *gi, struct TagItem *tags)
{
    struct opUpdate m;
    m.MethodID = OM_NOTIFY;
    m.opu_AttrList = tags;
    m.opu_GInfo = gi;
    m.opu_Flags = 0;
    domethod(o, (Msg)&m);
}

static void busy(Object *o, struct GadgetInfo *gi)
{
    struct TagItem t[3];
    t[0].ti_Tag = GA_ID;    t[0].ti_Data = ((struct Gadget *)o)->GadgetID;
    t[1].ti_Tag = DTA_Busy; t[1].ti_Data = TRUE;
    t[2].ti_Tag = TAG_DONE;
    notify(o, gi, t);
}

static void *pagealloc(unsigned long n)
{
    return AllocVec(n, MEMF_ANY);
}

static void pagefree(void *p)
{
    FreeVec(p);
}

/* ---- the font ----------------------------------------------------------- */

/* 8 rows where a pixel is about twice as tall as it is wide (640x256,
 * 640x200): VGA's 8x16 characters looked so on a PC's 640x400 screen.
 * 16 rows on a screen with square pixels (interlaced, RTG). */
static UWORD fontsize(void)
{
    struct Screen *s = LockPubScreen(NULL);
    struct DisplayInfo di;
    UWORD size = 16;
    ULONG id;
    if (!s) return size;
    id = GetVPModeID(&s->ViewPort);
    if (id != INVALID_ID && GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di), DTAG_DISP, id) &&
        di.Resolution.y >= 2 * di.Resolution.x - di.Resolution.x / 4)
        size = 8;
    else if (s->Height < 400 && s->Width >= 2 * s->Height)
        size = 8;
    UnlockPubScreen(NULL, s);
    return size;
}

static struct TextFont *openfont(const char *name, UWORD size)
{
    struct TextAttr ta;
    struct TextFont *f = NULL;
    ObtainSemaphore(&anlock);
    if (!DiskfontBase) DiskfontBase = OpenLibrary("diskfont.library", 36);
    ReleaseSemaphore(&anlock);
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = size ? size : fontsize();
    ta.ta_Style = 0;
    ta.ta_Flags = FPF_DISKFONT | FPF_DESIGNED;
    if (DiskfontBase) f = OpenDiskFont(&ta);
    /* a squeezed other size is no use for a grid of 256 characters */
    if (f && (f->tf_XSize != 8 || f->tf_LoChar != 0 || f->tf_HiChar != 255)) {
        CloseFont(f);
        f = NULL;
    }
    return f;
}

/* ---- the colours ------------------------------------------------------ */

/* a character whose foreground is not seen (a blank, not underlined), or
 * whose background is not (the PC's full block) */
#ifdef AMIANSI
#define NOFG(c, st) (((c) == ' ' || (c) == 0xA0) && !((st) & ANSI_UNDERLINE))
#define NOBG(c) 0
#else
#define NOFG(c, st) (((c) == ' ' || (c) == 0 || (c) == 0xFF) && !((st) & ANSI_UNDERLINE))
#define NOBG(c) ((c) == 0xDB)
#endif

static ULONG c32(ULONG c)
{
    return c << 24 | c << 16 | c << 8 | c;
}

/* how far apart two colours look: brightness counts three times as much
 * as hue, so dark grey goes to black or grey, never to brown */
static long apart(ULONG a, ULONG b)
{
    long ra = a >> 16 & 255, ga = a >> 8 & 255, ba = a & 255;
    long rb = b >> 16 & 255, gb = b >> 8 & 255, bb = b & 255;
    long ya = (299 * ra + 587 * ga + 114 * ba) / 1000, yb = (299 * rb + 587 * gb + 114 * bb) / 1000;
    long dy = ya - yb, du = (ba - ya) - (bb - yb), dv = (ra - ya) - (rb - yb);
    return 3 * dy * dy + du * du + dv * dv;
}

/* the pens: the colours the art uses most get a pen of their exact colour
 * first (one the screen has, or a free one set to it); when the screen has
 * no more, the rest are drawn with the screen's nearest colour */
static void getpens(struct AnData *an, struct Screen *scr)
{
    struct TagItem t[3];
    AnsiPage *p = &an->page;
    ULONG count[16], *rgb;
    long i, n, k, best, d, bd;
    int order[16], c, depth, j;
    if (an->cm || !scr) return;
    an->cm = scr->ViewPort.ColorMap;
    for (i = 0; i < 16; i++) {
        count[i] = 0;
        order[i] = i;
        an->pens[i] = -1;
        an->drawpens[i] = -1;
    }
    for (i = 0, n = (long)p->cols * p->rows; i < n; i++) {
        UBYTE ch = p->ch[i], a = p->at[i];
        if (!NOFG(ch, p->st ? p->st[i] : 0)) count[a & 15]++;
        if (!NOBG(ch)) count[an->ice ? a >> 4 : a >> 4 & 7]++;
    }
    count[0]++;                                 /* the window around the art */
    count[15]++;                                /* the out-of-memory line */
    for (i = 1; i < 16; i++)                    /* most used first */
        for (j = i; j > 0 && count[order[j]] > count[order[j - 1]]; j--) {
            c = order[j];
            order[j] = order[j - 1];
            order[j - 1] = c;
        }
    t[0].ti_Tag = OBP_Precision; t[0].ti_Data = PRECISION_EXACT;
    t[1].ti_Tag = OBP_FailIfBad; t[1].ti_Data = TRUE;
    t[2].ti_Tag = TAG_DONE;
    for (i = 0; i < 16 && count[order[i]]; i++) {
        ULONG v = PALETTE[order[i]];
        an->pens[order[i]] = an->drawpens[order[i]] =
            ObtainBestPenA(an->cm, c32(v >> 16 & 255), c32(v >> 8 & 255), c32(v & 255), t);
    }
    /* the rest: the nearest of the screen's colours, drawn with as it is */
    depth = scr->RastPort.BitMap->Depth;
    n = depth >= 8 ? 256 : 1L << depth;
    if (an->cm->Count < n) n = an->cm->Count;
    if ((rgb = AllocVec(n * 3 * sizeof(ULONG), MEMF_ANY))) {
        GetRGB32(an->cm, 0, n, rgb);
        for (c = 0; c < 16; c++) {
            if (an->drawpens[c] >= 0) continue;
            best = 0;
            bd = -1;
            for (k = 0; k < n; k++) {
                d = apart(PALETTE[c], (rgb[k * 3] >> 24) << 16 | (rgb[k * 3 + 1] >> 24) << 8 | rgb[k * 3 + 2] >> 24);
                if (bd < 0 || d < bd) {
                    bd = d;
                    best = k;
                }
            }
            an->drawpens[c] = best;
        }
        FreeVec(rgb);
    }
    for (c = 0; c < 16; c++) {
        if (an->drawpens[c] < 0) an->drawpens[c] = c ? 1 : 0;
        D(("ansi: colour %ld (%ld cells) -> pen %ld%s\n", (long)c, count[c], an->drawpens[c],
           an->pens[c] >= 0 ? "" : " (nearest)"));
    }
}

static void freepens(struct AnData *an)
{
    int i;
    if (!an->cm) return;
    for (i = 0; i < 16; i++)
        if (an->pens[i] >= 0) ReleasePen(an->cm, an->pens[i]);
    an->cm = NULL;
}

/* the pen for colour fg on bg: on a screen with few colours two colours
 * can get the same pen, and the characters would vanish; then white on a
 * dark background, black on a light one */
static LONG pen(struct AnData *an, int fg, int bg)
{
    if (fg == bg || an->drawpens[fg] != an->drawpens[bg]) return an->drawpens[fg];
    return an->drawpens[(bg == 7 || bg > 8) ? 0 : 15];
}

/* ---- the page into pieces --------------------------------------------- */

/* where the run of cells from i (of n) that look the same in one pair of
 * pens and one style ends; its colours and style in *fg, *bg, *style (a
 * space takes any foreground, a full block any background) */
static long runend(struct AnData *an, const UBYTE *ch, const UBYTE *at, const UBYTE *st, long i, long n,
                   int *fg, int *bg, ULONG *style)
{
    int f = -1, b = -1, cf, cb, s0 = st ? st[i] : 0;
    long j;
    for (j = i; j < n; j++) {
        if (st && st[j] != s0) break;
        cf = at[j] & 15;
        cb = at[j] >> 4;
        if (!an->ice) cb &= 7;                  /* blink: shown steady */
        if (!NOFG(ch[j], s0)) {
            if (f < 0) f = cf;
            else if (f != cf) break;
        }
        if (!NOBG(ch[j])) {
            if (b < 0) b = cb;
            else if (b != cb) break;
        }
    }
    *fg = f < 0 ? 7 : f;
    *bg = b < 0 ? 0 : b;
    *style = ((s0 & ANSI_ITALIC) ? FSF_ITALIC : 0) | ((s0 & ANSI_UNDERLINE) ? FSF_UNDERLINED : 0);
    return j;
}


static struct Line *piece(APTR pool, UBYTE *s, long n, LONG fg, LONG bg, ULONG style, struct TextFont *f,
                          long x, long y, int lf)
{
    struct Line *ln = AllocPooled(pool, sizeof(struct Line));
    if (!ln) return NULL;
    ln->ln_Text = s;
    ln->ln_TextLen = n;
    ln->ln_XOffset = x;
    ln->ln_YOffset = y;
    ln->ln_Width = n * f->tf_XSize;
    ln->ln_Height = f->tf_YSize;
    ln->ln_Flags = lf ? LNF_LF : 0;
    ln->ln_FgPen = fg;
    ln->ln_BgPen = bg;
    ln->ln_Style = style;
    ln->ln_Data = NULL;
    return ln;
}

/* one row of pieces: one for each run of cells that look the same */
static int row(struct AnData *an, struct List *lines, struct TextFont *f, long r)
{
    AnsiPage *p = &an->page;
    long o = r * p->cols;
    UBYTE *ch = p->ch + o, *at = p->at + o, *st = p->st ? p->st + o : NULL;
    long y = r * f->tf_YSize + f->tf_Baseline, x = 0, i = 0, j, n = p->cols;
    struct Line *ln;
    int fg, bg;
    ULONG style;
    while (i < n) {
        j = runend(an, ch, at, st, i, n, &fg, &bg, &style);
        if (!(ln = piece(an->pool, ch + i, j - i, pen(an, fg, bg), an->drawpens[bg], style, f, x, y, j == n)))
            return 0;
        AddTail(lines, (struct Node *)ln);
        x += ln->ln_Width;
        i = j;
    }
    return 1;
}

static const char nomem[] = "[ Not enough memory to show the rest of this file ]";

/* the line that says the rest did not fit */
static void toofull(struct AnData *an, struct List *lines, struct TextFont *f, long r)
{
    an->msgrow = r;
    struct Line *ln = piece(an->pool, (UBYTE *)nomem, sizeof(nomem) - 1, an->drawpens[15], an->drawpens[0], 0, f, 0,
                            r * f->tf_YSize + f->tf_Baseline, 1);
    if (ln) AddTail(lines, (struct Node *)ln);
}

/* the pieces for the whole page, for text.datatype to draw (and mark and
 * copy); returns the rows made, or -1 when they would not leave RESERVE
 * free: then the class draws the art itself and needs no pieces. A piece
 * takes some 40 bytes: a big, colourful file has 15 or more a row */
#define RESERVE (192 * 1024L)

static long build(struct AnData *an, struct List *lines, struct TextFont *f)
{
    long r, rows = an->page.rows;
    an->msgrow = -1;
    /* a 16-bit YOffset: no rows past it */
    if (rows > 65000L / f->tf_YSize) return -1;
    for (r = 0; r < rows; r++)
        if (((r & 31) == 0 && AvailMem(MEMF_ANY) < RESERVE) || !row(an, lines, f, r)) {
            D(("ansi: no room for pieces at row %ld, free %ld\n", r, AvailMem(MEMF_ANY)));
            return -1;
        }
    if (an->page.cut) toofull(an, lines, f, r++);
    return r;
}

/* the art, read from text.datatype's buffer */
static int readpage(Object *o, struct AnData *an, struct TextFont *f)
{
    struct TagItem t[3];
    UBYTE *text = NULL;
    long len = 0, end, maxrows;
    t[0].ti_Tag = TDTA_Buffer;    t[0].ti_Data = (ULONG)&text;
    t[1].ti_Tag = TDTA_BufferLen; t[1].ti_Data = (ULONG)&len;
    t[2].ti_Tag = TAG_DONE;
    getattrs(o, t);
    if (!text || len < 0) return 0;
    end = ansi_sauce(text, len, &an->sauce);
    an->ice = an->sauce.present && (an->sauce.flags & 1);
    an->art = text;
    an->artlen = end;
    /* the page may take half the largest free block */
    maxrows = AvailMem(MEMF_ANY | MEMF_LARGEST) / 2 / (2 * (an->sauce.present && an->sauce.datatype == 1 &&
                                                             an->sauce.tinfo1 >= 80 && an->sauce.tinfo1 <= ANSI_MAXCOLS
                                                             ? an->sauce.tinfo1 : 80));
#ifdef DEBUG
    {
        struct DateStamp a, b;
        int ok;
        DateStamp(&a);
        ok = ansi_read(text, end, &an->sauce, MODE, maxrows, pagealloc, pagefree, &an->page);
        DateStamp(&b);
        D(("ansi: ansi_read %ld ticks\n", (b.ds_Minute - a.ds_Minute) * 3000 + b.ds_Tick - a.ds_Tick));
        if (!ok) return 0;
    }
#else
    if (!ansi_read(text, end, &an->sauce, MODE, maxrows, pagealloc, pagefree, &an->page)) return 0;
#endif
    D(("ansi: %ld x %ld, sauce %ld ice %ld cut %ld\n", (long)an->page.cols, (long)an->page.rows,
       (long)an->sauce.present, (long)an->ice, (long)an->page.cut));
    return 1;
}

/* ---- playing an animation ------------------------------------------------ */

/* ENV:ANSIBaud: the speed animations play at, as a caller's modem saw them
 * (10 bits a character); 0 shows them still. 14400 if not set. */
static long getbaud(void)
{
    char b[16];
    long v = 0, i;
    if (GetVar("ANSIBaud", b, sizeof(b), GVF_GLOBAL_ONLY) <= 0) return 14400;
    for (i = 0; b[i] >= '0' && b[i] <= '9'; i++) v = v * 10 + b[i] - '0';
    return i ? v : 14400;
}

/* ask for a redraw from the player's process: through Intuition, which
 * gives the GadgetInfo (a method's own may not be kept) */
static void playdraw(struct AnData *an, ULONG how)
{
    struct gpRender g;
    g.MethodID = GM_RENDER;
    g.gpr_GInfo = NULL;
    g.gpr_RPort = NULL;
    g.gpr_Redraw = how;
    DoGadgetMethodA((struct Gadget *)an->self, an->win, an->req, (Msg)&g);
}

/* the player's process: as many bytes as the modem would have brought by
 * now (by the clock: a slow Amiga draws fewer steps, not slower), the rows
 * they changed drawn, until the end or until asked to stop; then the still
 * picture */
static long ticks(const struct DateStamp *a)
{
    struct DateStamp b;
    DateStamp(&b);
    return ((b.ds_Days - a->ds_Days) * 1440 + b.ds_Minute - a->ds_Minute) * 3000 + b.ds_Tick - a->ds_Tick;
}

static void playproc(void)
{
    struct AnData *an = FindTask(NULL)->tc_UserData;
    struct DateStamp t0;
    long cps = an->baud / 10, done = 0, want, left = 1, a, b;
    DateStamp(&t0);
    while (left && !an->stop) {
        want = ticks(&t0) * cps / 50;
        if (want - done > 4096) want = done + 4096;     /* (a step at a time on a very slow one) */
        if (want > done) {
            ObtainSemaphore(&an->lock);
            left = ansi_play_step(an->pl, want - done);
            ansi_play_dirty(an->pl, &a, &b);
            if (a < an->d0) an->d0 = a;
            if (b > an->d1) an->d1 = b;
            ReleaseSemaphore(&an->lock);
            done = want;
            if (an->d1 >= an->d0 && !an->stop) playdraw(an, GREDRAW_PLAY);
        }
        if (left) Delay(1);
    }
    ObtainSemaphore(&an->lock);
    ansi_play_free(an->pl, pagefree);
    an->pl = NULL;
    ReleaseSemaphore(&an->lock);
    if (!an->stop) playdraw(an, GREDRAW_REDRAW);    /* hold: the still picture */
    Forbid();                                   /* gone before anyone runs again */
    an->proc = NULL;
}

static void playstart(struct AnData *an)
{
    struct TagItem t[5];
    if (!(an->pl = ansi_play_new(an->art, an->artlen, &an->sauce, MODE, an->page.rows + 2 * 25, pagealloc,
                                 pagefree)))
        return;
    an->d0 = 0x7FFFFFFF;
    an->d1 = -1;
    an->stop = 0;
    t[0].ti_Tag = NP_Entry;     t[0].ti_Data = (ULONG)playproc;
    t[1].ti_Tag = NP_Name;      t[1].ti_Data = (ULONG)AN_NAME " player";
    t[2].ti_Tag = NP_StackSize; t[2].ti_Data = 8192;
    t[3].ti_Tag = NP_Priority;  t[3].ti_Data = 0;
    t[4].ti_Tag = TAG_DONE;
    Forbid();                                   /* it starts once it knows its object */
    if ((an->proc = CreateNewProc(t))) an->proc->pr_Task.tc_UserData = an;
    Permit();
    if (!an->proc) {
        ansi_play_free(an->pl, pagefree);
        an->pl = NULL;
    }
    D(("ansi: playing at %ld baud: %lx\n", an->baud, an->proc));
}

/* stop the player and wait until its process has ended */
static void playstop(struct AnData *an)
{
    if (!an->proc) return;
    an->stop = 1;
    while (an->proc) Delay(1);
}

/* Amiga ANSI: the font the record names, when it is not the default
 * text.datatype was given; 1 if this class must draw (it has the font) */
static int otherfont(struct AnData *an)
{
#ifdef AMIANSI
    int i, k;
    struct TextFont *f;
    for (i = 1; fontnames[i][0]; i++) {
        const char *a = fontnames[i][0], *b = an->sauce.font;
        for (k = 0; a[k] && a[k] == b[k]; k++) ;
        if (!a[k] && !b[k]) break;
    }
    if (!an->sauce.present || !fontnames[i][0] || !an->font) return 0;
    if (!(f = openfont(fontnames[i][1], an->font->tf_YSize))) return 0;
    CloseFont(an->font);
    an->font = f;
    D(("ansi: SAUCE font %s\n", fontnames[i][1]));
    return 1;
#else
    (void)an;
    return 0;
#endif
}

/* text.datatype 45 and later (3.1.4, 3.2) loses the underline of a piece
 * that starts 256 pixels or more from the left: then this class draws */
static int badunderline(struct AnData *an)
{
    AnsiPage *p = &an->page;
    long i, n = (long)p->cols * p->rows;
    if (!p->st || TextDTBase->lib_Version < 45) return 0;
    for (i = 0; i < n; i++)
        if (p->st[i] & ANSI_UNDERLINE) return 1;
    return 0;
}

/* "Title by Author/Group" from the record */
static void maketitle(struct AnData *an)
{
    char *d = an->title, *e = an->title + sizeof(an->title) - 1;
    const char *s;
    AnsiSauce *sc = &an->sauce;
    an->title[0] = 0;
    if (!sc->present || !sc->title[0]) return;
    for (s = sc->title; *s && d < e; ) *d++ = *s++;
    if (sc->author[0] || sc->group[0])
        for (s = " by "; *s && d < e; ) *d++ = *s++;
    for (s = sc->author; *s && d < e; ) *d++ = *s++;
    if (sc->author[0] && sc->group[0] && d < e) *d++ = '/';
    for (s = sc->group; *s && d < e; ) *d++ = *s++;
    *d = 0;
}

static ULONG layout(Class *cl, Object *o, struct gpLayout *gpl)
{
    struct AnData *an = INST_DATA(cl, o);
    struct DTSpecialInfo *si = (struct DTSpecialInfo *)((struct Gadget *)o)->SpecialInfo;
    struct TextFont *font = NULL;
    struct IBox *domain = NULL;
    struct List *lines = NULL;
    STRPTR title = NULL;
    struct TagItem t[5], n[14];
    long total, widest, hunit, vis;

    t[0].ti_Tag = DTA_TextFont;  t[0].ti_Data = (ULONG)&font;
    t[1].ti_Tag = DTA_Domain;    t[1].ti_Data = (ULONG)&domain;
    t[2].ti_Tag = DTA_ObjName;   t[2].ti_Data = (ULONG)&title;
    t[3].ti_Tag = TDTA_LineList; t[3].ti_Data = (ULONG)&lines;
    t[4].ti_Tag = TAG_DONE;
    if (getattrs(o, t) < 4 || !font || !domain || !lines) return 0;
    D(("ansi: layout %lx initial %ld, text font %s/%ld, ours %lx\n", o, gpl->gpl_Initial,
       font->tf_Message.mn_Node.ln_Name, (long)font->tf_YSize, an->font));
    /* text.datatype 40 (OS 3.1) keeps its own font whatever it is told:
     * then this class draws the art itself */
    if (!an->built) an->ownrender = an->font && font != an->font;
    if (an->font) font = an->font;

    ObtainSemaphore(&si->si_Lock);
    hunit = font->tf_XSize ? font->tf_XSize : 8;
    if (!an->built) {
        newlist(lines);
        if (an->pool) DeletePool(an->pool);
        an->pool = CreatePool(MEMF_ANY | MEMF_PUBLIC, 16384, 8192);
        total = 0;
#ifdef DEBUG
        struct DateStamp t0, t1, t2;
        DateStamp(&t0);
#endif
        if (an->pool && (an->page.ch || readpage(o, an, font))) {
            if (otherfont(an) || badunderline(an)) an->ownrender = 1;
            /* an animation plays: this class draws */
            an->baud = getbaud();
            D(("ansi: animated %ld (wipes %ld over %ld drawn %ld) ginfo %lx win %lx\n", (long)ANSI_ANIMATED(&an->page),
               (long)an->page.wipes, an->page.over, an->page.drawn, gpl->gpl_GInfo,
               gpl->gpl_GInfo ? gpl->gpl_GInfo->gi_Window : NULL));
            if (an->baud > 0 && ANSI_ANIMATED(&an->page) && gpl->gpl_GInfo && gpl->gpl_GInfo->gi_Window) {
                an->ownrender = 1;
                an->win = gpl->gpl_GInfo->gi_Window;
                an->req = gpl->gpl_GInfo->gi_Requester;
                an->playnow = 1;
            }
            if (an->font) font = an->font;
            getpens(an, gpl->gpl_GInfo ? gpl->gpl_GInfo->gi_Screen : NULL);
        }
#ifdef DEBUG
        DateStamp(&t1);
#endif
        if (an->pool && an->cm && an->page.ch) {
            total = an->ownrender ? -1 : build(an, lines, font);
            if (total < 0) {
                /* no pieces: this class draws */
                newlist(lines);
                DeletePool(an->pool);
                an->pool = NULL;
                an->ownrender = 1;
                total = an->page.rows;
                an->msgrow = an->page.cut ? total++ : -1;
            }
            an->built = 1;
        }
#ifdef DEBUG
        DateStamp(&t2);
        D(("ansi: read+pens %ld ticks, pieces %ld ticks (own %ld)\n",
           (t1.ds_Minute - t0.ds_Minute) * 3000 + t1.ds_Tick - t0.ds_Tick,
           (t2.ds_Minute - t1.ds_Minute) * 3000 + t2.ds_Tick - t1.ds_Tick, (long)an->ownrender));
#endif
        an->total = total;
        an->widest = an->page.cols * hunit;
        maketitle(an);
    }
    total = an->total;
    widest = an->widest;
    si->si_VertUnit = font->tf_YSize;
    si->si_VisVert = vis = domain->Height / font->tf_YSize;
    si->si_TotVert = total;
    si->si_HorizUnit = hunit;
    si->si_VisHoriz = domain->Width / hunit;
    si->si_TotHoriz = (widest + hunit - 1) / hunit;
    ReleaseSemaphore(&si->si_Lock);

    n[0].ti_Tag = GA_ID;             n[0].ti_Data = ((struct Gadget *)o)->GadgetID;
    n[1].ti_Tag = DTA_VisibleVert;   n[1].ti_Data = vis;
    n[2].ti_Tag = DTA_TotalVert;     n[2].ti_Data = total;
    n[3].ti_Tag = DTA_NominalVert;   n[3].ti_Data = (total < 25 ? total : 25) * font->tf_YSize;
    n[4].ti_Tag = DTA_VertUnit;      n[4].ti_Data = font->tf_YSize;
    n[5].ti_Tag = DTA_VisibleHoriz;  n[5].ti_Data = domain->Width / hunit;
    n[6].ti_Tag = DTA_TotalHoriz;    n[6].ti_Data = (widest + hunit - 1) / hunit;
    n[7].ti_Tag = DTA_NominalHoriz;  n[7].ti_Data = widest;
    n[8].ti_Tag = DTA_HorizUnit;     n[8].ti_Data = hunit;
    n[9].ti_Tag = DTA_TopHoriz;      n[9].ti_Data = si->si_TopHoriz;
    n[10].ti_Tag = DTA_Title;        n[10].ti_Data = (ULONG)(an->title[0] ? (STRPTR)an->title : title);
    n[11].ti_Tag = DTA_Busy;         n[11].ti_Data = FALSE;
    n[12].ti_Tag = DTA_Sync;         n[12].ti_Data = TRUE;
    n[13].ti_Tag = TAG_DONE;
    notify(o, gpl->gpl_GInfo, n);
    if (an->playnow && an->built) {
        an->playnow = 0;
        playstart(an);
    }
    return total;
}

/* ---- the methods ----------------------------------------------------- */

static ULONG m_dispose(Class *cl, Object *o, Msg msg);

static ULONG m_new(Class *cl, Object *o, struct opSet *msg)
{
#ifdef DEBUG
    struct DateStamp n0, n1;
    DateStamp(&n0);
#endif
    struct TextFont *f = openfont(fontname, 0);
    struct TextAttr ta;
    struct TagItem t[3];
    struct opSet m;
    struct AnData *an;
    Object *obj;
    /* the font for text.datatype to draw with: ours, before the caller's */
    if (f) {
        ta.ta_Name = (STRPTR)fontname;
        ta.ta_YSize = f->tf_YSize;
        ta.ta_Style = 0;
        ta.ta_Flags = f->tf_Flags;
        t[0].ti_Tag = DTA_TextAttr; t[0].ti_Data = (ULONG)&ta;
        t[1].ti_Tag = TAG_MORE;     t[1].ti_Data = (ULONG)msg->ops_AttrList;
        m.MethodID = OM_NEW;
        m.ops_AttrList = t;
        m.ops_GInfo = msg->ops_GInfo;
        msg = &m;
    }
    obj = (Object *)dosuper(cl, o, (Msg)msg);
    if (!obj) {
        if (f) CloseFont(f);
        return 0;
    }
    an = INST_DATA(cl, obj);
    an->font = f;
    an->self = obj;
    InitSemaphore(&an->lock);
#ifdef DEBUG
    DateStamp(&n1);
    D(("ansi: new took %ld ticks\n", (n1.ds_Minute - n0.ds_Minute) * 3000 + n1.ds_Tick - n0.ds_Tick));
#endif
    D(("ansi: new %lx font %lx size %ld\n", obj, f, f ? (long)f->tf_YSize : 0L));
    return (ULONG)obj;
}

static ULONG m_dispose(Class *cl, Object *o, Msg msg)
{
    struct AnData *an = INST_DATA(cl, o);
    struct List *lines = NULL;
    struct TagItem t[2];
    ULONG r;
    t[0].ti_Tag = TDTA_LineList; t[0].ti_Data = (ULONG)&lines;
    t[1].ti_Tag = TAG_DONE;
    D(("ansi: dispose %lx\n", o));
    playstop(an);
    /* the pieces are ours: text.datatype must not free them */
    if (getattrs(o, t) && lines) newlist(lines);
    if (an->pool) DeletePool(an->pool);
    an->pool = NULL;
    ansi_free(&an->page, pagefree);
    freepens(an);
    r = dosuper(cl, o, msg);
    /* after the superclass: it may hold the font until then */
    if (an->font) CloseFont(an->font);
    an->font = NULL;
    return r;
}

static ULONG m_set(Class *cl, Object *o, struct opSet *msg)
{
    ULONG r = dosuper(cl, o, (Msg)msg);
    /* as ascii.datatype: a change shows at once, if we are the class */
    if (r && OCLASS(o) == cl) {
        struct RastPort *rp = ObtainGIRPort(msg->ops_GInfo);
        if (rp) {
            struct gpRender g;
            g.MethodID = GM_RENDER;
            g.gpr_GInfo = msg->ops_GInfo;
            g.gpr_RPort = rp;
            g.gpr_Redraw = GREDRAW_UPDATE;
            domethod(o, (Msg)&g);
            ReleaseGIRPort(rp);
        }
        r = 0;
    }
    return r;
}

/* one row of the art from column c0, n columns, at x, y (the top) */
static void drawrow(struct AnData *an, AnsiPage *p, struct RastPort *rp, long row, long c0, long n, long x, long y)
{
    struct TextFont *f = an->font;
    UBYTE *ch, *at, *st;
    long i, j;
    if (row >= p->rows) {                       /* (the player's page may be shorter) */
        SetAPen(rp, an->drawpens[0]);
        RectFill(rp, x, y, x + n * f->tf_XSize - 1, y + f->tf_YSize - 1);
        return;
    }
    if (row == an->msgrow && p == &an->page) {
        long k = sizeof(nomem) - 1 - c0;
        if (k > n) k = n;
        SetAPen(rp, an->drawpens[15]);
        SetBPen(rp, an->drawpens[0]);
        if (k > 0) {
            Move(rp, x, y + f->tf_Baseline);
            Text(rp, (STRPTR)nomem + c0, k);
        } else
            k = 0;
        SetAPen(rp, an->drawpens[0]);
        if (k < n) RectFill(rp, x + k * f->tf_XSize, y, x + n * f->tf_XSize - 1, y + f->tf_YSize - 1);
        return;
    }
    ch = p->ch + row * p->cols + c0;
    at = p->at + row * p->cols + c0;
    st = p->st ? p->st + row * p->cols + c0 : NULL;
    for (i = 0; i < n; i = j) {
        int fg, bg;
        ULONG style;
        j = runend(an, ch, at, st, i, n, &fg, &bg, &style);
        SetAPen(rp, pen(an, fg, bg));
        SetBPen(rp, an->drawpens[bg]);
        if (st) SetSoftStyle(rp, style, FSF_ITALIC | FSF_UNDERLINED);
        Move(rp, x + i * f->tf_XSize, y + f->tf_Baseline);
        Text(rp, ch + i, j - i);
    }
    if (st) SetSoftStyle(rp, 0, FSF_ITALIC | FSF_UNDERLINED);
}

/* The art drawn by this class (on OS 3.1, where text.datatype will not use
 * the PC font), or by text.datatype (3.2), and then the window around the
 * art (below its last row, right of its last column) black, as a PC's
 * screen. Only whole characters are drawn: no clipping is needed. */
static ULONG m_render(Class *cl, Object *o, struct gpRender *m)
{
    struct AnData *an = INST_DATA(cl, o);
    struct DTSpecialInfo *si = (struct DTSpecialInfo *)((struct Gadget *)o)->SpecialInfo;
    struct IBox *d = NULL;
    struct TagItem t[2];
    struct RastPort *rp = m->gpr_RPort;
    struct TextFont *old;
    long w, h, x, y, rows, cols, r, top, left, n, d0 = 0, d1 = 0x7FFFFFFF;
    ULONG ret = 0;
    int ownrp = 0, play = m->gpr_Redraw == GREDRAW_PLAY;
    AnsiPage *pg = &an->page;
    if (play && !an->ownrender) return 0;
    if (!an->ownrender || !an->built) ret = dosuper(cl, o, (Msg)m);
    /* from the player's process (DoGadgetMethod): no RastPort given */
    if (!rp && m->gpr_GInfo && (rp = ObtainGIRPort(m->gpr_GInfo))) ownrp = 1;
    t[0].ti_Tag = DTA_Domain; t[0].ti_Data = (ULONG)&d;
    t[1].ti_Tag = TAG_DONE;
    if (!an->built || !an->font || !rp || !getattrs(o, t) || !d || d->Width <= 0 || d->Height <= 0) {
        if (ownrp) ReleaseGIRPort(rp);
        return ret;
    }
    /* while an animation plays, its player's page; GREDRAW_PLAY draws only
     * the rows it changed */
    ObtainSemaphore(&an->lock);
    if (an->pl) {
        pg = ansi_play_page(an->pl);
        if (play) {
            d0 = an->d0;
            d1 = an->d1;
            an->d0 = 0x7FFFFFFF;
            an->d1 = -1;
        }
    } else if (play) {
        ReleaseSemaphore(&an->lock);
        if (ownrp) ReleaseGIRPort(rp);
        return 0;
    }
    ObtainSemaphoreShared(&si->si_Lock);
    top = si->si_TopVert;
    left = si->si_TopHoriz;
    rows = d->Height / an->font->tf_YSize;      /* whole rows and columns that fit */
    cols = d->Width / an->font->tf_XSize;
    if (an->ownrender) {
        old = rp->Font;
        SetFont(rp, an->font);
        SetDrMd(rp, JAM2);
        SetSoftStyle(rp, 0, AskSoftStyle(rp));
        n = an->page.cols - left;
        if (n > cols) n = cols;
        for (r = 0; r < rows && top + r < si->si_TotVert; r++)
            if (n > 0 && top + r >= d0 && top + r <= d1)
                drawrow(an, pg, rp, top + r, left, n, d->Left, d->Top + r * an->font->tf_YSize);
        SetFont(rp, old);
        h = (r < rows ? r : rows) * an->font->tf_YSize;
        w = (n > 0 ? n : 0) * an->font->tf_XSize;
    } else {
        h = (si->si_TotVert - top) * an->font->tf_YSize;
        w = (an->page.cols - left) * an->font->tf_XSize;
    }
    ReleaseSemaphore(&si->si_Lock);
    ReleaseSemaphore(&an->lock);
    if (play) {                                 /* the rest was drawn before */
        if (ownrp) ReleaseGIRPort(rp);
        return 0;
    }
    if (h < 0) h = 0;
    if (w < 0) w = 0;
    SetAPen(rp, an->drawpens[0]);
    SetDrMd(rp, JAM1);
    if (h < d->Height) {
        y = d->Top + h;
        RectFill(rp, d->Left, y, d->Left + d->Width - 1, d->Top + d->Height - 1);
    }
    if (w < d->Width && h > 0) {
        x = d->Left + w;
        RectFill(rp, x, d->Top, d->Left + d->Width - 1, d->Top + (h < d->Height ? h : d->Height) - 1);
    }
    if (ownrp) ReleaseGIRPort(rp);
    return ret;
}

static ULONG dispatch(REG(a0, Class *cl), REG(a2, Object *o), REG(a1, Msg msg))
{
    ULONG r;
    switch (msg->MethodID) {
    case OM_NEW:
        return m_new(cl, o, (struct opSet *)msg);
    case OM_DISPOSE:
        return m_dispose(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        return m_set(cl, o, (struct opSet *)msg);
    case GM_RENDER:
        return m_render(cl, o, (struct gpRender *)msg);
    case DTM_REMOVEDTOBJECT:                    /* out of its window: stop playing there */
        playstop((struct AnData *)INST_DATA(cl, o));
        return dosuper(cl, o, msg);
    case GM_LAYOUT:
        busy(o, ((struct gpLayout *)msg)->gpl_GInfo);
        r = dosuper(cl, o, msg);
        return r + DoAsyncLayout(o, (struct gpLayout *)msg);
    case DTM_PROCLAYOUT:
        busy(o, ((struct gpLayout *)msg)->gpl_GInfo);
        dosuper(cl, o, msg);
        return layout(cl, o, (struct gpLayout *)msg);
    case DTM_ASYNCLAYOUT:
        return layout(cl, o, (struct gpLayout *)msg);
    }
    return dosuper(cl, o, msg);
}

/* ---- the library ---------------------------------------------------- */

static struct Library *initlib(REG(d0, struct AnBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys));
static struct Library *libopen(REG(a6, struct AnBase *b));
static BPTR libclose(REG(a6, struct AnBase *b));
static BPTR libexpunge(REG(a6, struct AnBase *b));
static LONG libnull(void);
static Class *obtainengine(REG(a6, struct AnBase *b));

static const APTR functable[] = {
    (APTR)libopen, (APTR)libclose, (APTR)libexpunge, (APTR)libnull,
    (APTR)obtainengine,
    (APTR)-1
};

static const ULONG inittab[4] = {
    sizeof(struct AnBase), (ULONG)functable, 0, (ULONG)initlib
};

const struct Resident romtag = {
    RTC_MATCHWORD, (struct Resident *)&romtag, (APTR)(&romtag + 1),
    RTF_AUTOINIT, AN_VERSION, NT_LIBRARY, 0,
    (char *)libname, (char *)libid, (APTR)inittab
};

static void closeall(void)
{
    if (DiskfontBase) CloseLibrary(DiskfontBase);
    if (TextDTBase) CloseLibrary(TextDTBase);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    DiskfontBase = TextDTBase = DataTypesBase = UtilityBase = NULL;
    GfxBase = NULL;
    IntuitionBase = NULL;
    DOSBase = NULL;
}

static struct Library *initlib(REG(d0, struct AnBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys))
{
    SysBase = sys;
    b->lib.lib_Node.ln_Type = NT_LIBRARY;
    b->lib.lib_Node.ln_Name = (char *)libname;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = AN_VERSION;
    b->lib.lib_Revision = AN_REVISION;
    b->lib.lib_IdString = (APTR)libid;
    b->seglist = seg;
    InitSemaphore(&anlock);
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    UtilityBase = OpenLibrary("utility.library", 39);
    DataTypesBase = OpenLibrary("datatypes.library", 39);
    TextDTBase = OpenLibrary("datatypes/text.datatype", 39);
    if (DOSBase && IntuitionBase && GfxBase && UtilityBase && DataTypesBase && TextDTBase &&
        (anclass = MakeClass((STRPTR)libname, (STRPTR)TEXTDTCLASS, NULL, sizeof(struct AnData), 0))) {
        anclass->cl_Dispatcher.h_Entry = (ULONG (*)())(APTR)dispatch;
        anclass->cl_UserData = (ULONG)b;
        AddClass(anclass);
        return &b->lib;
    }
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return NULL;
}

static struct Library *libopen(REG(a6, struct AnBase *b))
{
    b->lib.lib_OpenCnt++;
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    return &b->lib;
}

static BPTR libexpunge(REG(a6, struct AnBase *b))
{
    BPTR seg;
    /* still open, or objects of the class still about: later */
    if (b->lib.lib_OpenCnt || !FreeClass(anclass)) {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    anclass = NULL;
    seg = b->seglist;
    Remove(&b->lib.lib_Node);
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return seg;
}

static BPTR libclose(REG(a6, struct AnBase *b))
{
    if (--b->lib.lib_OpenCnt == 0 && (b->lib.lib_Flags & LIBF_DELEXP))
        return libexpunge(b);
    return 0;
}

static LONG libnull(void)
{
    return 0;
}

static Class *obtainengine(REG(a6, struct AnBase *b))
{
    (void)b;
    return anclass;
}
