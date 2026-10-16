/* markdown.datatype: Markdown files shown formatted - headings, lists,
 * quotes, tables, code, emphasis, links - in MultiView, AmigaGuide and
 * every other program that shows text through datatypes.
 *
 * A subclass of text.datatype, as ascii.datatype is: text.datatype loads
 * the file into TDTA_Buffer and draws a list of struct Line pieces; this
 * class makes those pieces. The Markdown is read and wrapped by md.c, the
 * Markdown viewer's renderer (TextTools/markdown, built in unchanged): it
 * gives rows of cells with colours and attributes, made here into pieces
 * with Workbench pens and soft styles, one piece per run of the same look,
 * laid end to end. Wrapped to the window's width, again when it changes.
 *
 * The library: the Resident tag, open/close/expunge and the jump table are
 * written by hand (as tui.library's), with ObtainEngine() as the one call
 * of a datatype class. No C library: shim.c has the string calls. Runs on
 * a 68000 and Kickstart 3.0 (datatypes.library 39). */
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
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
#include "markdown.h"
#include "md.h"
#include "shim.h"
#include "version.h"

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
static struct Library *TextDTBase;      /* text.datatype, the superclass */
static Class *mdclass;
static int nounder;                     /* the text.datatype here draws underline wrong */
static struct SignalSemaphore mdlock;   /* md.c keeps its state in statics: one at a time */

struct MdBase {
    struct Library lib;
    BPTR seglist;
};

static const char libname[] = "markdown.datatype";
static const char libid[] = "markdown.datatype " MD_VERSTR " (" MD_DATE ")\r\n";
const char verstag[] = "$VER: markdown.datatype " MD_VERSTR " (" MD_DATE ")";

/* ---- an object's own data ---------------------------------------------- */

struct MdData {
    APTR pool;                  /* the pieces (struct Line + their text) */
    Buf buf;                    /* the file as lines, for md.c */
    long cols;                  /* the width it was last laid out to */
};

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

/* ---- the file as lines --------------------------------------------------- */

/* md.c reads a Buf: lines of bytes (pointing into text.datatype's buffer)
 * and the encoding */
static int makebuf(Buf *b, char *text, long n)
{
    long i, k = 0, lines = 1;
    for (i = 0; i < n; i++)
        if (text[i] == '\n') lines++;
    if (!(b->l = AllocVec(sizeof(Line) * lines, MEMF_ANY | MEMF_CLEAR))) return 0;
    b->cap = lines;
    b->l[0].s = text;
    for (i = 0; i < n; i++)
        if (text[i] == '\n') {
            b->l[k].len = text + i - b->l[k].s;
            k++;
            b->l[k].s = text + i + 1;
        }
    b->l[k].len = text + n - b->l[k].s;
    /* a last empty line after the final newline is not a line */
    b->n = (k > 0 && b->l[k].len == 0) ? k : k + 1;
    b->enc = isutf8((const unsigned char *)text, n) ? ENC_UTF8 : ENC_LATIN1;
    b->rev = 1;
    return 1;
}

/* text.datatype loads the file itself; if this one did not, read it from
 * the handle datatypes.library opened */
static int loadbuffer(Class *cl, Object *o, char **text, long *len)
{
    struct TagItem t[3];
    struct opSet set;
    BPTR fh = 0;
    ULONG type = DTST_FILE;
    long size;
    char *b;
    t[0].ti_Tag = DTA_Handle;     t[0].ti_Data = (ULONG)&fh;
    t[1].ti_Tag = DTA_SourceType; t[1].ti_Data = (ULONG)&type;
    t[2].ti_Tag = TAG_DONE;
    getattrs(o, t);
    if (type != DTST_FILE || !fh) return 0;
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    if (size <= 0 || !(b = AllocVec(size + 1, MEMF_ANY))) return 0;
    if (Read(fh, b, size) != size) {
        FreeVec(b);
        return 0;
    }
    b[size] = 0;
    t[0].ti_Tag = TDTA_Buffer;    t[0].ti_Data = (ULONG)b;
    t[1].ti_Tag = TDTA_BufferLen; t[1].ti_Data = size;
    t[2].ti_Tag = TAG_DONE;
    set.MethodID = OM_SET;
    set.ops_AttrList = t;
    set.ops_GInfo = NULL;
    dosuper(cl, o, (Msg)&set);
    *text = b;
    *len = size;
    return 1;
}

/* ---- colours and styles ---------------------------------------------- */

/* md.c's colours are CTerm's sixteen; a Workbench has four pens it is
 * sure of: 0 background, 1 text, 2 shine (white), 3 fill (blue) */
static UBYTE pen(UBYTE c)
{
    if (c == TC_DEFAULT || c == 0) return 1;
    if (c == 7 || c == 8 || c == 15) return 2;      /* grey and white: rules, boxes, dim */
    return 3;                                       /* every colour: headings, links, code */
}

struct Look { UBYTE fg, bg; UBYTE style; };

static struct Look look(const RCell *c)
{
    struct Look l;
    UBYTE fg = (c->at & TA_DIM) ? 2 : pen(c->fg);
    l.style = 0;
    if (c->at & TA_BOLD) l.style |= FSF_BOLD;
    if (c->at & TA_ITAL) l.style |= FSF_ITALIC;
    /* links: underlined; text.datatype 45 and later (3.1.4, 3.2) lose the
     * underline of a piece that starts 256 pixels or more from the left,
     * and may carry it into the next piece: blue italic there instead */
    if (c->at & TA_UNDER) l.style |= nounder ? FSF_ITALIC : FSF_UNDERLINED;
    if (c->at & TA_REV) {               /* `code` and the top heading's band */
        l.bg = fg == 1 ? 2 : fg;
        l.fg = l.bg == 2 ? 1 : 2;
    } else {
        l.bg = 0;
        l.fg = fg;
    }
    return l;
}

/* ---- layout: md.c's rows made into text.datatype's pieces -------------- */

static struct Line *piece(APTR pool, const RCell *c, long n, struct Look lk,
                          struct RastPort *rp, long x, long y, int lf)
{
    struct Line *ln = AllocPooled(pool, sizeof(struct Line) + n + 1);
    char *s;
    long i;
    if (!ln) return NULL;
    s = (char *)(ln + 1);
    for (i = 0; i < n; i++) s[i] = latin1(c[i].c);
    s[n] = 0;
    ln->ln_Text = s;
    ln->ln_TextLen = n;
    ln->ln_XOffset = x;
    ln->ln_YOffset = y;
    ln->ln_Width = n ? TextLength(rp, s, n) : 0;
    ln->ln_Height = rp->Font->tf_YSize;
    ln->ln_Flags = lf ? LNF_LF : 0;
    ln->ln_FgPen = lk.fg;
    ln->ln_BgPen = lk.bg;
    ln->ln_Style = lk.style;
    ln->ln_Data = NULL;
    return ln;
}

/* md.c on a stack of its own: a datatype runs on whatever stack its caller
 * has (often 4 KB), and md.c takes about 1 KB a level of nested lists and
 * quotes (blocks(), -fstack-usage) */
#define MDSTACK 32768

struct RenderArgs { Doc *d; Buf *b; long cols; };

void callswapped(struct StackSwapStruct *sss, void (*fn)(void *), void *arg);

static void render(void *p)
{
    struct RenderArgs *a = p;
    md_render(a->d, a->b, a->cols, "");
}

/* the pieces for the whole document; returns the rows made, -1 out of memory */
static long build(struct MdData *md, struct List *lines, struct TextFont *font,
                  long cols, long *widest)
{
    Doc d;
    struct RastPort rp;
    struct StackSwapStruct sss;
    struct RenderArgs args;
    APTR stack;
    long r, total = 0;
    memset(&d, 0, sizeof(d));
    d.rev = -1;
    InitRastPort(&rp);
    SetFont(&rp, font);
    *widest = 0;

    if (!(stack = AllocVec(MDSTACK, MEMF_ANY))) return -1;
    sss.stk_Lower = stack;
    sss.stk_Upper = (ULONG)stack + MDSTACK;
    sss.stk_Pointer = (APTR)sss.stk_Upper;
    args.d = &d;
    args.b = &md->buf;
    args.cols = cols;
    ObtainSemaphore(&mdlock);
    md_unicode = 0;             /* Latin-1 fonts: ASCII bullets and boxes */
    md_pictures = 0;            /* no pictures in a text.datatype */
    md_cellw = md_cellh = 0;
    callswapped(&sss, render, &args);
    ReleaseSemaphore(&mdlock);
    FreeVec(stack);

    for (r = 0; r < d.nrows; r++) {
        const RRow *row = &d.rows[r];
        const RCell *c = d.cells + row->cell;
        long y = r * font->tf_YSize + font->tf_Baseline;
        long x = 0, i = 0, n = row->len;
        struct Line *ln;
        if (n == 0) {               /* an empty row: one empty piece ends it */
            struct Look none = { 1, 0, 0 };
            if (!(ln = piece(md->pool, c, 0, none, &rp, 0, y, 1))) goto oom;
            AddTail(lines, (struct Node *)ln);
        }
        while (i < n) {
            struct Look lk = look(&c[i]), lk2;
            long j = i + 1;
            while (j < n) {
                lk2 = look(&c[j]);
                if (lk2.fg != lk.fg || lk2.bg != lk.bg || lk2.style != lk.style) break;
                j++;
            }
            if (!(ln = piece(md->pool, c + i, j - i, lk, &rp, x, y, j == n))) goto oom;
            AddTail(lines, (struct Node *)ln);
            x += ln->ln_Width;
            i = j;
        }
        if (x > *widest) *widest = x;
        total++;
    }
    md_free(&d);
    return total;
oom:
    md_free(&d);
    return total ? total : -1;
}

static ULONG layout(Class *cl, Object *o, struct gpLayout *gpl)
{
    struct MdData *md = INST_DATA(cl, o);
    struct DTSpecialInfo *si = (struct DTSpecialInfo *)((struct Gadget *)o)->SpecialInfo;
    struct TextFont *font = NULL;
    struct IBox *domain = NULL;
    struct List *lines = NULL;
    STRPTR title = NULL;
    struct TagItem t[5], n[14];
    long cols, total, widest, hunit, vis;

    t[0].ti_Tag = DTA_TextFont;  t[0].ti_Data = (ULONG)&font;
    t[1].ti_Tag = DTA_Domain;    t[1].ti_Data = (ULONG)&domain;
    t[2].ti_Tag = DTA_ObjName;   t[2].ti_Data = (ULONG)&title;
    t[3].ti_Tag = TDTA_LineList; t[3].ti_Data = (ULONG)&lines;
    t[4].ti_Tag = TAG_DONE;
    if (getattrs(o, t) < 4 || !font || !domain || !lines) return 0;

    ObtainSemaphore(&si->si_Lock);
    hunit = font->tf_XSize ? font->tf_XSize : 8;
    cols = domain->Width / hunit;
    if (cols < 20) cols = 20;
    D(("md: layout %lx initial %ld cols %ld (was %ld)\n", o, gpl->gpl_Initial, cols, md->cols));
    if (gpl->gpl_Initial || cols != md->cols || !md->pool) {
        /* the old pieces go with their pool (whatever text.datatype had in
         * the list is not ours to free) */
#ifdef DEBUG
        {
            struct Node *nd;
            long k = 0;
            for (nd = lines->lh_Head; nd->ln_Succ; nd = nd->ln_Succ) k++;
            D(("md: list had %ld pieces (ours: pool %lx)\n", k, md->pool));
        }
#endif
        newlist(lines);
        if (md->pool) DeletePool(md->pool);
        md->pool = CreatePool(MEMF_ANY | MEMF_PUBLIC, 8192, 4096);
        total = md->pool ? build(md, lines, font, cols, &widest) : -1;
        if (total < 0) {
            newlist(lines);
            total = 0;
            widest = 0;
        }
        md->cols = cols;
    } else {
        total = si->si_TotVert;
        widest = si->si_TotHoriz * si->si_HorizUnit;
    }
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
    n[3].ti_Tag = DTA_NominalVert;   n[3].ti_Data = 24 * font->tf_YSize;
    n[4].ti_Tag = DTA_VertUnit;      n[4].ti_Data = font->tf_YSize;
    n[5].ti_Tag = DTA_VisibleHoriz;  n[5].ti_Data = domain->Width / hunit;
    n[6].ti_Tag = DTA_TotalHoriz;    n[6].ti_Data = (widest + hunit - 1) / hunit;
    n[7].ti_Tag = DTA_NominalHoriz;  n[7].ti_Data = 80 * hunit;
    n[8].ti_Tag = DTA_HorizUnit;     n[8].ti_Data = hunit;
    n[9].ti_Tag = DTA_TopHoriz;      n[9].ti_Data = si->si_TopHoriz;
    n[10].ti_Tag = DTA_Title;        n[10].ti_Data = (ULONG)title;
    n[11].ti_Tag = DTA_Busy;         n[11].ti_Data = FALSE;
    n[12].ti_Tag = DTA_Sync;         n[12].ti_Data = TRUE;
    n[13].ti_Tag = TAG_DONE;
    notify(o, gpl->gpl_GInfo, n);
    return total;
}

/* ---- the methods ----------------------------------------------------- */

static ULONG m_new(Class *cl, Object *o, struct opSet *msg)
{
    Object *obj = (Object *)dosuper(cl, o, (Msg)msg);
    struct MdData *md;
    struct TagItem t[3];
    char *text = NULL;
    long len = 0;
    if (!obj) return 0;
    md = INST_DATA(cl, obj);
    t[0].ti_Tag = TDTA_Buffer;    t[0].ti_Data = (ULONG)&text;
    t[1].ti_Tag = TDTA_BufferLen; t[1].ti_Data = (ULONG)&len;
    t[2].ti_Tag = TAG_DONE;
    getattrs(obj, t);
    D(("md: new %lx buffer %lx len %ld\n", obj, text, len));
    if (!text) {
        loadbuffer(cl, obj, &text, &len);
        D(("md: loaded it ourselves: %lx len %ld\n", text, len));
    }
    if (text && len >= 0 && makebuf(&md->buf, text, len))
        return (ULONG)obj;
    SetIoErr(text ? ERROR_NO_FREE_STORE : ERROR_REQUIRED_ARG_MISSING);
    {
        ULONG dm = OM_DISPOSE;      /* our own dispose, then the superclass's */
        CallHookPkt((struct Hook *)cl, obj, &dm);
    }
    return 0;
}

static ULONG m_dispose(Class *cl, Object *o, Msg msg)
{
    struct MdData *md = INST_DATA(cl, o);
    struct List *lines = NULL;
    struct TagItem t[2];
    t[0].ti_Tag = TDTA_LineList; t[0].ti_Data = (ULONG)&lines;
    t[1].ti_Tag = TAG_DONE;
    D(("md: dispose %lx pool %lx lines %lx\n", o, md->pool, md->buf.l));
    /* the pieces are ours: text.datatype must not free them */
    if (getattrs(o, t) && lines) newlist(lines);
    if (md->pool) DeletePool(md->pool);
    md->pool = NULL;
    if (md->buf.l) FreeVec(md->buf.l);
    md->buf.l = NULL;
    return dosuper(cl, o, msg);
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

static struct Library *initlib(REG(d0, struct MdBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys));
static struct Library *libopen(REG(a6, struct MdBase *b));
static BPTR libclose(REG(a6, struct MdBase *b));
static BPTR libexpunge(REG(a6, struct MdBase *b));
static LONG libnull(void);
static Class *obtainengine(REG(a6, struct MdBase *b));

static const APTR functable[] = {
    (APTR)libopen, (APTR)libclose, (APTR)libexpunge, (APTR)libnull,
    (APTR)obtainengine,
    (APTR)-1
};

static const ULONG inittab[4] = {
    sizeof(struct MdBase), (ULONG)functable, 0, (ULONG)initlib
};

const struct Resident romtag = {
    RTC_MATCHWORD, (struct Resident *)&romtag, (APTR)(&romtag + 1),
    RTF_AUTOINIT, MD_VERSION, NT_LIBRARY, 0,
    (char *)libname, (char *)libid, (APTR)inittab
};

static void closeall(void)
{
    if (TextDTBase) CloseLibrary(TextDTBase);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    TextDTBase = DataTypesBase = UtilityBase = NULL;
    GfxBase = NULL;
    IntuitionBase = NULL;
    DOSBase = NULL;
}

static struct Library *initlib(REG(d0, struct MdBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys))
{
    SysBase = sys;
    b->lib.lib_Node.ln_Type = NT_LIBRARY;
    b->lib.lib_Node.ln_Name = (char *)libname;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = MD_VERSION;
    b->lib.lib_Revision = MD_REVISION;
    b->lib.lib_IdString = (APTR)libid;
    b->seglist = seg;
    InitSemaphore(&mdlock);
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    UtilityBase = OpenLibrary("utility.library", 39);
    DataTypesBase = OpenLibrary("datatypes.library", 39);
    TextDTBase = OpenLibrary("datatypes/text.datatype", 39);
    if (DOSBase && IntuitionBase && GfxBase && UtilityBase && DataTypesBase && TextDTBase &&
        (mdclass = MakeClass((STRPTR)libname, (STRPTR)TEXTDTCLASS, NULL, sizeof(struct MdData), 0))) {
        mdclass->cl_Dispatcher.h_Entry = (ULONG (*)())(APTR)dispatch;
        mdclass->cl_UserData = (ULONG)b;
        nounder = TextDTBase->lib_Version >= 45;
        AddClass(mdclass);
        return &b->lib;
    }
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return NULL;
}

static struct Library *libopen(REG(a6, struct MdBase *b))
{
    b->lib.lib_OpenCnt++;
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    return &b->lib;
}

static BPTR libexpunge(REG(a6, struct MdBase *b))
{
    BPTR seg;
    /* still open, or objects of the class still about: later */
    if (b->lib.lib_OpenCnt || !FreeClass(mdclass)) {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    mdclass = NULL;
    md_cleanup();               /* md.c's work buffers, kept between layouts */
    seg = b->seglist;
    Remove(&b->lib.lib_Node);
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return seg;
}

static BPTR libclose(REG(a6, struct MdBase *b))
{
    if (--b->lib.lib_OpenCnt == 0 && (b->lib.lib_Flags & LIBF_DELEXP))
        return libexpunge(b);
    return 0;
}

static LONG libnull(void)
{
    return 0;
}

static Class *obtainengine(REG(a6, struct MdBase *b))
{
    (void)b;
    return mdclass;
}
