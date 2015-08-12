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

    PurgeComm(Context->PortHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
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
    _MSRRecvChar(Context);
    _MSRRecvChar(Context);
    _MSRRecvChar(Context);
    return Status;
}

LIBMSRSTATUS LIBMSRAPI MSRCardReadISO(LIBMSRHANDLE Handle, BYTE *pTrack1Buffer, BYTE *pTrack2Buffer, BYTE *pTrack3Buffer)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    LIBMSRSTATUS Status;
    BYTE *Ptr;
    BYTE ch;
    BYTE Buffer[2048];

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
            //for(;;) { ch = _MSRRecvChar(Context); }
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

