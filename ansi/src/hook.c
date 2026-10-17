/* ansi.datatype's recognition code, the DTCD chunk of its descriptors.
 * TRUE = PC ANSI art (or CP437 text art):
 *
 * - a file with a SAUCE record: when it says character art (ASCII, ANSI,
 *   ANSiMation) and does not name an Amiga font ("Amiga Topaz 1" ...);
 * - without a record, by name and bytes: .ans and .ice unless the bytes
 *   look Amiga (guess.c); .nfo, .diz and any other name only when there
 *   are PC bytes (block and line characters) and they look PC, and for
 *   other names an ESC [ too. Plain ASCII without a record is left to the
 *   text datatypes: it may well be Amiga ANSI.
 *
 * Reads the record from the end and up to SCAN bytes from the start, then
 * puts the file back where it was. Without a file handle (the clipboard)
 * only the bytes datatypes.library already read. No globals and no C
 * library: the code is loaded on its own, the libraries come in the
 * context. */
#include <exec/memory.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#define __NOLIBBASE__
#include <proto/exec.h>
#include <proto/dos.h>
#include "guess.h"

#define SCAN 8192

LONG ansihook(struct DTHookContext *d)
{
    struct ExecBase *SysBase = (struct ExecBase *)d->dthc_SysBase;
    struct DosLibrary *DOSBase = (struct DosLibrary *)d->dthc_DOSBase;
    BPTR fh = d->dthc_FileHandle;
    const char *name = d->dthc_FIB ? (const char *)d->dthc_FIB->fib_FileName : "";
    const UBYTE *tail = NULL;
    UBYTE *buf;
    LONG old = -1, n, i, size = d->dthc_FIB ? d->dthc_FIB->fib_Size : 0;
    int r = 0, nul = 0;

    /* most files that are not ours end here: binary data has a NUL early */
    if (d->dthc_Buffer)
        for (i = 0; i < (LONG)d->dthc_BufferLength && d->dthc_Buffer[i] != 0x1A; i++)
            if (!d->dthc_Buffer[i]) {
                nul = 1;
                break;
            }
    if (!fh) {
        if (nul || !d->dthc_Buffer) return FALSE;
        return ansi_claim(name, NULL, d->dthc_Buffer, d->dthc_BufferLength);
    }
    if (!(buf = AllocMem(SCAN + 128, MEMF_ANY))) return FALSE;
    old = Seek(fh, 0, OFFSET_CURRENT);
    if (size >= 128 && Seek(fh, size - 128, OFFSET_BEGINNING) >= 0 && Read(fh, buf + SCAN, 128) == 128)
        tail = buf + SCAN;
    if (nul && !(tail && tail[0] == 'S' && tail[1] == 'A')) goto done;
    if (Seek(fh, 0, OFFSET_BEGINNING) >= 0 && (n = Read(fh, buf, SCAN)) > 0)
        r = ansi_claim(name, tail, buf, n);
done:
    if (old >= 0) Seek(fh, old, OFFSET_BEGINNING);
    FreeMem(buf, SCAN + 128);
    return r;
}
