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


static LIBMSRSTATUS _MSRSend(LPMSRCONTEXT Context, LPBYTE Buffer, SIZE_T Count)
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

static LIBMSRSTATUS _MSRRecv(LPMSRCONTEXT Context, LPBYTE Buffer, SIZE_T Count)
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

LIBMSRSTATUS LIBMSRAPI MSRTestComms(LIBMSRHANDLE Handle)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    BYTE ResponseBuffer[2];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x65;
    Status = _MSRSend(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRRecv(Context, ResponseBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (ResponseBuffer[0] != ESC) {
        return LIBMSR_DEVICE_SYNC_LOST;
    }
    if (ResponseBuffer[1] != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRCardErase(LIBMSRHANDLE Handle, BOOL EraseTrack1, BOOL EraseTrack2, BOOL EraseTrack3)
{
    BYTE TrackMask;
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[3];
    BYTE ResponseBuffer[2];
    LIBMSRSTATUS Status;

    TrackMask = !!EraseTrack1 | (!!EraseTrack2 << 1) | (!!EraseTrack3 << 2);
    if (!TrackMask) {
        return LIBMSR_OK;
    }

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x63;
    CommandBuffer[2] = TrackMask;
    Status = _MSRSend(Context, CommandBuffer, 3);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRRecv(Context, ResponseBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (ResponseBuffer[0] != ESC) {
        return LIBMSR_DEVICE_SYNC_LOST;
    }
    if (ResponseBuffer[1] != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRSetCoercivity(LIBMSRHANDLE Handle, BOOL IsHiCo)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    BYTE ResponseBuffer[2];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = IsHiCo ? 0x78 : 0x79;
    Status = _MSRSend(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRRecv(Context, ResponseBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (ResponseBuffer[0] != ESC) {
        return LIBMSR_DEVICE_SYNC_LOST;
    }
    if (ResponseBuffer[1] != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRGetCoercivity(LIBMSRHANDLE Handle, BOOL *pIsHiCo)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    BYTE ResponseBuffer[2];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x64;
    Status = _MSRSend(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRRecv(Context, ResponseBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (ResponseBuffer[0] != ESC) {
        return LIBMSR_DEVICE_SYNC_LOST;
    }
    if (ResponseBuffer[1] == 'H') {
        *pIsHiCo = TRUE;
    }
    else if (ResponseBuffer[1] == 'L') {
        *pIsHiCo = FALSE;
    }
    else {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRSetLeadingZeroCount(LIBMSRHANDLE Handle, BYTE Tracks13Count, BYTE Track2Count)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[4];
    BYTE ResponseBuffer[2];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x7A;
    CommandBuffer[1] = Tracks13Count;
    CommandBuffer[1] = Track2Count;
    Status = _MSRSend(Context, CommandBuffer, 4);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRRecv(Context, ResponseBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    if (ResponseBuffer[0] != ESC) {
        return LIBMSR_DEVICE_SYNC_LOST;
    }
    if (ResponseBuffer[1] != 0x30) {
        return LIBMSR_DEVICE_COMMAND_FAILED;
    }
    return LIBMSR_OK;
}

LIBMSRSTATUS LIBMSRAPI MSRGetLeadingZeroCount(LIBMSRHANDLE Handle, BYTE *pTracks13Count, BYTE *pTrack2Count)
{
    LPMSRCONTEXT Context = (LPMSRCONTEXT)Handle;
    BYTE CommandBuffer[2];
    BYTE ResponseBuffer[3];
    LIBMSRSTATUS Status;

    CommandBuffer[0] = ESC;
    CommandBuffer[1] = 0x6C;
    Status = _MSRSend(Context, CommandBuffer, 2);
    if (Status < 0) {
        return Status;
    }
    Status = _MSRRecv(Context, ResponseBuffer, 3);
    if (Status < 0) {
        return Status;
    }
    if (ResponseBuffer[0] != ESC) {
        return LIBMSR_DEVICE_SYNC_LOST;
    }
    *pTracks13Count = ResponseBuffer[1];
    *pTrack2Count = ResponseBuffer[2];
    return LIBMSR_OK;
}

