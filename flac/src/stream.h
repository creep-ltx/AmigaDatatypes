/* stream: a FLAC file from start to end through a small window, the same
 * on the Amiga (dos.library) and in the tests on the PC (stdio): the
 * metadata, then every frame into the 8-bit sound. A damaged frame is
 * skipped (silence) and the next one found. */
#ifndef STREAM_H
#define STREAM_H

#include "flacdec.h"
#include "conv.h"

typedef struct {
    LONG (*read)(void *h, UBYTE *buf, LONG len);    /* bytes read, < len at the end */
    int (*skip)(void *h, LONG len);                 /* 1 if done */
    void *h;
    UBYTE *buf;                         /* room for a VORBIS_COMMENT block (or NULL) */
    ULONG bufsize;
    char title[80], artist[80];
} FlStream;

/* "fLaC" (after any ID3v2 tags, skipped) and the metadata blocks, up to
 * the first frame: 1 if it is a FLAC stream fl_streaminfo takes and that
 * says its length */
int fs_meta(FlStream *s, FlacDec *d);

/* the window's size: a whole frame at the most, and 4 KB to read ahead */
#define FS_WINDOW(d) (fl_framemax(d) + 4096)

/* the frames into c (cv_init done, c->out allocated). WIN is the caller's,
 * FS_WINDOW(d) bytes, with 16 more after them (fl_frame may read there). The frames that were whole and right
 * go into *good, damaged stretches skipped into *bad. */
void fs_frames(FlStream *s, FlacDec *d, Conv *c, UBYTE *win, ULONG *good, ULONG *bad);

#endif
