/* dtwhich FILE...: the datatype descriptor that claims each file (a small
 * DTInfo). Built for the tests: make dtwhich, then copy to the Amiga. */
#include <stdio.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>

struct Library *DataTypesBase;

int main(int argc, char **argv)
{
    int i;
    if (!(DataTypesBase = OpenLibrary("datatypes.library", 39))) return 20;
    for (i = 1; i < argc; i++) {
        BPTR lock = Lock(argv[i], ACCESS_READ);
        struct DataType *dt;
        if (!lock) {
            printf("%s: cannot lock\n", argv[i]);
            continue;
        }
        if ((dt = ObtainDataTypeA(DTST_FILE, (APTR)lock, NULL))) {
            printf("%s: %s (%s)\n", argv[i], dt->dtn_Header->dth_Name, dt->dtn_Header->dth_BaseName);
            ReleaseDataType(dt);
        } else
            printf("%s: no datatype\n", argv[i]);
        UnLock(lock);
    }
    CloseLibrary(DataTypesBase);
    return 0;
}
