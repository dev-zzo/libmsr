#include "libmsr.h"
#include <stdio.h>
#include <tchar.h>

int _tmain(int argc, _TCHAR *argv[])
{
    LIBMSRHANDLE Handle;
    LIBMSRSTATUS Status;
    BYTE Track1[512];
    BYTE Track2[512];
    BYTE Track3[512];

    Status = MSROpen(argv[1], &Handle);
    if (Status < 0) {
        printf("Failed with status %08X\n", Status);
        return 1;
    }

    Status = MSRReset(Handle);
    if (Status < 0) {
        printf("Failed with status %08X\n", Status);
        return 1;
    }
    Status = MSRTestComms(Handle);
    if (Status < 0) {
        printf("Failed with status %08X\n", Status);
        return 1;
    }
    Status = MSRReset(Handle);
    if (Status < 0) {
        printf("Failed with status %08X\n", Status);
        return 1;
    }

    Status = MSRCardReadISO(Handle, Track1, Track2, Track3);

    MSRClose(Handle);
    return 0;
}
