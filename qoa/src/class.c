/* qoa.datatype: QOA sounds (the "Quite OK Audio" format) in MultiView,
 * file managers and every program that plays sounds through datatypes.
 *
 * A subclass of sound.datatype. The file is read and decoded a frame at a
 * time (qoadec.c) straight into 8-bit samples, so only the 8-bit sound is
 * ever whole in memory (2.5 times the file's size, half that in mono):
 *
 *  - sound.datatype 44 and later (OS 3.5, 3.9, 3.2): stereo stays stereo
 *    (more channels: the first two, front left and right).
 *  - sound.datatype 40 (OS 3.0, 3.1): no stereo there, all channels mixed
 *    into one.
 *  - Paula plays at most about 28 kHz, so 44.1 or 48 kHz sound is averaged
 *    down to half (a third, a quarter...) while decoding instead of
 *    playing slow.
 *
 * Files without a length ("streaming" QOA, where rate and channels may
 * change) and over 8 channels are refused. A damaged file plays up to the
 * damage.
 *
 * The library part (Resident tag, jump table, ObtainEngine) is
 * qoi.datatype's. No C library. Runs on a 68000 and Kickstart 3.0. */
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/datatypes.h>
#include "qoadec.h"
#include "conv.h"
#include "version.h"

#define REG(r, a) a __asm(#r)

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase;
struct Library *DataTypesBase;
static struct Library *SoundDTBase;     /* sound.datatype, the superclass */
static Class *qoaclass;

struct QoaBase {
    struct Library lib;
    BPTR seglist;
};

static const char libname[] = "qoa.datatype";
static const char libid[] = "qoa.datatype " QA_VERSTR " (" QA_DATE ")\r\n";
const char verstag[] = "$VER: qoa.datatype " QA_VERSTR " (" QA_DATE ")";

/* ---- calling methods without amiga.lib --------------------------------- */

static ULONG dosuper(Class *cl, Object *o, Msg m)
{
    return CallHookPkt((struct Hook *)cl->cl_Super, o, m);
}

/* tag lists are built as arrays: a varargs helper that takes &tag breaks
 * when the compiler inlines it (the tags are then not on the stack in a row) */
#define TAGS(...) ((struct TagItem []){ __VA_ARGS__, { TAG_DONE, 0 } })
#define SETATTRS(o, ...) SetDTAttrsA((o), NULL, NULL, TAGS(__VA_ARGS__))
#define GETATTRS(o, ...) GetDTAttrsA((o), TAGS(__VA_ARGS__))

/* ---- the object ------------------------------------------------------ */

static LONG load(Object *o)
{
    BPTR fh = 0;
    ULONG type = DTST_FILE, rate, period, k, len;
    LONG size, pos;
    UBYTE head[QOA_HEADER];
    UBYTE *frame = NULL;
    WORD *pcm = NULL;
    QoaDec *d;
    Conv c;
    LONG rc = ERROR_NO_FREE_STORE;
    int v44 = SoundDTBase->lib_Version >= 44;

    c.out[0] = c.out[1] = NULL;
    GETATTRS(o, { DTA_SourceType, (ULONG)&type }, { DTA_Handle, (ULONG)&fh });
    if (type != DTST_FILE) return ERROR_NOT_IMPLEMENTED;
    if (!fh) return ERROR_REQUIRED_ARG_MISSING;
    if (Seek(fh, 0, OFFSET_END) < 0 || (size = Seek(fh, 0, OFFSET_BEGINNING)) < 0) return ERROR_SEEK_ERROR;
    if (Read(fh, head, QOA_HEADER) != QOA_HEADER) return DTERROR_NOT_ENOUGH_DATA;
    if (!(d = AllocVec(sizeof(QoaDec), MEMF_ANY))) return ERROR_NO_FREE_STORE;
    if (!qa_header(d, head)) {
        FreeVec(d);
        return DTERROR_INVALID_DATA;
    }
    qa_limit(d, size);

    rate = cv_init(&c, d, v44);
    len = (c.len + 1) & ~1;             /* sound.datatype 40 wants an even length */
    for (k = 0; k < c.outch; k++)
        if (!(c.out[k] = AllocVec(len ? len : 2, MEMF_ANY | MEMF_CLEAR))) goto done;
    if (!(frame = AllocVec(QOA_FRAMEMAX(c.ch), MEMF_ANY)) ||
        !(pcm = AllocVec(QOA_FRAMELEN * c.ch * sizeof(WORD), MEMF_ANY)))
        goto done;

    /* every frame, until the length or the first one that does not fit */
    pos = QOA_HEADER - 8;
    CopyMem(head + 8, frame, 8);
    for (;;) {
        ULONG fl = qa_frameheader(d, frame);
        if (!fl || d->fsize > (ULONG)(size - pos)) break;
        if (Read(fh, frame + 8, fl - 8) != (LONG)(fl - 8)) break;
        qa_frame(d, frame, pcm);
        cv_frame(&c, pcm, d->fsamples);
        pos += fl;
        if (d->fsamples != QOA_FRAMELEN || c.pos >= c.len) break;
        if (Read(fh, frame, 8) != 8) break;     /* the next frame's header */
    }
    rc = DTERROR_INVALID_DATA;
    if (!c.pos) goto done;
    len = c.pos;
    if (!v44) len = (len + 1) & ~1;

    /* the object's name: the file's, as 8svx.datatype gives it */
    {
        STRPTR name = NULL;
        GETATTRS(o, { DTA_Name, (ULONG)&name });
        if (name) SETATTRS(o, { DTA_ObjName, (ULONG)name });
    }
    period = (SysBase->ex_EClockFrequency * 5 + rate / 2) / rate;     /* the colour clock, PAL or NTSC */
    if (period < 124) period = 124;
    if (period > 65535) period = 65535;
    if (v44) {
        struct VoiceHeader *vh = NULL;
        GETATTRS(o, { SDTA_VoiceHeader, (ULONG)&vh });
        if (vh) {                       /* for saving as 8SVX (DTM_WRITE) */
            vh->vh_OneShotHiSamples = len;
            vh->vh_RepeatHiSamples = 0;
            vh->vh_SamplesPerHiCycle = 0;
            vh->vh_SamplesPerSec = rate;
            vh->vh_Octaves = 1;
            vh->vh_Compression = CMP_NONE;
            vh->vh_Volume = Unity;
        }
        if (c.outch == 2)
            SETATTRS(o, { SDTA_LeftSample, (ULONG)c.out[0] }, { SDTA_RightSample, (ULONG)c.out[1] },
                     { SDTA_SampleLength, len }, { SDTA_SamplesPerSec, rate }, { SDTA_Volume, 64 },
                     { SDTA_Cycles, 1 });
        else
            SETATTRS(o, { SDTA_Sample, (ULONG)c.out[0] }, { SDTA_SampleLength, len },
                     { SDTA_SamplesPerSec, rate }, { SDTA_Volume, 64 }, { SDTA_Cycles, 1 });
    } else
        SETATTRS(o, { SDTA_Sample, (ULONG)c.out[0] }, { SDTA_SampleLength, len },
                 { SDTA_Period, period }, { SDTA_Volume, 64 }, { SDTA_Cycles, 1 });
    c.out[0] = c.out[1] = NULL;         /* sound.datatype frees them now (FreeVec) */
    rc = 0;
done:
    if (c.out[1]) FreeVec(c.out[1]);
    if (c.out[0]) FreeVec(c.out[0]);
    if (pcm) FreeVec(pcm);
    if (frame) FreeVec(frame);
    FreeVec(d);
    return rc;
}

/* not inlined into dispatch(): every method passes through dispatch, also
 * on the small stack of datatypes' layout process, several deep; it must
 * not reserve the loading code's stack each time */
static __attribute__((noinline)) ULONG m_new(Class *cl, Object *o, struct opSet *msg)
{
    LONG rc;
    if (!(o = (Object *)dosuper(cl, o, (Msg)msg))) return 0;
    if ((rc = load(o))) {
        ULONG m = OM_DISPOSE;
        CallHookPkt((struct Hook *)cl, o, (Msg)&m);     /* this class's dispose: the object goes */
        SetIoErr(rc);
        return 0;
    }
    return (ULONG)o;
}

/* OM_SET and OM_UPDATE as a datatype subclass must answer them (the
 * superclass renders only for objects of its own class): no loops through
 * the notification chain, then the superclass, then draw if the object is
 * of this class */
static __attribute__((noinline)) ULONG m_set(Class *cl, Object *o, struct opSet *msg)
{
    ULONG r;
    if (msg->MethodID == OM_UPDATE) {
        ULONG m = ICM_CHECKLOOP;
        if (CallHookPkt((struct Hook *)OCLASS(o), o, (Msg)&m)) return 0;
    }
    if ((r = dosuper(cl, o, (Msg)msg)) && OCLASS(o) == cl) {
        struct RastPort *rp = ObtainGIRPort(msg->ops_GInfo);
        if (rp) {
            struct gpRender g;
            g.MethodID = GM_RENDER;
            g.gpr_GInfo = msg->ops_GInfo;
            g.gpr_RPort = rp;
            g.gpr_Redraw = GREDRAW_UPDATE;
            CallHookPkt((struct Hook *)OCLASS(o), o, (Msg)&g);
            ReleaseGIRPort(rp);
        }
    }
    return r;
}

static ULONG dispatch(REG(a0, Class *cl), REG(a2, Object *o), REG(a1, Msg msg))
{
    switch (msg->MethodID) {
    case OM_NEW:
        return m_new(cl, o, (struct opSet *)msg);
    case OM_SET:
    case OM_UPDATE:
        return m_set(cl, o, (struct opSet *)msg);
    }
    return dosuper(cl, o, msg);
}

/* ---- the library ---------------------------------------------------- */

static struct Library *initlib(REG(d0, struct QoaBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys));
static struct Library *libopen(REG(a6, struct QoaBase *b));
static BPTR libclose(REG(a6, struct QoaBase *b));
static BPTR libexpunge(REG(a6, struct QoaBase *b));
static LONG libnull(void);
static Class *obtainengine(REG(a6, struct QoaBase *b));

static const APTR functable[] = {
    (APTR)libopen, (APTR)libclose, (APTR)libexpunge, (APTR)libnull,
    (APTR)obtainengine,
    (APTR)-1
};

static const ULONG inittab[4] = {
    sizeof(struct QoaBase), (ULONG)functable, 0, (ULONG)initlib
};

const struct Resident romtag = {
    RTC_MATCHWORD, (struct Resident *)&romtag, (APTR)(&romtag + 1),
    RTF_AUTOINIT, QA_VERSION, NT_LIBRARY, 0,
    (char *)libname, (char *)libid, (APTR)inittab
};

static void closeall(void)
{
    if (SoundDTBase) CloseLibrary(SoundDTBase);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    SoundDTBase = DataTypesBase = UtilityBase = NULL;
    IntuitionBase = NULL;
    DOSBase = NULL;
}

static struct Library *initlib(REG(d0, struct QoaBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys))
{
    SysBase = sys;
    b->lib.lib_Node.ln_Type = NT_LIBRARY;
    b->lib.lib_Node.ln_Name = (char *)libname;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = QA_VERSION;
    b->lib.lib_Revision = QA_REVISION;
    b->lib.lib_IdString = (APTR)libid;
    b->seglist = seg;
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    UtilityBase = OpenLibrary("utility.library", 39);
    DataTypesBase = OpenLibrary("datatypes.library", 39);
    SoundDTBase = OpenLibrary("datatypes/sound.datatype", 39);
    if (DOSBase && IntuitionBase && UtilityBase && DataTypesBase && SoundDTBase &&
        (qoaclass = MakeClass((STRPTR)libname, (STRPTR)SOUNDDTCLASS, NULL, 0, 0))) {
        qoaclass->cl_Dispatcher.h_Entry = (ULONG (*)())(APTR)dispatch;
        qoaclass->cl_UserData = (ULONG)b;
        AddClass(qoaclass);
        return &b->lib;
    }
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return NULL;
}

static struct Library *libopen(REG(a6, struct QoaBase *b))
{
    b->lib.lib_OpenCnt++;
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    return &b->lib;
}

static BPTR libexpunge(REG(a6, struct QoaBase *b))
{
    BPTR seg;
    if (b->lib.lib_OpenCnt || !FreeClass(qoaclass)) {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    qoaclass = NULL;
    seg = b->seglist;
    Remove(&b->lib.lib_Node);
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return seg;
}

static BPTR libclose(REG(a6, struct QoaBase *b))
{
    if (--b->lib.lib_OpenCnt == 0 && (b->lib.lib_Flags & LIBF_DELEXP))
        return libexpunge(b);
    return 0;
}

static LONG libnull(void)
{
    return 0;
}

static Class *obtainengine(REG(a6, struct QoaBase *b))
{
    (void)b;
    return qoaclass;
}
