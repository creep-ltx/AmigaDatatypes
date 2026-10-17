/* sndtest: open sound files through datatypes on the Amiga, as MultiView
 * does, and say what came out: the class, length, rate, mono or stereo,
 * the time NewDTObject took, and memory before and after (three rounds:
 * the first one also loads the classes, the others must give it all back).
 *   sndtest FILE [DUMP] [PLAY]
 * DUMP: the 8-bit samples into that file (left/mono, then right) for
 * tests/expect.c to compare; PLAY: plays it and waits until it ends. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>

struct Library *DataTypesBase;

static ULONG ticks(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return (ULONG)d.ds_Minute * 3000 + d.ds_Tick;
}

int main(int argc, char **argv)
{
    int round, play = argc > 3;
    if (argc < 2) { printf("usage: sndtest FILE [DUMP] [PLAY]\n"); return 10; }
    if (!(DataTypesBase = OpenLibrary("datatypes.library", 39))) return 20;
    for (round = 0; round < 3; round++) {
        ULONG before = AvailMem(MEMF_ANY), t = ticks(), len = 0, rate = 0, period = 0, vol = 0;
        BYTE *mono = NULL, *left = NULL, *right = NULL;
        struct DataType *dt = NULL;
        Object *o = NewDTObject(argv[1], DTA_GroupID, GID_SOUND, TAG_DONE);
        t = ticks() - t;
        if (!o) {
            printf("%s: no object, error %ld\n", argv[1], (long)IoErr());
            break;
        }
        GetDTAttrs(o, DTA_DataType, (ULONG)&dt, SDTA_Sample, (ULONG)&mono, SDTA_SampleLength, (ULONG)&len,
                   SDTA_Period, (ULONG)&period, SDTA_Volume, (ULONG)&vol, TAG_DONE);
        /* V44 tags: sound.datatype 40 leaves them alone */
        GetDTAttrs(o, SDTA_LeftSample, (ULONG)&left, SDTA_RightSample, (ULONG)&right,
                   SDTA_SamplesPerSec, (ULONG)&rate, TAG_DONE);
        if (round == 0) {
            printf("%s: %s, %lu samples, period %lu, %lu Hz, volume %lu, %s, %lu.%02lu s to open\n", argv[1],
                   dt ? (char *)dt->dtn_Header->dth_Name : "?", (unsigned long)len, (unsigned long)period,
                   (unsigned long)rate, (unsigned long)vol, left && right ? "stereo" : "mono",
                   (unsigned long)(t / 50), (unsigned long)(t % 50 * 2));
            if (argc > 2 && strcmp(argv[2], "-")) {
                BPTR fh = Open(argv[2], MODE_NEWFILE);
                if (fh) {
                    if (left && right) { Write(fh, left, len); Write(fh, right, len); }
                    else Write(fh, mono ? mono : left, len);
                    Close(fh);
                }
            }
            if (play) {
                ULONG secs = rate ? len / rate + 1 : len * period / 3546895 + 1;
                DoDTMethod(o, NULL, NULL, DTM_TRIGGER, NULL, STM_PLAY, NULL);
                Delay(secs * 50);
            }
        }
        DisposeDTObject(o);
        printf("round %d: %ld bytes not given back\n", round + 1, (long)(before - AvailMem(MEMF_ANY)));
    }
    CloseLibrary(DataTypesBase);
    return 0;
}
