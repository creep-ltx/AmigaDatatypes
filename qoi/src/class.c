/* qoi.datatype: QOI pictures (the "Quite OK Image" format) in MultiView,
 * file managers and every program that shows pictures through datatypes.
 *
 * A subclass of picture.datatype. The file is read in 16 KB pieces and
 * decoded a band of rows at a time (qoidec.c), so it never sits whole in
 * memory. Three ways to hand the pixels over, the best one available:
 *
 *  - picture.datatype 47 (OS 3.2): straight into the picture's own pixel
 *    buffer (PDTA_ObtainPixelBuffer) - no copy at all.
 *  - picture.datatype 43-46 (OS 3.5, 3.9, or 3.2 when that is refused):
 *    PDTM_WRITEPIXELARRAY, a band at a time.
 *  - picture.datatype 39-42 (OS 3.0, 3.1): no 24-bit pictures there, so
 *    256 colours: exactly the picture's own when it has that few, else a
 *    median-cut palette with dithering (quant.c). Decoded twice instead of
 *    kept whole.
 *
 * Transparency (alpha) is kept where picture.datatype knows it (47.9+);
 * on 3.0/3.1 the picture is shown over Workbench's grey. Pictures that
 * only say they have alpha but are opaque are told apart while decoding.
 *
 * The library part (Resident tag, jump table, ObtainEngine) is
 * markdown.datatype's. No C library. Runs on a 68000 and Kickstart 3.0. */
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/displayinfo.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/datatypes.h>
#include "qoidec.h"
#include "quant.h"
#include "version.h"

#define REG(r, a) a __asm(#r)
#define BAND 32768                      /* bytes of pixels handed over at a time */

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *DataTypesBase;
static struct Library *PictureDTBase;   /* picture.datatype, the superclass */
static Class *qoiclass;

struct QoiBase {
    struct Library lib;
    BPTR seglist;
};

static const char libname[] = "qoi.datatype";
static const char libid[] = "qoi.datatype " QD_VERSTR " (" QD_DATE ")\r\n";
const char verstag[] = "$VER: qoi.datatype " QD_VERSTR " (" QD_DATE ")";

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

static void *zalloc(ULONG n)
{
    return AllocVec(n, MEMF_ANY | MEMF_CLEAR);
}

/* ---- reading -------------------------------------------------------- */

static LONG readfile(void *fh, UBYTE *dst, LONG n)
{
    return Read((BPTR)fh, dst, n);
}


/* ---- OS 3.5 and later: 24-bit, with alpha ---------------------------- */

static LONG truecolour(Class *cl, Object *o, QoiDec *d, struct BitMapHeader *bmh)
{
    struct pdtBlitPixelArray direct, w;
    ULONG y, rows, fmt, bpp;
    UBYTE *band = NULL;
    int alpha = d->channels == 4;

    bmh->bmh_Depth = alpha ? 32 : 24;
    bmh->bmh_Masking = alpha ? mskHasAlpha : mskNone;

    /* OS 3.2: write into the picture's own buffer */
    direct.pbpa_PixelData = NULL;
    direct.pbpa_PixelFormat = direct.pbpa_PixelArrayMod = 0;
    direct.pbpa_Left = direct.pbpa_Top = direct.pbpa_Width = direct.pbpa_Height = 0;
    direct.MethodID = 0;
    if (PictureDTBase->lib_Version >= 47)
        SETATTRS(o, { PDTA_SourceMode, PMODE_V43 }, { DTA_NominalHoriz, d->width }, { DTA_NominalVert, d->height },
                 { PDTA_SubClassRendersAll, 1 }, { PDTA_ObtainPixelBuffer, (ULONG)&direct });
    else
        SETATTRS(o, { PDTA_SourceMode, PMODE_V43 }, { DTA_NominalHoriz, d->width }, { DTA_NominalVert, d->height });

    if (direct.pbpa_PixelData &&
        (direct.pbpa_PixelFormat == PBPAFMT_ARGB || direct.pbpa_PixelFormat == PBPAFMT_RGBA ||
         direct.pbpa_PixelFormat == PBPAFMT_RGB)) {
        UBYTE *row = direct.pbpa_PixelData;
        fmt = direct.pbpa_PixelFormat == PBPAFMT_ARGB ? QD_ARGB :
              direct.pbpa_PixelFormat == PBPAFMT_RGBA ? QD_RGBA : QD_RGB;
        bpp = fmt == QD_RGB ? 3 : 4;
        if (direct.pbpa_PixelArrayMod == d->width * bpp)
            qd_pixels(d, row, d->width * d->height, fmt);   /* one go */
        else
            for (y = 0; y < d->height; y++, row += direct.pbpa_PixelArrayMod)
                qd_pixels(d, row, d->width, fmt);
    } else {
        /* a band of rows at a time through PDTM_WRITEPIXELARRAY */
        bpp = alpha ? 4 : 3;
        fmt = alpha ? QD_RGBA : QD_RGB;
        rows = BAND / (d->width * bpp);
        if (!rows) rows = 1;
        if (rows > d->height) rows = d->height;
        if (!(band = AllocVec(d->width * bpp * rows, MEMF_ANY))) return ERROR_NO_FREE_STORE;
        w.MethodID = PDTM_WRITEPIXELARRAY;
        w.pbpa_PixelData = band;
        w.pbpa_PixelFormat = alpha ? PBPAFMT_RGBA : PBPAFMT_RGB;
        w.pbpa_PixelArrayMod = d->width * bpp;
        w.pbpa_Left = 0;
        w.pbpa_Width = d->width;
        for (y = 0; y < d->height; y += rows) {
            ULONG n = d->height - y < rows ? d->height - y : rows;
            qd_pixels(d, band, d->width * n, fmt);
            w.pbpa_Top = y;
            w.pbpa_Height = n;
            dosuper(cl, o, (Msg)&w);
        }
        FreeVec(band);
    }
    /* said alpha, but every pixel is opaque: show it as a plain picture */
    if (alpha && (d->alphaand & 255) == 255) {
        bmh->bmh_Masking = mskNone;
        bmh->bmh_Depth = 24;
    }
    return 0;
}

/* ---- OS 3.0 and 3.1: 256 colours ------------------------------------- */

/* one row of palette numbers into a planar bitmap's planes: 8 pixels make
 * one byte in each plane. Much faster than WritePixelLine8 in the 3.1 ROM
 * (about 140 ms a 256-pixel row on a 68020). */
static void planarrow(struct BitMap *bm, ULONG y, const UBYTE *ix, ULONG w)
{
    ULONG x, off = y * bm->BytesPerRow;
    int p, depth = bm->Depth;
    for (x = 0; x < w; x += 8) {
        UBYTE pl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, bit = 0x80;
        const UBYTE *s = ix + x;
        int k, n = w - x < 8 ? (int)(w - x) : 8;
        for (k = 0; k < n; k++, bit >>= 1) {
            UBYTE v = s[k];
            for (p = 0; v; p++, v >>= 1)
                if (v & 1) pl[p] |= bit;
        }
        for (p = 0; p < depth; p++) bm->Planes[p][off + (x >> 3)] = pl[p];
    }
}

/* the pixels of one row as RGBA numbers, transparency flattened */
static void rgbarow(QoiDec *d, ULONG *row)
{
    qd_pixels(d, (UBYTE *)row, d->width, QD_RGBA);
    if (d->channels == 4) qu_flatten(row, d->width);
}

static LONG palette(Object *o, QoiDec *d, BPTR fh, const UBYTE *head, ULONG size,
                    struct BitMapHeader *bmh)
{
    Quant *q;
    ULONG *row, y, i, n, depth;
    UBYTE *ix;
    WORD *err;
    struct BitMap *bm = NULL, *tbm = NULL;
    struct RastPort rp, trp;
    struct ColorRegister *cmap = NULL;
    ULONG *cregs = NULL;
    LONG rc = ERROR_NO_FREE_STORE;
    int planar;

    q = AllocVec(sizeof(Quant), MEMF_ANY);
    row = AllocVec(d->width * 4, MEMF_ANY);
    ix = AllocVec((d->width + 15) & ~15, MEMF_ANY | MEMF_CLEAR);
    err = zalloc(6 * (d->width + 2) * sizeof(WORD));
    if (!q || !row || !ix || !err) goto done;

    /* pass 1: which colours */
    qu_init(q);
    for (y = 0; y < d->height; y++) {
        rgbarow(d, row);
        qu_count(q, row, d->width);
    }
    qu_palette(q);
    /* an even number of colours: picture.datatype 40 puts PDTA_CRegs (longs)
     * right after PDTA_ColorRegisters (3 bytes a colour), so an odd count
     * gives an odd address, and a 68000 crashes writing it */
    n = q->ncolors ? (q->ncolors + 1) & ~1 : 2;
    for (depth = 1; (1UL << depth) < n; depth++) ;

    /* the bitmap; a plain planar one is written directly, any other (a
     * graphics card's) through WritePixelLine8 and a one-row bitmap */
    bm = AllocBitMap(d->width, d->height, depth, BMF_CLEAR, NULL);
    if (!bm) goto done;
    WaitBlit();                         /* BMF_CLEAR clears with the blitter: let it finish */
    planar = bm->Depth == depth && !(GetBitMapAttr(bm, BMA_FLAGS) & BMF_INTERLEAVED) &&
             (TypeOfMem(bm->Planes[0]) & MEMF_CHIP);     /* a card's bitmap only looks planar */
    if (!planar && !(tbm = AllocBitMap((d->width + 15) & ~15, 1, depth, 0, NULL))) goto done;
    InitRastPort(&rp);
    rp.BitMap = bm;
    trp = rp;
    trp.Layer = NULL;
    trp.BitMap = tbm;

    /* pass 2: from the start again, each row mapped and drawn */
    rc = ERROR_SEEK_ERROR;
    if (Seek(fh, QOI_HEADER, OFFSET_BEGINNING) < 0 || !qd_header(d, head, size, readfile, (void *)fh)) goto done;
    for (y = 0; y < d->height; y++) {
        rgbarow(d, row);
        qu_map(q, row, ix, d->width, err);
        if (planar) planarrow(bm, y, ix, d->width);
        else WritePixelLine8(&rp, 0, y, d->width, ix, &trp);
    }

    /* the palette */
    SETATTRS(o, { PDTA_NumColors, n });
    GETATTRS(o, { PDTA_ColorRegisters, (ULONG)&cmap }, { PDTA_CRegs, (ULONG)&cregs });
    rc = ERROR_NO_FREE_STORE;
    if (!cmap || !cregs) goto done;
    for (i = 0; i < n; i++) {
        UBYTE *c = q->pal[i < (ULONG)q->ncolors ? i : 0];
        cmap[i].red = c[0]; cmap[i].green = c[1]; cmap[i].blue = c[2];
        cregs[i * 3] = c[0] * 0x01010101UL;
        cregs[i * 3 + 1] = c[1] * 0x01010101UL;
        cregs[i * 3 + 2] = c[2] * 0x01010101UL;
    }
    bmh->bmh_Depth = depth;
    bmh->bmh_Masking = mskNone;
    SETATTRS(o, { DTA_NominalHoriz, d->width }, { DTA_NominalVert, d->height },
             { PDTA_ModeID, d->width >= 640 ? (d->height >= 400 ? HIRESLACE_KEY : HIRES_KEY) : LORES_KEY },
             { PDTA_BitMap, (ULONG)bm });
    bm = NULL;                          /* picture.datatype frees it now */
    rc = 0;
done:
    if (tbm) FreeBitMap(tbm);
    if (bm) FreeBitMap(bm);
    if (err) FreeVec(err);
    if (ix) FreeVec(ix);
    if (row) FreeVec(row);
    if (q) FreeVec(q);
    return rc;
}

/* ---- the object ------------------------------------------------------ */

static LONG load(Class *cl, Object *o)
{
    struct BitMapHeader *bmh = NULL;
    BPTR fh = 0;
    ULONG type = DTST_FILE;
    LONG size, rc;
    UBYTE head[QOI_HEADER];
    QoiDec *d;

    GETATTRS(o, { DTA_SourceType, (ULONG)&type }, { DTA_Handle, (ULONG)&fh }, { PDTA_BitMapHeader, (ULONG)&bmh });
    if (type != DTST_FILE) return ERROR_NOT_IMPLEMENTED;
    if (!fh || !bmh) return ERROR_REQUIRED_ARG_MISSING;
    if (Seek(fh, 0, OFFSET_END) < 0 || (size = Seek(fh, 0, OFFSET_BEGINNING)) < 0) return ERROR_SEEK_ERROR;
    if (Read(fh, head, QOI_HEADER) != QOI_HEADER) return DTERROR_NOT_ENOUGH_DATA;
    if (!(d = AllocVec(sizeof(QoiDec), MEMF_ANY))) return ERROR_NO_FREE_STORE;
    if (!qd_header(d, head, size, readfile, (void *)fh) || d->width > 65535 || d->height > 65535) {
        FreeVec(d);
        return DTERROR_INVALID_DATA;
    }
    bmh->bmh_Left = bmh->bmh_Top = 0;
    bmh->bmh_Width = bmh->bmh_PageWidth = d->width;
    bmh->bmh_Height = bmh->bmh_PageHeight = d->height;
    bmh->bmh_Compression = cmpNone;
    bmh->bmh_XAspect = bmh->bmh_YAspect = 1;
    /* the object's name: the file's, as ilbm.datatype gives it. MultiView on
     * OS 3.1 uses it without checking and crashes on none */
    {
        STRPTR name = NULL;
        GETATTRS(o, { DTA_Name, (ULONG)&name });
        if (name) SETATTRS(o, { DTA_ObjName, (ULONG)name });
    }
    if (PictureDTBase->lib_Version >= 43)
        rc = truecolour(cl, o, d, bmh);
    else
        rc = palette(o, d, fh, head, size, bmh);
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
    if ((rc = load(cl, o))) {
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

static struct Library *initlib(REG(d0, struct QoiBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys));
static struct Library *libopen(REG(a6, struct QoiBase *b));
static BPTR libclose(REG(a6, struct QoiBase *b));
static BPTR libexpunge(REG(a6, struct QoiBase *b));
static LONG libnull(void);
static Class *obtainengine(REG(a6, struct QoiBase *b));

static const APTR functable[] = {
    (APTR)libopen, (APTR)libclose, (APTR)libexpunge, (APTR)libnull,
    (APTR)obtainengine,
    (APTR)-1
};

static const ULONG inittab[4] = {
    sizeof(struct QoiBase), (ULONG)functable, 0, (ULONG)initlib
};

const struct Resident romtag = {
    RTC_MATCHWORD, (struct Resident *)&romtag, (APTR)(&romtag + 1),
    RTF_AUTOINIT, QD_VERSION, NT_LIBRARY, 0,
    (char *)libname, (char *)libid, (APTR)inittab
};

static void closeall(void)
{
    if (PictureDTBase) CloseLibrary(PictureDTBase);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    if (DOSBase) CloseLibrary((struct Library *)DOSBase);
    PictureDTBase = DataTypesBase = UtilityBase = NULL;
    GfxBase = NULL;
    IntuitionBase = NULL;
    DOSBase = NULL;
}

static struct Library *initlib(REG(d0, struct QoiBase *b), REG(a0, BPTR seg), REG(a6, struct ExecBase *sys))
{
    SysBase = sys;
    b->lib.lib_Node.ln_Type = NT_LIBRARY;
    b->lib.lib_Node.ln_Name = (char *)libname;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = QD_VERSION;
    b->lib.lib_Revision = QD_REVISION;
    b->lib.lib_IdString = (APTR)libid;
    b->seglist = seg;
    qd_use020 = (sys->AttnFlags & AFF_68020) != 0;     /* the assembly loop (fast020.s) */
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    UtilityBase = OpenLibrary("utility.library", 39);
    DataTypesBase = OpenLibrary("datatypes.library", 39);
    PictureDTBase = OpenLibrary("datatypes/picture.datatype", 39);
    if (DOSBase && IntuitionBase && GfxBase && UtilityBase && DataTypesBase && PictureDTBase &&
        (qoiclass = MakeClass((STRPTR)libname, (STRPTR)PICTUREDTCLASS, NULL, 0, 0))) {
        qoiclass->cl_Dispatcher.h_Entry = (ULONG (*)())(APTR)dispatch;
        qoiclass->cl_UserData = (ULONG)b;
        AddClass(qoiclass);
        return &b->lib;
    }
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return NULL;
}

static struct Library *libopen(REG(a6, struct QoiBase *b))
{
    b->lib.lib_OpenCnt++;
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    return &b->lib;
}

static BPTR libexpunge(REG(a6, struct QoiBase *b))
{
    BPTR seg;
    if (b->lib.lib_OpenCnt || !FreeClass(qoiclass)) {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    qoiclass = NULL;
    seg = b->seglist;
    Remove(&b->lib.lib_Node);
    closeall();
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, b->lib.lib_NegSize + b->lib.lib_PosSize);
    return seg;
}

static BPTR libclose(REG(a6, struct QoiBase *b))
{
    if (--b->lib.lib_OpenCnt == 0 && (b->lib.lib_Flags & LIBF_DELEXP))
        return libexpunge(b);
    return 0;
}

static LONG libnull(void)
{
    return 0;
}

static Class *obtainengine(REG(a6, struct QoiBase *b))
{
    (void)b;
    return qoiclass;
}
