#include "libmsr.h"
#include "internals.h"

LIBMSRSTATUS LIBMSRAPI MSROpen(LPTSTR PortName, LIBMSRHANDLE *pHandle)
{
    LIBMSRSTATUS Status;
    LPMSRCONTEXT Context;
    COMMTIMEOUTS Timeouts;

    Context = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*Context));
    if (!Context) {
        Status = LIBMSR_MEM_ALLOC_FAILED;
        goto fail0;
    }

    Context->PortHandle = CreateFile(
        PortName,
        GENERIC_READ | GENERIC_WRITE,
		0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (Context->PortHandle == INVALID_HANDLE_VALUE) {
        Status = LIBMSR_PORT_OPEN_FAILED;
        goto fail1;
    }

    Context->PortSettings.DCBlength = sizeof(DCB);
    if (!GetCommState(Context->PortHandle, &Context->PortSettings)) {
        Status = LIBMSR_PORT_SETUP_FAILED;
        goto fail1;
    }

    /* NOTE: All MSRxxx devices seem to use 9600, 8N1*/
    Context->PortSettings.BaudRate = CBR_9600;
    Context->PortSettings.fParity = FALSE;
    Context->PortSettings.ByteSize = 8;
    Context->PortSettings.Parity = NOPARITY;
    Context->PortSettings.StopBits = ONESTOPBIT;
    if (!SetCommState(Context->PortHandle, &Context->PortSettings)) {
        Status = LIBMSR_PORT_SETUP_FAILED;
        goto fail1;
    }

    if (!SetupComm(Context->PortHandle, 1024, 1024)) {
        Status = LIBMSR_PORT_SETUP_FAILED;
        goto fail1;
    }

    /* TODO: Set timeouts */

    *pHandle = (LIBMSRHANDLE)Context;
    return LIBMSR_OK;

fail1:
    HeapFree(GetProcessHeap(), 0, Context);

fail0:
    return Status;
}

void LIBMSRAPI MSRClose(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;

    CloseHandle(Context->PortHandle);

    HeapFree(GetProcessHeap(), 0, Context);
}


static LIBMSRSTATUS LIBMSRDECL _MSRSend(LPMSRCONTEXT Context, LPBYTE Buffer, SIZE_T Count)
{
    DWORD BytesWritten;

    while (Count > 0) {
        if (!WriteFile(Context->PortHandle, Buffer, Count, &BytesWritten, NULL)) {
            return LIBMSR_PORT_WRITE_FAILED;
        }
        if (BytesWritten == 0) {
            return LIBMSR_PORT_WRITE_FAILED;
        }
        Count -= BytesWritten;
        Buffer += BytesWritten;
    }
    return LIBMSR_OK;
}

static LIBMSRSTATUS LIBMSRDECL _MSRRecv(LPMSRCONTEXT Context, LPBYTE Buffer, SIZE_T Count)
{
    DWORD BytesRead;

    while (Count > 0) {
        if (!ReadFile(Context->PortHandle, Buffer, Count, &BytesRead, NULL)) {
            return LIBMSR_PORT_READ_FAILED;
        }
        if (BytesRead == 0) {
            return LIBMSR_PORT_READ_FAILED;
        }
        Count -= BytesRead;
        Buffer += BytesRead;
    }
    return LIBMSR_OK;
}

static int LIBMSRDECL _MSRRecvChar(LPMSRCONTEXT Context)
{
    BYTE Buffer;
    DWORD BytesRead;

    if (!ReadFile(Context->PortHandle, &Buffer, 1, &BytesRead, NULL)) {
        return -1;
    }
    return Buffer;
}

LIBMSRSTATUS LIBMSRAPI MSRReset(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x61;
    /* No answer expected */
    return _MSRSend(Context, CommandBuffer, 2);
}

LIBMSRSTATUS LIBMSRAPI MSRLedAllOff(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x81;
    /* No answer expected */
    return _MSRSend(Context, CommandBuffer, 2);
}

LIBMSRSTATUS LIBMSRAPI MSRLedAllOn(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x82;
    /* No answer expected */
    return _MSRSend(Context, CommandBuffer, 2);
}

LIBMSRSTATUS LIBMSRAPI MSRLedGreenOn(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x82;
    /* No answer expected */
    return _MSRSend(Context, CommandBuffer, 2);
}

LIBMSRSTATUS LIBMSRAPI MSRLedYellowOn(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x82;
    /* No answer expected */
    return _MSRSend(Context, CommandBuffer, 2);
}

LIBMSRSTATUS LIBMSRAPI MSRLedRedOn(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x82;
    /* No answer expected */
    return _MSRSend(Context, CommandBuffer, 2);
}

static LIBMSRSTATUS LIBMSRDECL _MSRDoSendRecvWithCheck(LPMSRCONTEXT Context, BYTE CommandBuffer[], UINT CommandLength)
{
    LIBMSRSTATUS Status;
    int Esc;

    PurgeComm(Context->PortHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    Status = _MSRSend(Context, CommandBuffer, CommandLength);
    if (Status < 0) {
        return Status;
    }
    Esc = _MSRRecvChar(Context);
    if (Esc != ESC) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }
    return LIBMSR_OK;
}

/* For commands that return 1B 30 or 1B 41 */
static LIBMSRSTATUS LIBMSRDECL _MSRDoCommandNoData(LPMSRCONTEXT Context, BYTE CommandBuffer[], UINT CommandLength)
{
    LIBMSRSTATUS Status;
    int ReportedStatus;

    Status = _MSRDoSendRecvWithCheck(Context, CommandBuffer, CommandLength);
    if (Status < 0) {
        return Status;
    }
    ReportedStatus = _MSRRecvChar(Context);
    if (ReportedStatus != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRTestComms(LIBMSRHANDLE Handle)
{
    BYTE CommandBuffer[2];
    LIBMSRSTATUS Status;
    int ReportedStatus;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x65;
    Status = _MSRDoSendRecvWithCheck((LPMSRCONTEXT)Handle, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    ReportedStatus = _MSRRecvChar((LPMSRCONTEXT)Handle);
    if (ReportedStatus != 0x79) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRCardErase(LIBMSRHANDLE Handle, BOOL EraseTrack1, BOOL EraseTrack2, BOOL EraseTrack3)
{
    BYTE TrackMask;
    BYTE CommandBuffer[3];

    TrackMask = !!EraseTrack1 | (!!EraseTrack2 << 1) | (!!EraseTrack3 << 2);
    if (!TrackMask) {
        return LIBMSR_INVALID_ARGUMENT;
    }

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x63;
    CommandBuffer[2] = TrackMask;
    return _MSRDoCommandNoData((LPMSRCONTEXT)Handle, CommandBuffer, 3);
}

LIBMSRSTATUS LIBMSRAPI MSRSetCoercivity(LIBMSRHANDLE Handle, BOOL IsHiCo)
{
    BYTE CommandBuffer[2];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = IsHiCo ? 0x78 : 0x79;
    return _MSRDoCommandNoData((LPMSRCONTEXT)Handle, CommandBuffer, 2);
}

LIBMSRSTATUS LIBMSRAPI MSRGetCoercivity(LIBMSRHANDLE Handle, BOOL *pIsHiCo)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    LIBMSRSTATUS Status;
    int Value;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x64;
    Status = _MSRDoSendRecvWithCheck(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    Value = _MSRRecvChar(Context);
    if (Value == 'H') {
        *pIsHiCo = TRUE;
    }
    else if (Value == 'L') {
        *pIsHiCo = FALSE;
    }
    else {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRSetLeadingZeroCount(LIBMSRHANDLE Handle, BYTE Tracks13Count, BYTE Track2Count)
{
    BYTE CommandBuffer[4];

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x7A;
    CommandBuffer[2] = Tracks13Count;
    CommandBuffer[3] = Track2Count;
    return _MSRDoCommandNoData((LPMSRCONTEXT)Handle, CommandBuffer, 4);
}

LIBMSRSTATUS LIBMSRAPI MSRGetLeadingZeroCount(LIBMSRHANDLE Handle, BYTE *pTracks13Count, BYTE *pTrack2Count)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x6C;
    Status = _MSRDoSendRecvWithCheck(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    *pTracks13Count = _MSRRecvChar(Context);
    *pTrack2Count = _MSRRecvChar(Context);
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRSetDensity(LIBMSRHANDLE Handle, UINT Track, UINT BitsPerInch)
{
    BYTE CommandBuffer[3];
    static const BYTE BpiSetting[] = { 0xA0, 0xA1, 0x4B, 0xD2, 0xC0, 0xC1 };

    if (Track > 3) {
        return LIBMSR_INVALID_ARGUMENT;
    }
    if (BitsPerInch != 210 && BitsPerInch != 75) {
        return LIBMSR_INVALID_ARGUMENT;
    }

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x62;
    CommandBuffer[2] = BpiSetting[((Track - 1) << 1) + (BitsPerInch == 210)];
    return _MSRDoCommandNoData((LPMSRCONTEXT)Handle, CommandBuffer, 3);
}

LIBMSRSTATUS LIBMSRAPI MSRSetBitsPerChar(LIBMSRHANDLE Handle, BYTE Track1BPC, BYTE Track2BPC, BYTE Track3BPC)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[5];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x6F;
    CommandBuffer[2] = Track1BPC;
    CommandBuffer[3] = Track2BPC;
    CommandBuffer[4] = Track3BPC;
    Status = _MSRDoSendRecvWithCheck(Context, CommandBuffer, 5);
    _MSRRecvChar(Context); // 30
    _MSRRecvChar(Context); // b1
    _MSRRecvChar(Context); // b2
    _MSRRecvChar(Context); // b3
    return Status;
}

LIBMSRSTATUS LIBMSRAPI MSRCardReadISO(LIBMSRHANDLE Handle, BYTE *pTrack1Buffer, BYTE *pTrack2Buffer, BYTE *pTrack3Buffer)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    LIBMSRSTATUS Status;
    BYTE CommandBuffer[2];
    BYTE *Ptr;
    BYTE ch;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x72;
    Status = _MSRDoSendRecvWithCheck(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (_MSRRecvChar(Context) != 0x73) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }

    for (;;) {
        ch = _MSRRecvChar(Context);
        if (ch != ESC) {
            break;
        }
        switch (_MSRRecvChar(Context)) {
        case 1:
            Ptr = pTrack1Buffer;
            break;
        case 2:
            Ptr = pTrack2Buffer;
            break;
        case 3:
            Ptr = pTrack3Buffer;
            break;
        default:
            Ptr = NULL;
            break;
        }

        do {
            ch = _MSRRecvChar(Context);
            /* MSR605: If no data on the track, 1B 2B is sent as content */
            if (ch == ESC) {
                ch = _MSRRecvChar(Context);
                break;
            }
            *Ptr++ = ch;
        } while (ch != 0x3F);
        *Ptr = 0x00;
    }

    if (ch != 0x3F) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }
    if (_MSRRecvChar(Context) != 0x1C) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }

    if (_MSRRecvChar(Context) != ESC) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }
    ch = _MSRRecvChar(Context);
    if (ch != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRCardReadRaw(LIBMSRHANDLE Handle, 
    BYTE *pTrack1Buffer, SIZE_T *pTrack1Length,
    BYTE *pTrack2Buffer, SIZE_T *pTrack2Length,
    BYTE *pTrack3Buffer, SIZE_T *pTrack3Length)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    LIBMSRSTATUS Status;
    BYTE CommandBuffer[2];
    BYTE ch;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x6D;
    Status = _MSRDoSendRecvWithCheck(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (_MSRRecvChar(Context) != 0x73) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }

    for (;;) {
        BYTE *TrackBuffer;
        SIZE_T TrackLength;
        SIZE_T *pTrackLength;

        ch = _MSRRecvChar(Context);
        if (ch != ESC) {
            //for(;;) { ch = _MSRRecvChar(Context); }
            break;
        }
        switch (_MSRRecvChar(Context)) {
        case 1:
            TrackBuffer = pTrack1Buffer;
            pTrackLength = pTrack1Length;
            break;
        case 2:
            TrackBuffer = pTrack2Buffer;
            pTrackLength = pTrack2Length;
            break;
        case 3:
            TrackBuffer = pTrack3Buffer;
            pTrackLength = pTrack3Length;
            break;
        default:
            TrackBuffer = NULL;
            break;
        }

        TrackLength = _MSRRecvChar(Context);
        Status = _MSRRecv(Context, TrackBuffer, TrackLength);
        if (Status < 0) {
            return Status;
        }
        *pTrackLength = TrackLength;
    }

    if (ch != 0x3F) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }
    if (_MSRRecvChar(Context) != 0x1C) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }

    if (_MSRRecvChar(Context) != ESC) {
        return LIBMSR_DEVICE_UNEXPECTED_RESPONSE;
    }
    ch = _MSRRecvChar(Context);
    if (ch != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

static LIBMSRSTATUS LIBMSRDECL _MSRCardXmitTrack(LPMSRCONTEXT Context, BYTE TrackId, BYTE *Buffer, SIZE_T Length)
{
    BYTE CommandBuffer[3];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = TrackId;
    CommandBuffer[2] = (BYTE)Length;
    Status = _MSRSend(Context, CommandBuffer, 3);
    if (Status < 0) {
        return Status;
    }
    if (Length > 0) {
        Status = _MSRSend(Context, Buffer, Length);
    }
    return Status;
}

LIBMSRSTATUS LIBMSRAPI MSRCardWriteRaw(LIBMSRHANDLE Handle, 
    BYTE *pTrack1Buffer, SIZE_T Track1Length,
    BYTE *pTrack2Buffer, SIZE_T Track2Length,
    BYTE *pTrack3Buffer, SIZE_T Track3Length)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[3];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x6E;
    Status = _MSRSend(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }

    CommandBuffer[1] = 0x73;
    Status = _MSRSend(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }

    Status = _MSRCardXmitTrack(Context, 1, pTrack1Buffer, Track1Length);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRCardXmitTrack(Context, 2, pTrack2Buffer, Track2Length);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRCardXmitTrack(Context, 3, pTrack3Buffer, Track3Length);
    if (Status < 0) {
        return Status;
    }

    CommandBuffer[0] = 0x3F;
    CommandBuffer[1] = 0x1C;
    return _MSRDoCommandNoData(Context, CommandBuffer, 2);
}

static const BYTE BitReverseTable[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF,
};

BYTE ComputeParity(BYTE x)
{
    int PopCount;

    PopCount = 0;
    while (x) {
        PopCount += x & 1;
        x >>= 1;
    }
    return (PopCount & 1) == 0;
}

LIBMSRSTATUS LIBMSRAPI MSRUnpackData(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest)
{
    BYTE ParityMask = 1 << (BitsPerChar - 1);

    while (SourceLen-- > 0) {
        BYTE ch;
        BYTE Parity;

        ch = BitReverseTable[*Source++] >> (8 - BitsPerChar);
        Parity = !!(ch & ParityMask);
        ch &= ~ParityMask;
        *Dest++ = ch;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRPackData(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest)
{
    while (SourceLen-- > 0) {
        BYTE ch;

        ch = *Source++;
        ch |= ComputeParity(ch) << (BitsPerChar - 1);
        *Dest++ = ch;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRDecodeTrack(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest)
{
    MSRUnpackData(BitsPerChar, Source, SourceLen, Dest);
    ISO7811ToAscii(BitsPerChar, Dest, SourceLen, Dest);
    Dest[SourceLen] = 0x00;
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSREncodeTrack(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest)
{
    AsciiToISO7811(BitsPerChar, Source, SourceLen, Dest);
    MSRPackData(BitsPerChar, Dest, SourceLen, Dest);
    return LIBMSR_OK;
}
