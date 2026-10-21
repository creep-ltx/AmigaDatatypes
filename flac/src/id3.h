/* id3: the size of an ID3v2 tag in front of a FLAC stream, for stream.c
 * (which skips it) and hook.c (the recognition code, which looks past it).
 * Plain C, no library. */
#ifndef ID3_H
#define ID3_H

#include "flacdec.h"

#define FS_MAXTAGS 4                    /* ID3v2 tags in a row, at most */

/* the tag's 10-byte header at b: the whole tag's size in bytes (header and
 * footer included), 0 if it is not an ID3v2 tag */
static ULONG fs_id3size(const UBYTE *b)
{
    if (b[0] != 'I' || b[1] != 'D' || b[2] != '3' || b[3] == 0xff || b[4] == 0xff ||
        ((b[6] | b[7] | b[8] | b[9]) & 0x80))
        return 0;
    return 10 + ((ULONG)b[6] << 21 | (ULONG)b[7] << 14 | (ULONG)b[8] << 7 | b[9]) + (b[5] & 0x10 ? 10 : 0);
}

#endif
