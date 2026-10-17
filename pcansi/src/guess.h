/* guess.c: above 0 (or 0) the bytes look like Amiga text art, below 0 PC */
#ifndef GUESS_H
#define GUESS_H
long ansi_guess(const unsigned char *b, long n);

/* is it PC text art? name: the file's name; tail: its last 128 bytes (NULL
 * if shorter or unknown); b, n: its first bytes */
int ansi_claim(const char *name, const unsigned char *tail, const unsigned char *b, long n);
#endif
