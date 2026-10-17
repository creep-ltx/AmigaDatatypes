/* claimcheck: would ansi.datatype's recognition code take these files?
 * Prints "1 path" or "0 path" for each path given on stdin, as hook.c
 * decides it (the record from the last 128 bytes, the first 8 KB).
 *   cc -O2 -o claimcheck claimcheck.c ../src/guess.c */
#include <stdio.h>
#include <string.h>
#include "../src/guess.h"

int main(void)
{
    static unsigned char buf[8192], tail[128];
    char path[4096];
    while (fgets(path, sizeof(path), stdin)) {
        FILE *f;
        long size, n;
        const char *name;
        int hastail = 0;
        path[strcspn(path, "\n")] = 0;
        if (!(f = fopen(path, "rb"))) continue;
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        if (size >= 128) {
            fseek(f, size - 128, SEEK_SET);
            hastail = fread(tail, 1, 128, f) == 128;
        }
        fseek(f, 0, SEEK_SET);
        n = (long)fread(buf, 1, sizeof(buf), f);
        fclose(f);
        name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
        printf("%d %s\n", n > 0 && ansi_claim(name, hastail ? tail : NULL, buf, n), path);
    }
    return 0;
}
