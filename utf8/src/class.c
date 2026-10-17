/* utf8.datatype: UTF-8 text files shown with the right letters in
 * MultiView, AmigaGuide and every other program that shows text through
 * datatypes, instead of two or three wrong ones for each.
 *
 * A subclass of ascii.datatype: text.datatype loads the file into
 * TDTA_Buffer, this class turns it into Latin-1 with utf8.library
 * (Utf8ToCharset: what Latin-1 lacks becomes the nearest it has: curly
 * quotes plain ones, a Polish L with stroke an L, the euro EUR, an emoji
 * '?'; the byte order mark goes), drops the CR of each CR LF, and puts
 * the new text in place of the old. ascii.datatype does the rest as for
 * any text file: layout, tabs, search, marking, copying, printing.
 * Without utf8.library the file is shown as it is.
 *
 * The descriptors (DEVS:DataTypes/UTF-8 and UTF-8bin) carry recognition
 * code (hook.c) that claims a file only when it is good UTF-8 with
 * something beyond ASCII.
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
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/textclass.h>
#include <utility/hooks.h>
#include <libraries/utf8.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/datatypes.h>
#include <proto/utf8.h>
#include "version.h"

#define REG(r, a) a __asm(#r)

/* make DEBUG=1: a trace through RawPutChar (wasabi debug) */
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
struct Library *UtilityBase;
struct Library *DataTypesBase;
struct Library *Utf8Base;               /* opened at the first file, may stay NULL */
static struct Library *AsciiDTBase;     /* ascii.datatype, the superclass */
static Class *u8class;
static struct SignalSemaphore u8lock;   /* opening utf8.library */

struct U8Base {
    struct Library lib;
    BPTR seglist;
};

static const char libname[] = "utf8.datatype";
static const char libid[] = "utf8.datatype " U8_VERSTR " (" U8_DATE ")\r\n";
const char verstag[] = "$VER: utf8.datatype " U8_VERSTR " (" U8_DATE ")";

/* ---- calling methods without amiga.lib --------------------------------- */

static ULONG domethod(Object *o, Msg m)
{
    return CallHookPkt((struct Hook *)OCLASS(o), o, m);
}

static ULONG dosuper(Class *cl, Object *o, Msg m)
{
    return CallHookPkt((struct Hook *)cl->cl_Super, o, m);
}

/* ---- the text ---------------------------------------------------------- */

static struct Library *utf8lib(void)
{
    ObtainSemaphore(&u8lock);
    if (!Utf8Base) Utf8Base = OpenLibrary(UTF8NAME, 0);
    ReleaseSemaphore(&u8lock);
    return Utf8Base;
}

/* the first byte from 128 up, or -1: four bytes at a time once the
 * address is even (a 68000 reads a long only from an even address) */
static long firsthigh(const UBYTE *t, long len)
{
    const UBYTE *p = t, *e = t + len;
    if (((ULONG)p & 1) && p < e && !(*p & 0x80)) p++;
    if (!((ULONG)p & 1)) {
        const ULONG *l = (const ULONG *)p, *le = (const ULONG *)(p + ((e - p) & ~3L));
        while (l < le && !(*l & 0x80808080UL)) l++;
        p = (const UBYTE *)l;
    }
    while (p < e && !(*p & 0x80)) p++;
    return p < e ? p - t : -1;
}

/* the text's lines from p to e as Latin-1 at d, CR LF made LF. Plain
 * ASCII lines are copied as they are, lines with more go through
 * utf8.library (a whole line, so accents written as marks join their
 * letters). With inplace, d is in the same buffer behind p, and a line is
 * written only if it ends no further on than its own UTF-8 did: else it
 * stops there. *pp and *dp say how far it got. 0: all done; 1: stopped
 * (in place only); -1: out of memory. tmp/tmpsize: a passing buffer for
 * one line, grown as needed. */
static int lines(const UBYTE **pp, const UBYTE *e, char **dp, int inplace, char **tmp, long *tmpsize)
{
    const UBYTE *p = *pp, *q, *r;
    char *d = *dp;
    int st = 0;
    while (p < e) {
        int high = 0;
        long n;
        for (q = p; q < e && *q != '\n'; q++)
            high |= *q & 0x80;
        if (q < e) q++;                     /* through the LF */
        if (!high) {
            /* (the CR is looked for before the line moves over itself) */
            int crlf = q - p >= 2 && q[-1] == '\n' && q[-2] == '\r';
            if ((char *)p != d)
                for (r = p; r < q; r++) *d++ = *r;
            else
                d += q - p;
            if (crlf) d[-2] = '\n', d--;
            p = q;
            continue;
        }
        for (;;) {
            n = Utf8ToCharset((CONST_STRPTR)p, q - p, *tmp, *tmpsize, U8CS_LATIN1, 0);
            if (n >= 0 && n < *tmpsize) break;
            FreeVec(*tmp);
            if (n < 0 || !(*tmp = AllocVec(*tmpsize = n + 64, MEMF_ANY))) {
                *tmp = NULL;
                *tmpsize = 0;
                st = -1;
                goto out;
            }
        }
        if (n >= 2 && (*tmp)[n - 1] == '\n' && (*tmp)[n - 2] == '\r') (*tmp)[n - 2] = '\n', n--;
        if (inplace && d + n > (char *)q) {         /* would pass what is still to read */
            st = 1;
            break;
        }
        CopyMem(*tmp, d, n);
        d += n;
        p = q;
    }
out:
    *pp = p;
    *dp = d;
    return st;
}

/* the Latin-1 lines from p on would need: an upper bound */
static long measure(const UBYTE *p, const UBYTE *e)
{
    long n = 0;
    while (p < e) {
        const UBYTE *q = p;
        while (q < e && *q != '\n') q++;
        if (q < e) q++;
        n += Utf8ToCharset((CONST_STRPTR)p, q - p, NULL, 0, U8CS_LATIN1, 0);
        p = q;
    }
    return n;
}

/* The file into Latin-1 where it lies: lines are written back over their
 * own UTF-8, which nearly always ends further on (memory: the file and one
 * line, not two files). Korean and a few Russian letters are spelt longer
 * than their UTF-8; when the text would pass what is still to be read, the
 * rest goes into a new buffer, which replaces the old (freed here:
 * text.datatype does not free a buffer it is given another for). A file
 * with nothing from 128 up is left as it is (four bytes at a time to
 * know). The byte after the text (text.datatype's own end mark) is kept. */
static void convert(Class *cl, Object *o)
{
    struct TagItem t[3];
    struct opSet set;
    char *text = NULL, *nb = NULL, *d, *tmp;
    ULONG len = 0;
    long n, tmpsize = 1024;
    const UBYTE *p, *e;
    int st;
    t[0].ti_Tag = TDTA_Buffer;    t[0].ti_Data = (ULONG)&text;
    t[1].ti_Tag = TDTA_BufferLen; t[1].ti_Data = (ULONG)&len;
    t[2].ti_Tag = TAG_DONE;
    GetDTAttrsA(o, t);
    D(("u8: buffer %lx len %ld\n", text, len));
    if (!text || !len || firsthigh((const UBYTE *)text, len) < 0 || !utf8lib()) return;
    if (!(tmp = AllocVec(tmpsize, MEMF_ANY))) return;
    p = (const UBYTE *)text;
    e = p + len;
    d = text;
    st = lines(&p, e, &d, 1, &tmp, &tmpsize);
    if (st == 1) {
        /* grew: what is done, then the rest, into a new buffer */
        const UBYTE *p0 = p;
        long done = d - text, size = done + measure(p, e) + 1;
        if ((nb = AllocVec(size, MEMF_ANY))) {
            char *nd = nb + done;
            CopyMem(text, nb, done);
            if (lines(&p, e, &nd, 0, &tmp, &tmpsize) == 0)
                d = nd;
            else {
                FreeVec(nb);
                nb = NULL;
                p = p0;                     /* the rest as it is, below */
                st = -1;
            }
        } else
            st = -1;
    }
    if (tmp) FreeVec(tmp);
    if (st < 0) {
        /* out of memory part way: what is converted, then the rest as it was */
        D(("u8: out of memory part way\n"));
        while (p < e) *d++ = *p++;
    }
    n = d - (nb ? nb : text);
    if (nb) {
        nb[n] = text[len];
        t[0].ti_Data = (ULONG)nb;
    } else {
        if (n < len) text[n] = text[len];
        t[0].ti_Tag = TAG_IGNORE;
    }
    t[1].ti_Data = n;
    set.MethodID = OM_SET;
    set.ops_AttrList = t;
    set.ops_GInfo = NULL;
    dosuper(cl, o, (Msg)&set);
    if (nb) FreeVec(text);
    D(("u8: now len %ld%s\n", n, nb ? " (new buffer)" : ""));
}

/* ---- methods ----------------------------------------------------------- */

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
    switch (msg->MethodID) {
    case OM_NEW: {
        Object *obj = (Object *)dosuper(cl, o, msg);
        if (obj) convert(cl, obj);
        return (ULONG)obj;
    }
    case OM_SET:
    case OM_UPDATE:
        return m_set(cl, o, (struct opSet *)msg);
    }
    return dosuper(cl, o, msg);
}

/* ---- the library ---------------------------------------------------- */

static struct Library *initlib(REG(d0, struct U8Base *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys));
static struct Library *libopen(REG(a6, struct U8Base *b));
static BPTR libclose(REG(a6, struct U8Base *b));
static BPTR libexpunge(REG(a6, struct U8Base *b));
static LONG libnull(void);
static Class *obtainengine(REG(a6, struct U8Base *b));

static const APTR functable[] = {
    (APTR)libopen, (APTR)libclose, (APTR)libexpunge, (APTR)libnull,
    (APTR)obtainengine,
    (APTR)-1
};

static const ULONG inittab[4] = {
    sizeof(struct U8Base), (ULONG)functable, 0, (ULONG)initlib
};

const struct Resident romtag = {
    RTC_MATCHWORD, (struct Resident *)&romtag, (APTR)(&romtag + 1),
    RTF_AUTOINIT, U8_VERSION, NT_LIBRARY, 0,
    (char *)libname, (char *)libid, (APTR)inittab
};

static void closeall(void)
{
    if (Utf8Base) CloseLibrary(Utf8Base);
    if (AsciiDTBase) CloseLibrary(AsciiDTBase);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    Utf8Base = AsciiDTBase = DataTypesBase = UtilityBase = NULL;
    IntuitionBase = NULL;
    DOSBase = NULL;
}

static struct Library *initlib(REG(d0, struct U8Base *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys))
{
    SysBase = sys;
    b->lib.lib_Node.ln_Type = NT_LIBRARY;
    b->lib.lib_Node.ln_Name = (char *)libname;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = U8_VERSION;
    b->lib.lib_Revision = U8_REVISION;
    b->lib.lib_IdString = (APTR)libid;
    b->seglist = seg;
    InitSemaphore(&u8lock);
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    UtilityBase = OpenLibrary("utility.library", 39);
    DataTypesBase = OpenLibrary("datatypes.library", 39);
    AsciiDTBase = OpenLibrary("datatypes/ascii.datatype", 39);
    if (DOSBase && IntuitionBase && UtilityBase && DataTypesBase && AsciiDTBase &&
        (u8class = MakeClass((STRPTR)libname, (STRPTR)"ascii.datatype", NULL, 0, 0))) {
        u8class->cl_Dispatcher.h_Entry = (ULONG (*)())(APTR)dispatch;
        u8class->cl_UserData = (ULONG)b;
        AddClass(u8class);
        return &b->lib;
    }
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return NULL;
}

static struct Library *libopen(REG(a6, struct U8Base *b))
{
    b->lib.lib_OpenCnt++;
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    return &b->lib;
}

static BPTR libexpunge(REG(a6, struct U8Base *b))
{
    BPTR seg;
    /* still open, or objects of the class still about: later */
    if (b->lib.lib_OpenCnt || !FreeClass(u8class)) {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    u8class = NULL;
    seg = b->seglist;
    Remove(&b->lib.lib_Node);
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return seg;
}

static BPTR libclose(REG(a6, struct U8Base *b))
{
    if (--b->lib.lib_OpenCnt == 0 && (b->lib.lib_Flags & LIBF_DELEXP))
        return libexpunge(b);
    return 0;
}

static LONG libnull(void)
{
    return 0;
}

static Class *obtainengine(REG(a6, struct U8Base *b))
{
    (void)b;
    return u8class;
}
