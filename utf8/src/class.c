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

/* the file as Latin-1, CR LF made LF, in a passing buffer of the file's
 * size: it always fits (see convert()). Lines of plain ASCII, most of a
 * README, are copied as they are; utf8.library gets only the lines with
 * more (a whole line, so accents written as marks join their letters):
 * on a 68000 that is several times faster than handing it everything. */
static char *latin1(const char *text, long len, long *outlen)
{
    const UBYTE *t = (const UBYTE *)text, *p = t, *e = t + len, *q;
    char *b, *d, *s, *end;
    if (!(b = AllocVec(len + 1, MEMF_ANY))) return NULL;
    d = b;
    while (p < e) {
        /* the plain lines up to the next line with a byte from 128 up */
        for (q = p; q < e && *q < 0x80; q++)
            ;
        while (q > p && q[-1] != '\n') q--;
        CopyMem((APTR)p, d, q - p);
        d += q - p;
        if ((p = q) == e) break;
        /* that line, through its LF */
        for (q = p; q < e && *q != '\n'; q++)
            ;
        if (q < e) q++;
        {
            LONG n = Utf8ToCharset((CONST_STRPTR)p, q - p, (STRPTR)d, b + len + 1 - d, U8CS_LATIN1, 0);
            if (n < 0 || n > q - p) {
                FreeVec(b);
                return NULL;
            }
            d += n;
        }
        p = q;
    }
    /* a CR would show as a box at the end of every line */
    for (s = b, end = d, d = b; s < end; s++)
        if (*s != '\r' || s + 1 == end || s[1] != '\n') *d++ = *s;
    *outlen = d - b;
    return b;
}

/* text.datatype (40 and 47) does not free its buffer when given another
 * (it leaked the file's size at each open), so the Latin-1 text goes back
 * into the same buffer and only the length changes. It always fits: no
 * character's stand-in is longer than its UTF-8 (all of Unicode checked
 * against utf8.library on the PC), joining marks and CR LF only shorten it. */
static void convert(Class *cl, Object *o)
{
    struct TagItem t[3];
    struct opSet set;
    char *text = NULL, *b;
    ULONG len = 0;
    long n;
    t[0].ti_Tag = TDTA_Buffer;    t[0].ti_Data = (ULONG)&text;
    t[1].ti_Tag = TDTA_BufferLen; t[1].ti_Data = (ULONG)&len;
    t[2].ti_Tag = TAG_DONE;
    GetDTAttrsA(o, t);
    D(("u8: buffer %lx len %ld\n", text, len));
    if (!text || !len || !utf8lib() || !(b = latin1(text, len, &n))) return;
    CopyMem(b, text, n);
    if (n < len) text[n] = 0;
    FreeVec(b);
    t[0].ti_Tag = TDTA_BufferLen; t[0].ti_Data = n;
    t[1].ti_Tag = TAG_DONE;
    set.MethodID = OM_SET;
    set.ops_AttrList = t;
    set.ops_GInfo = NULL;
    dosuper(cl, o, (Msg)&set);
    D(("u8: now len %ld\n", n));
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
