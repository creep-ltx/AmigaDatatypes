/* utf8.datatype's recognition code, the DTCD chunk of its descriptors.
 * datatypes.library calls it for a file no other text datatype claimed:
 * TRUE = the file is UTF-8 text with at least one character beyond ASCII
 * (or a byte order mark). Plain ASCII is left to ascii.datatype, and any
 * byte that is not good UTF-8 (RFC 3629), or a 0 byte, means it is not
 * ours: Latin-1 text and binary files fail at their first such byte.
 *
 * An AmigaGuide file (starting "@database") is left to amigaguide.datatype,
 * whatever its name: datatypes.library asks the descriptors in the order
 * of DEVS:DataTypes, so this one may be asked first.
 *
 * Reads up to SCAN bytes from the file's start, then puts the file back
 * where it was. Without a file handle (the clipboard) only the bytes
 * datatypes.library already read. No globals and no C library: the code is
 * loaded on its own, the libraries come in the context. */
#include <exec/memory.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#define __NOLIBBASE__
#include <proto/exec.h>
#include <proto/dos.h>

#define SCAN  16384     /* how much of the file decides */
#define CHUNK 1024

struct Scan {
    int need;           /* continuation bytes still to come */
    UBYTE lo, hi;       /* the range of the next one */
    int high;           /* seen a character beyond ASCII */
};

/* -1: not UTF-8 text; 0: good so far */
static int scan(struct Scan *s, const UBYTE *p, long n)
{
    while (n-- > 0) {
        UBYTE c = *p++;
        if (s->need) {
            if (c < s->lo || c > s->hi) return -1;
            s->lo = 0x80;
            s->hi = 0xBF;
            s->need--;
            continue;
        }
        if (c < 0x80) {
            if (!c) return -1;
            continue;
        }
        s->high = 1;
        s->lo = 0x80;
        s->hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) s->need = 1;
        else if (c >= 0xE0 && c <= 0xEF) {
            s->need = 2;
            if (c == 0xE0) s->lo = 0xA0;            /* no overlong forms */
            else if (c == 0xED) s->hi = 0x9F;       /* no surrogates */
        } else if (c >= 0xF0 && c <= 0xF4) {
            s->need = 3;
            if (c == 0xF0) s->lo = 0x90;
            else if (c == 0xF4) s->hi = 0x8F;       /* nothing past U+10FFFF */
        } else return -1;
    }
    return 0;
}

LONG utf8hook(struct DTHookContext *d)
{
    struct ExecBase *SysBase = (struct ExecBase *)d->dthc_SysBase;
    struct DosLibrary *DOSBase = (struct DosLibrary *)d->dthc_DOSBase;
    struct Scan s;
    BPTR fh = d->dthc_FileHandle;
    UBYTE *buf;
    LONG old, got, total = 0;
    int bad = 0;

    if (d->dthc_Buffer && d->dthc_BufferLength >= 9) {
        static const char db[] = "@DATABASE";
        int i;
        for (i = 0; i < 9 && (d->dthc_Buffer[i] & 0xDF) == (UBYTE)(db[i] & 0xDF); i++)
            ;
        if (i == 9) return FALSE;
    }
    s.need = 0;
    s.high = 0;
    /* what was read already: most files that are not ours end here */
    if (d->dthc_Buffer && scan(&s, d->dthc_Buffer, d->dthc_BufferLength) < 0) return FALSE;
    if (!fh) return s.high;
    if (!(buf = AllocMem(CHUNK, MEMF_ANY))) return s.high;
    s.need = 0;
    s.high = 0;
    if ((old = Seek(fh, 0, OFFSET_BEGINNING)) >= 0) {
        while (total < SCAN && (got = Read(fh, buf, CHUNK)) > 0) {
            if (scan(&s, buf, got) < 0) {
                bad = 1;
                break;
            }
            total += got;
        }
        Seek(fh, old, OFFSET_BEGINNING);
    }
    FreeMem(buf, CHUNK);
    /* a character cut at SCAN or at the end of a cut-short file is fine */
    return !bad && s.high;
}
