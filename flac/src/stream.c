/* stream: the file through a window. See stream.h. */
#include "stream.h"

int fs_meta(FlStream *s, FlacDec *d)
{
    UBYTE b[FL_INFO];
    ULONG type, len;
    int last = 0, info = 0;
    s->title[0] = s->artist[0] = 0;
    if (s->read(s->h, b, 4) != 4 || b[0] != 'f' || b[1] != 'L' || b[2] != 'a' || b[3] != 'C') return 0;
    while (!last) {
        if (s->read(s->h, b, 4) != 4) return 0;
        type = fl_blockheader(b, &len, &last);
        if (!info) {                    /* STREAMINFO comes first */
            if (type != 0 || len < FL_INFO || s->read(s->h, b, FL_INFO) != FL_INFO || !fl_streaminfo(d, b))
                return 0;
            info = 1;
            len -= FL_INFO;
        } else if (type == 4 && s->buf && len <= s->bufsize) {     /* VORBIS_COMMENT: title, artist */
            if (s->read(s->h, s->buf, len) != (LONG)len) return 0;
            fl_comment(s->buf, len, s->title, s->artist, sizeof(s->title));
            len = 0;
        }
        if (len && !s->skip(s->h, len)) return 0;
    }
    return d->samples != 0;
}

void fs_frames(FlStream *s, FlacDec *d, Conv *c, UBYTE *win, ULONG *good, ULONG *bad)
{
    ULONG off = 0, have = 0, frame = fl_framemax(d), winsize = frame + 4096, used, i;
    int eof = 0;
    *good = *bad = 0;
    for (;;) {
        if (have < frame && !eof) {      /* at least a whole frame in the window */
            LONG got;
            for (i = 0; i < have; i++) win[i] = win[off + i];
            off = 0;
            got = s->read(s->h, win + have, winsize - have);
            if (got < (LONG)(winsize - have)) eof = 1;
            if (got > 0) have += got;
        }
        if (!have) break;
        if ((used = fl_frame(d, win + off, have))) {
            (*good)++;
            cv_block(c, d);
            if (c->pos >= c->len) break;
        } else {                        /* not a frame here: the next one */
            used = fl_sync(d, win + off, have);
            if (used == have && !eof && have > 16) used = have - 16;   /* one may start in the last bytes */
            (*bad)++;
        }
        off += used;
        have -= used;
    }
}
