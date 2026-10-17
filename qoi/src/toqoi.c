/* ToQOI: turns pictures into QOI files. The picture is read through
 * datatypes, so every format the Amiga has a picture datatype for works
 * (PNG, JPEG, GIF, BMP, WebP, ILBM...); the QOI is written by qoienc.c.
 *
 *   ToQOI FROM/A/M,TO/K,FORCE/S,QUIET/S
 *
 * From Workbench: select pictures, then shift-double-click ToQOI; each QOI
 * goes beside its picture and a requester tells how it went.
 *
 * Neither the picture nor the file is held whole: on picture.datatype 43
 * and later (OS 3.5+) the pixels are read as full colour a band of rows at
 * a time; on 3.0/3.1 (and for palette pictures with a transparent colour)
 * the class's bitmap and palette are read a row at a time, HAM and
 * Extra-Halfbrite included. */
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <workbench/startup.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <graphics/gfx.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <string.h>
#include <stdarg.h>
#include "qoienc.h"

#define TQ_VERSTR "0.1b1"
#define TQ_DATE   "17.10.2026"

static const char version[] __attribute__((used)) = "$VER: ToQOI " TQ_VERSTR " (" TQ_DATE ")";
unsigned long __stack = 32768;          /* datatypes run on the caller's stack (libnix swaps) */

struct Library *DataTypesBase;
static ULONG pdtver;                    /* picture.datatype's version */

/* what convert() gives */
#define OK      0
#define SKIPPED 1
#define FAILED  2
#define BROKEN  3                       /* Ctrl-C: stop everything */

static int quiet, force;
static int fromwb;
static char wbtext[1024];               /* the requester's text, from Workbench */
static int nmade, nskipped, nfailed;

/* RawDoFmt's character routine: move.b d0,(a3)+ ; rts. Words, not a
 * string: code must start at an even address. */
static const UWORD putch[] = { 0x16c0, 0x4e75 };
#define PUTCH ((void (*)())putch)

#ifdef DEBUG
/* RawPutChar through exec: move.l a6,-(sp); move.l 4.w,a6; jsr -516(a6); move.l (sp)+,a6; rts */
static void dbg(const char *fmt, ...)
{
    char b[200];
    BPTR f;
    va_list ap;
    va_start(ap, fmt);
    RawDoFmt((STRPTR)fmt, (APTR)ap, PUTCH, b);
    va_end(ap);
    if ((f = Open((STRPTR)"SYS:tq.log", MODE_READWRITE))) {
        Seek(f, 0, OFFSET_END);
        Write(f, b, strlen(b));
        Close(f);
    }
}
#define D(x) dbg x
#else
#define D(x)
#endif

/* va_list on the 68k is a pointer to the arguments, as RawDoFmt and
 * VPrintf want them (all longs: %ld, %lu, %s) */
static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (fromwb) {
        ULONG len = strlen(wbtext);
        if (len < sizeof(wbtext) - 200)
            RawDoFmt((STRPTR)fmt, (APTR)ap, PUTCH, wbtext + len);
    } else
        VPrintf((STRPTR)fmt, (APTR)ap);
    va_end(ap);
}

/* ---- writing the file -------------------------------------------------- */

static int fhwrite(void *ctx, const UBYTE *src, LONG n)
{
    return Write((BPTR)ctx, (APTR)src, n) == n;
}

/* ---- reading the picture ----------------------------------------------- */

typedef struct {
    Object *o;
    ULONG w, h;
    int alpha;                          /* the source may have transparency */
    int planar;                         /* read the bitmap ourselves (else PDTM_READPIXELARRAY) */
    int lut;                            /* PDTM_READPIXELARRAY as colour numbers + the palette */
    struct BitMap *bm;
    int depth, ham, ehb;
    LONG trans;                         /* the transparent colour, or -1 */
    UBYTE pal[256][4];                  /* RGBA */
    UBYTE *idx;                         /* one row of colour numbers */
} Pic;

/* one row of colour numbers from a planar bitmap */
static void planarrow(Pic *p, ULONG y, UBYTE *idx)
{
    struct BitMap *bm = p->bm;
    ULONG nb = (p->w + 7) >> 3, xb;
    int pl;

    memset(idx, 0, nb * 8);
    for (pl = 0; pl < p->depth; pl++) {
        UBYTE *s = bm->Planes[pl], bit = 1 << pl, *q = idx;
        if (!s)
            continue;
        if (s == (UBYTE *)-1) {         /* a plane of all ones */
            for (xb = 0; xb < nb * 8; xb++)
                idx[xb] |= bit;
            continue;
        }
        s += y * bm->BytesPerRow;
        for (xb = 0; xb < nb; xb++, q += 8) {
            UBYTE v = s[xb];
            if (!v)
                continue;
            if (v & 0x80) q[0] |= bit;
            if (v & 0x40) q[1] |= bit;
            if (v & 0x20) q[2] |= bit;
            if (v & 0x10) q[3] |= bit;
            if (v & 0x08) q[4] |= bit;
            if (v & 0x04) q[5] |= bit;
            if (v & 0x02) q[6] |= bit;
            if (v & 0x01) q[7] |= bit;
        }
    }
}

/* n rows from y on, as RGBA, into buf. 0 = the datatype refused. */
static int getrows(Pic *p, ULONG y, ULONG n, UBYTE *buf)
{
    ULONG x, r;

    if (p->lut) {
        for (r = 0; r < n; r++, y++) {
            UBYTE *q = buf + r * p->w * 4;
            if (!DoMethod(p->o, PDTM_READPIXELARRAY, (ULONG)p->idx, PBPAFMT_LUT8, p->w, 0, y, p->w, 1))
                return 0;
            for (x = 0; x < p->w; x++, q += 4)
                memcpy(q, p->pal[p->idx[x]], 4);
        }
        return 1;
    }
    if (!p->planar)
        return DoMethod(p->o, PDTM_READPIXELARRAY, (ULONG)buf, PBPAFMT_RGBA, p->w * 4, 0, y, p->w, n) != 0;
    for (r = 0; r < n; r++, y++) {
        UBYTE *q = buf + r * p->w * 4;
        planarrow(p, y, p->idx);
        if (p->ham) {                   /* each pixel changes one gun of the one before */
            UBYTE cr = p->pal[0][0], cg = p->pal[0][1], cb = p->pal[0][2];
            int sh = p->depth - 2, mask = (1 << sh) - 1;
            for (x = 0; x < p->w; x++, q += 4) {
                UBYTE c = p->idx[x], d = c & mask;
                UBYTE v = sh == 4 ? d * 17 : (d << 2) | (d >> 4);
                switch (c >> sh) {
                case 0: cr = p->pal[d][0]; cg = p->pal[d][1]; cb = p->pal[d][2]; break;
                case 1: cb = v; break;
                case 2: cr = v; break;
                case 3: cg = v; break;
                }
                q[0] = cr; q[1] = cg; q[2] = cb; q[3] = 255;
            }
        } else {
            for (x = 0; x < p->w; x++, q += 4)
                memcpy(q, p->pal[p->idx[x]], 4);
        }
    }
    return 1;
}

/* the palette, the transparent colour and a row of colour numbers;
 * 0 = no palette or no memory */
static int setuppalette(Pic *p, struct BitMapHeader *bmh)
{
    ULONG *cregs = NULL, ncol = 0, i;

    GetDTAttrs(p->o, PDTA_CRegs, (ULONG)&cregs, PDTA_NumColors, (ULONG)&ncol, TAG_DONE);
    D(("tq: cregs %lx ncol %ld\n", cregs, ncol));
    if (!cregs || !ncol)
        return 0;
    memset(p->pal, 0, sizeof(p->pal));
    if (ncol > 256)
        ncol = 256;
    for (i = 0; cregs && i < ncol; i++) {
        p->pal[i][0] = cregs[i * 3] >> 24;
        p->pal[i][1] = cregs[i * 3 + 1] >> 24;
        p->pal[i][2] = cregs[i * 3 + 2] >> 24;
    }
    for (i = 0; i < 256; i++)
        p->pal[i][3] = 255;
    if (p->ehb)                         /* what the chips show, whatever the class gave for 32-63 */
        for (i = 0; i < 32; i++) {
            p->pal[i + 32][0] = p->pal[i][0] >> 1;
            p->pal[i + 32][1] = p->pal[i][1] >> 1;
            p->pal[i + 32][2] = p->pal[i][2] >> 1;
        }
    p->trans = -1;
    if (bmh->bmh_Masking == mskHasTransparentColor && !p->ham) {
        p->trans = bmh->bmh_Transparent & 255;
        p->pal[p->trans][3] = 0;
        p->alpha = 1;
    }
    if (!p->idx)
        p->idx = AllocVec(((p->w + 7) & ~7) + 8, MEMF_ANY);
    return p->idx != NULL;
}

/* how to read the class's bitmap, and its palette; 0 = it can't be read */
static int setupplanar(Pic *p, struct BitMapHeader *bmh)
{
    ULONG modeid = INVALID_ID;

    GetDTAttrs(p->o, PDTA_ModeID, (ULONG)&modeid, TAG_DONE);
    D(("tq: flags %lx depth %ld mode %lx\n", GetBitMapAttr(p->bm, BMA_FLAGS), GetBitMapAttr(p->bm, BMA_DEPTH), modeid));
    if (!(GetBitMapAttr(p->bm, BMA_FLAGS) & BMF_STANDARD))
        return 0;
    p->depth = GetBitMapAttr(p->bm, BMA_DEPTH);
    if (p->depth > 8)
        p->depth = 8;
    if (modeid != INVALID_ID) {
        p->ham = (modeid & HAM_KEY) && (p->depth == 6 || p->depth == 8);
        p->ehb = (modeid & EXTRAHALFBRITE_KEY) && p->depth == 6;
    }
    return setuppalette(p, bmh);
}

/* the picture as an object: full colour (picture.datatype 43+) or the
 * class's own bitmap and palette */
static Object *opendt(const char *src, int v43)
{
    return NewDTObject((APTR)src, DTA_GroupID, GID_PICTURE, PDTA_Remap, FALSE,
                       v43 ? PDTA_DestMode : TAG_IGNORE, PMODE_V43, TAG_DONE);
}

/* ---- one picture ------------------------------------------------------- */

static void sizetext(char *buf, ULONG n)
{
    struct { ULONG a, b; } v;
    v.a = n;
    if (n < 10240)
        RawDoFmt((STRPTR)"%lu bytes", &v, PUTCH, buf);
    else {
        v.a = (n + 512) >> 10;
        RawDoFmt((STRPTR)"%lu KB", &v, PUTCH, buf);
    }
}

static void faultsay(const char *src, const char *what, LONG err)
{
    char buf[100];
    if (err >= DTERROR_UNKNOWN_DATATYPE && err <= DTERROR_NOT_AVAILABLE) {
        STRPTR s = GetDTString(err);
        strncpy(buf, s ? (char *)s : "datatypes error", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
    } else if (err)
        Fault(err, NULL, (STRPTR)buf, sizeof(buf));
    else
        strcpy(buf, "unknown error");
    say("%s: %s: %s\n", src, what, buf);
}

static ULONG filesize(BPTR lock)
{
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    ULONG n = 0;
    if (fib) {
        if (Examine(lock, fib))
            n = fib->fib_Size;
        FreeDosObject(DOS_FIB, fib);
    }
    return n;
}

static int convert(const char *src, const char *dst)
{
    BPTR lock, fh;
    UBYTE magic[4];
    LONG got;
    ULONG insize, outsize, band, y, err = 0;
    Pic p;
    struct BitMapHeader *bmh = NULL;
    QoiEnc *e = NULL;
    UBYTE *buf = NULL;
    int ret = FAILED, stopped = 0;
    char a[24], b[24];

    D(("tq: convert %s\n", src));
    if (!(lock = Lock((STRPTR)src, SHARED_LOCK))) {
        faultsay(src, "can't open", IoErr());
        return FAILED;
    }
    insize = filesize(lock);
    UnLock(lock);
    if ((fh = Open((STRPTR)src, MODE_OLDFILE))) {
        got = Read(fh, magic, 4);
        Close(fh);
        if (got == 4 && !memcmp(magic, "qoif", 4)) {
            say("%s: already QOI\n", src);
            return SKIPPED;
        }
    }
    D(("tq: size %ld\n", insize));
    if (!Stricmp((STRPTR)dst, (STRPTR)src)) {
        say("%s: would replace itself\n", src);
        return SKIPPED;
    }
    if (!force && (lock = Lock((STRPTR)dst, SHARED_LOCK))) {
        UnLock(lock);
        say("%s: %s is there already (FORCE replaces it)\n", src, dst);
        return SKIPPED;
    }

    memset(&p, 0, sizeof(p));
    D(("tq: new %s pdt %ld\n", src, pdtver));
    p.o = opendt(src, pdtver >= 43);
    if (!p.o) {
        err = IoErr();
        if (!err || err == DTERROR_UNKNOWN_DATATYPE || err == ERROR_OBJECT_WRONG_TYPE) {
            say("%s: not a picture this Amiga can read\n", src);
            return SKIPPED;
        }
        faultsay(src, "can't read the picture", err);
        return FAILED;
    }
    GetDTAttrs(p.o, PDTA_BitMapHeader, (ULONG)&bmh, PDTA_BitMap, (ULONG)&p.bm, TAG_DONE);
    if (pdtver < 43 && bmh && !p.bm) {
        /* picture.datatype 40 hands out the bitmap only after a layout
         * (without a screen and without remapping: the picture as it is) */
        DoDTMethod(p.o, NULL, NULL, DTM_PROCLAYOUT, NULL, 1);
        GetDTAttrs(p.o, PDTA_BitMap, (ULONG)&p.bm, TAG_DONE);
    }
    D(("tq: bmh %lx bm %lx\n", bmh, p.bm));
    if (!bmh || !bmh->bmh_Width || !bmh->bmh_Height) {
        say("%s: the picture has no size\n", src);
        goto out;
    }
    p.w = bmh->bmh_Width;
    p.h = bmh->bmh_Height;
    if (pdtver >= 43) {
        ULONG ach = FALSE;
        GetDTAttrs(p.o, PDTA_AlphaChannel, (ULONG)&ach, TAG_DONE);
        p.alpha = bmh->bmh_Masking == mskHasAlpha || ach;
        /* a palette picture with a transparent colour: full colour pixels
         * come without the transparency (picture.datatype 47), so read
         * colour numbers and the palette */
        if (bmh->bmh_Masking == mskHasTransparentColor && setuppalette(&p, bmh)) {
            p.lut = 1;
            if (!DoMethod(p.o, PDTM_READPIXELARRAY, (ULONG)p.idx, PBPAFMT_LUT8, p.w, 0, 0, p.w, 1))
                p.lut = p.alpha = 0;
        }
    } else {
        p.planar = 1;
        if (!p.bm || !setupplanar(&p, bmh)) {
            say("%s: this kind of picture needs OS 3.5 or later\n", src);
            goto out;
        }
    }

    D(("tq: %ldx%ld mask %ld planar %ld alpha %ld\n", p.w, p.h, (LONG)bmh->bmh_Masking, (LONG)p.planar, (LONG)p.alpha));
    band = p.planar || p.lut ? 1 : 32768 / (p.w * 4);
    if (band < 1) band = 1;
    if (band > p.h) band = p.h;
    if (!(buf = AllocVec(p.w * 4 * band, MEMF_ANY)) || !(e = AllocVec(sizeof(*e), MEMF_ANY))) {
        say("%s: not enough memory\n", src);
        goto out;
    }
    if (!(fh = Open((STRPTR)dst, MODE_NEWFILE))) {
        faultsay(dst, "can't write", IoErr());
        goto out;
    }
    qe_begin(e, p.w, p.h, p.alpha ? 4 : 3, fhwrite, (void *)fh);
    for (y = 0; y < p.h && !e->failed; y += band) {
        ULONG n = p.h - y < band ? p.h - y : band;
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            stopped = 1;
            break;
        }
        D(("tq: rows %ld\n", y));
        if (!getrows(&p, y, n, buf)) {
            say("%s: the datatype gave no pixels\n", src);
            break;
        }
        qe_pixels(e, buf, p.w * n, !p.alpha);
    }
    D(("tq: end\n"));
    if (y >= p.h && qe_end(e)) {
        /* it said transparency but every pixel is solid: a plain picture */
        if (p.alpha && e->alphaand == 255 && Seek(fh, 12, OFFSET_BEGINNING) >= 0)
            Write(fh, (APTR)"\3", 1);
        Seek(fh, 0, OFFSET_END);
        outsize = Seek(fh, 0, OFFSET_CURRENT);
        Close(fh);
        if (!quiet) {
            sizetext(a, insize);
            sizetext(b, outsize);
            say("%s -> %s: %lux%lu%s, %s -> %s\n", src, dst, p.w, p.h,
                p.alpha && e->alphaand != 255 ? " with transparency" : "", a, b);
        }
        ret = OK;
    } else {
        err = IoErr();
        Close(fh);
        DeleteFile((STRPTR)dst);
        if (stopped) {
            say("%s: stopped\n", src);
            ret = BROKEN;
        } else if (e->failed)
            faultsay(dst, "can't write", err);
    }
out:
    FreeVec(e);
    FreeVec(buf);
    FreeVec(p.idx);
    DisposeDTObject(p.o);
    return ret;
}

/* ---- names ------------------------------------------------------------- */

/* the picture's name with .qoi for its ending, in dir (or beside it) */
static void destname(const char *src, const char *dir, char *dst, ULONG size)
{
    const char *fp = (const char *)FilePart((STRPTR)src);
    char *dot;

    if (dir) {
        strncpy(dst, dir, size - 1);
        dst[size - 1] = 0;
        AddPart((STRPTR)dst, (STRPTR)fp, size - 5);
    } else {
        strncpy(dst, src, size - 5);
        dst[size - 5] = 0;
    }
    fp = (const char *)FilePart((STRPTR)dst);
    dot = strrchr(fp, '.');
    if (dot && dot != fp)
        *dot = 0;
    strcat(dst, ".qoi");
}

static int one(const char *src, const char *todir, const char *tofile)
{
    char dst[512];
    int r;

    if (tofile) {
        strncpy(dst, tofile, sizeof(dst) - 1);
        dst[sizeof(dst) - 1] = 0;
    } else
        destname(src, todir, dst, sizeof(dst));
    r = convert(src, dst);
    if (r == OK) nmade++;
    else if (r == SKIPPED) nskipped++;
    else nfailed++;
    return r;
}

static int isdir(const char *name)
{
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
    struct FileInfoBlock *fib;
    int r = 0;
    if (!lock)
        return 0;
    if ((fib = AllocDosObject(DOS_FIB, NULL))) {
        if (Examine(lock, fib))
            r = fib->fib_DirEntryType > 0;
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return r;
}

static int haswild(const char *s)
{
    char tmp[256];
    return ParsePatternNoCase((STRPTR)s, (STRPTR)tmp, sizeof(tmp)) == 1;
}

static int endsinfo(const char *s)
{
    ULONG n = strlen(s);
    return n > 5 && !Stricmp((STRPTR)s + n - 5, (STRPTR)".info");
}

/* ---- the command ------------------------------------------------------- */

static int fromshell(void)
{
    LONG args[4] = { 0, 0, 0, 0 };
    struct RDArgs *rda = ReadArgs((STRPTR)"FROM/A/M,TO/K,FORCE/S,QUIET/S", args, NULL);
    char **from, *to, *todir = NULL, *tofile = NULL;
    int i, n, many, rc = RETURN_OK, r = OK;

    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"ToQOI");
        return RETURN_FAIL;
    }
    from = (char **)args[0];
    to = (char *)args[1];
    force = args[2] != 0;
    quiet = args[3] != 0;
    for (n = 0; from[n]; n++)
        ;
    many = n > 1;
    for (i = 0; i < n; i++)
        if (haswild(from[i]))
            many = 1;
    if (to) {
        if (isdir(to))
            todir = to;
        else if (many) {
            Printf((STRPTR)"ToQOI: TO must be a directory when there are several pictures\n");
            FreeArgs(rda);
            return RETURN_FAIL;
        } else
            tofile = to;
    }

    for (i = 0; i < n && r != BROKEN; i++) {
        if (!haswild(from[i])) {
            r = one(from[i], todir, tofile);
            continue;
        }
        {
            struct AnchorPath *ap = AllocVec(sizeof(struct AnchorPath) + 512, MEMF_CLEAR);
            LONG e;
            int found = 0;
            if (!ap)
                break;
            ap->ap_Strlen = 512;
            ap->ap_BreakBits = SIGBREAKF_CTRL_C;
            for (e = MatchFirst((STRPTR)from[i], ap); !e && r != BROKEN; e = MatchNext(ap)) {
                if (ap->ap_Info.fib_DirEntryType > 0 || endsinfo((char *)ap->ap_Buf))
                    continue;
                found = 1;
                r = one((char *)ap->ap_Buf, todir, NULL);
            }
            MatchEnd(ap);
            FreeVec(ap);
            if (e == ERROR_BREAK) {
                Printf((STRPTR)"***Break\n");
                r = BROKEN;
            } else if (!found && r != BROKEN) {
                Printf((STRPTR)"ToQOI: nothing matches %s\n", from[i]);
                nskipped++;
            }
        }
    }
    if (r == BROKEN)
        rc = RETURN_WARN;
    else if (nfailed)
        rc = RETURN_ERROR;
    else if (nskipped)
        rc = RETURN_WARN;
    FreeArgs(rda);
    return rc;
}

static void fromworkbench(struct WBStartup *wbs)
{
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"ToQOI", (UBYTE *)"%s", (UBYTE *)"OK" };
    char tail[100];
    LONG i;

    fromwb = 1;
    quiet = 1;                          /* only what went wrong goes in the requester */
    for (i = 1; i < wbs->sm_NumArgs; i++) {
        struct WBArg *wa = &wbs->sm_ArgList[i];
        BPTR old;
        int r;
        if (!wa->wa_Lock || !wa->wa_Name[0])
            continue;                   /* a drawer or disk */
        old = CurrentDir(wa->wa_Lock);
        r = one((char *)wa->wa_Name, NULL, NULL);
        CurrentDir(old);
        if (r == BROKEN)
            break;
    }
    if (wbs->sm_NumArgs < 2) {
        strcpy(wbtext, "Select pictures, then hold Shift and\ndouble-click ToQOI.\n"
                       "Each one is saved as a QOI picture beside it.\n");
    } else {
        struct { LONG n; } v = { nmade };
        RawDoFmt((STRPTR)(nmade == 1 ? "Made %ld QOI picture." : "Made %ld QOI pictures."), &v,
                 PUTCH, tail);
        strcat(wbtext, tail);
    }
    EasyRequest(NULL, &es, NULL, (ULONG)wbtext);
}

int main(int argc, char **argv)
{
    struct Library *pdt;
    int rc = RETURN_OK;

    if (!(DataTypesBase = OpenLibrary((STRPTR)"datatypes.library", 39))) {
        if (argc)
            Printf((STRPTR)"ToQOI needs datatypes.library (OS 3.0 or later)\n");
        return RETURN_FAIL;
    }
    if ((pdt = OpenLibrary((STRPTR)"datatypes/picture.datatype", 0))) {
        pdtver = pdt->lib_Version;
        CloseLibrary(pdt);
    }
    if (argc)
        rc = fromshell();
    else
        fromworkbench((struct WBStartup *)argv);
    CloseLibrary(DataTypesBase);
    return rc;
}
