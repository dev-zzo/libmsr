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
    SIZE_T Track1Len, Track2Len, Track3Len;
    BYTE Track1Text[512];
    BYTE Track2Text[512];
    BYTE Track3Text[512];

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

#if 0
    Status = MSRCardReadISO(Handle, Track1, Track2, Track3);
    printf("Track1: '%s'\n", Track1);
    printf("Track2: '%s'\n", Track2);
    printf("Track3: '%s'\n", Track3);
#endif

    Status = MSRSetBitsPerChar(Handle, 7, 5, 5);
    if (Status < 0) {
        printf("Failed with status %08X\n", Status);
        return 1;
    }
    Status = MSRCardReadRaw(Handle, Track1, &Track1Len, Track2, &Track2Len, Track3, &Track3Len);
    if (Status < 0) {
        printf("Failed with status %08X\n", Status);
        return 1;
    }
    MSRDecodeTrack(7, Track1, Track1Len, Track1Text);
    MSRDecodeTrack(5, Track2, Track2Len, Track2Text);
    MSRDecodeTrack(5, Track3, Track3Len, Track3Text);
    printf("Track1: '%s'\n", Track1Text);
    printf("Track2: '%s'\n", Track2Text);
    printf("Track3: '%s'\n", Track3Text);

    MSRClose(Handle);
    return 0;
}
