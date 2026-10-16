/* shim.c: the few things the class needs beside md.c */
#ifndef SHIM_H
#define SHIM_H

#include <exec/types.h>

int isutf8(const unsigned char *t, long n);  /* valid UTF-8 beyond ASCII */
UBYTE latin1(ULONG c);                       /* a code point as the nearest Latin-1 character */

/* no <string.h> (libnix's turns memmove into __bcopz): shim.c has these */
void *memset(void *d, int c, unsigned long n);

#endif
