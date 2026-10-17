/* the tests' file readers for stream.c: a file in memory */
#include <string.h>
#include "stream.h"

typedef struct { const UBYTE *b; long size, pos; } Mem;

static LONG mread(void *h, UBYTE *buf, LONG len)
{
    Mem *m = h;
    if (len > m->size - m->pos) len = m->size - m->pos;
    memcpy(buf, m->b + m->pos, len);
    m->pos += len;
    return len;
}

static int mskip(void *h, LONG len)
{
    Mem *m = h;
    if (len > m->size - m->pos) return 0;
    m->pos += len;
    return 1;
}
