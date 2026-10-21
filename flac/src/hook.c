/* flac.datatype's recognition code, the DTCD chunk of the FLAC-ID3
 * descriptor (its mask: "ID3"). TRUE = a FLAC stream behind one or more
 * ID3v2 tags, which some taggers write into FLAC files. An MP3 or any
 * other file with an ID3 tag gives FALSE and is left to its own datatype.
 * Plain FLAC files ("fLaC" at the start) are the FLAC descriptor's, which
 * has no code.
 *
 * Reads each tag's 10-byte header and the 4 bytes after the last one,
 * then puts the file back where it was. No globals and no C library: the
 * code is loaded on its own, the libraries come in the context. */
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#define __NOLIBBASE__
#include <proto/dos.h>
#include "id3.h"

LONG id3hook(struct DTHookContext *d)
{
    struct DosLibrary *DOSBase = (struct DosLibrary *)d->dthc_DOSBase;
    BPTR fh = d->dthc_FileHandle;
    UBYTE b[10];
    LONG old, pos = 0, tags;
    ULONG len;
    int r = FALSE;

    if (!fh) return FALSE;              /* the clipboard: no FLAC there */
    if ((old = Seek(fh, 0, OFFSET_CURRENT)) < 0) return FALSE;
    for (tags = 0; tags <= FS_MAXTAGS; tags++) {
        if (Seek(fh, pos, OFFSET_BEGINNING) < 0 || Read(fh, b, 10) < 4) break;
        if (b[0] == 'f' && b[1] == 'L' && b[2] == 'a' && b[3] == 'C') {
            r = tags > 0;
            break;
        }
        if (tags == FS_MAXTAGS || !(len = fs_id3size(b))) break;
        pos += len;
    }
    Seek(fh, old, OFFSET_BEGINNING);
    return r;
}
