#include "libmsr.h"
#include "internals.h"

LIBMSRSTATUS LIBMSRAPI ISO7811ToAscii(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest)
{
    BYTE CharMask = (1 << (BitsPerChar - 1)) - 1;
    while (SourceLen-- > 0) {
        BYTE ch;

        ch = (*Source++) & CharMask;
        switch (BitsPerChar) {
        case 5:
            ch = 0x30 + ch;
            break;
        case 7:
            ch = 0x20 + ch;
            break;
        default:
            return LIBMSR_INVALID_ARGUMENT;
        }
        *Dest++ = ch;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI AsciiToISO7811(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest)
{
    while (SourceLen-- > 0) {
        BYTE ch;

        ch = *Source++;
        switch (BitsPerChar) {
        case 5:
            ch = ch - 0x30;
            break;
        case 7:
            ch = ch - 0x20;
            break;
        default:
            return LIBMSR_INVALID_ARGUMENT;
        }
        *Dest++ = ch;
    }
    return LIBMSR_OK;
}
