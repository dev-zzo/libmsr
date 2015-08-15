/*
 * libmsr: A library to interact with MSRxxx magnetic card readers/writers
 */

#ifndef LIBMSR_H
#define LIBMSR_H

#include <Windows.h>

#define LIBMSRDECL __stdcall

#ifdef _DLL
#define LIBMSRAPI __declspec(dllexport) LIBMSRDECL
#else 
#define LIBMSRAPI __declspec(dllimport) LIBMSRDECL
#endif

typedef long LIBMSRSTATUS;
typedef void* LIBMSRHANDLE;

/*** Status codes returned by all APIs ***/

#define LIBMSR_OK 0L
#define LIBMSR_ERROR 0xC0000000L
#define LIBMSR_MEM_ALLOC_FAILED (LIBMSR_ERROR | 0x00000001)
#define LIBMSR_INVALID_ARGUMENT (LIBMSR_ERROR | 0x00000002)

#define LIBMSR_DEVICE_ERROR (LIBMSR_ERROR | 0x00010000L)
#define LIBMSR_DEVICE_UNEXPECTED_RESPONSE (LIBMSR_DEVICE_ERROR | 0x00000001)
#define LIBMSR_DEVICE_COMMAND_FAILED (LIBMSR_DEVICE_ERROR | 0x00000002)

#define LIBMSR_COMM_PORT_ERROR (LIBMSR_ERROR | 0x00020000L)
#define LIBMSR_PORT_OPEN_FAILED (LIBMSR_COMM_PORT_ERROR | 0x00000002)
#define LIBMSR_PORT_SETUP_FAILED (LIBMSR_COMM_PORT_ERROR | 0x00000003)
#define LIBMSR_PORT_WRITE_FAILED (LIBMSR_COMM_PORT_ERROR | 0x00000004)
#define LIBMSR_PORT_READ_FAILED (LIBMSR_COMM_PORT_ERROR | 0x00000005)

#define LIBMSR_CODEC_ERROR (LIBMSR_ERROR | 0x00040000L)
#define LIBMSR_PARITY_ERROR (LIBMSR_CODEC_ERROR | 0x00000001)

/*** General device API */

/* Open the port and allocate a handle.
 */
LIBMSRSTATUS LIBMSRAPI MSROpen(LPTSTR PortName, LIBMSRHANDLE *pHandle);

/* Close the port; handle is not usable after that.
 */
void LIBMSRAPI MSRClose(LIBMSRHANDLE Handle);

/* Soft-reset the device.
 */
LIBMSRSTATUS LIBMSRAPI MSRReset(LIBMSRHANDLE Handle);

/* LED twiddling */
LIBMSRSTATUS LIBMSRAPI MSRLedAllOff(LIBMSRHANDLE Handle);
LIBMSRSTATUS LIBMSRAPI MSRLedAllOn(LIBMSRHANDLE Handle);
LIBMSRSTATUS LIBMSRAPI MSRLedGreenOn(LIBMSRHANDLE Handle);
LIBMSRSTATUS LIBMSRAPI MSRLedYellowOn(LIBMSRHANDLE Handle);
LIBMSRSTATUS LIBMSRAPI MSRLedRedOn(LIBMSRHANDLE Handle);

/* Verify communications are possible with the device.
 */
LIBMSRSTATUS LIBMSRAPI MSRTestComms(LIBMSRHANDLE Handle);

/* Set whether to use LoCo or HiCo settings when writing a card.
 * This does not affect reading a card in any way.
 */
LIBMSRSTATUS LIBMSRAPI MSRSetCoercivity(LIBMSRHANDLE Handle, BOOL IsHiCo);
LIBMSRSTATUS LIBMSRAPI MSRGetCoercivity(LIBMSRHANDLE Handle, BOOL *pIsHiCo);

/* Set the count of zero bits written before actual data.
 */
LIBMSRSTATUS LIBMSRAPI MSRSetLeadingZeroCount(LIBMSRHANDLE Handle, BYTE Tracks13Count, BYTE Track2Count);
LIBMSRSTATUS LIBMSRAPI MSRGetLeadingZeroCount(LIBMSRHANDLE Handle, BYTE *pTracks13Count, BYTE *pTrack2Count);

/* Set the bit density for a track, in bits per inch (bpi).
 * Currently accepted values are 75 and 210 only.
 * Note that depending on the device, there might be only one accepted setting.
 */
LIBMSRSTATUS LIBMSRAPI MSRSetDensity(LIBMSRHANDLE Handle, UINT Track, UINT BitsPerInch);

/* Set bits per character for data input or output via raw commands.
 * The original documentation is "a bit unclear" on how this affects input/output.
 * So, setting e.g. 8 bits per character means each byte, say, returned by ReadRaw
 * command will contain 8 bits of track data, independent of what BPC was 
 * used originally.
 */
LIBMSRSTATUS LIBMSRAPI MSRSetBitsPerChar(LIBMSRHANDLE Handle, BYTE Track1BPC, BYTE Track2BPC, BYTE Track3BPC);

/*** Card access API ***/

/* Erase a card being swiped.
 */
LIBMSRSTATUS LIBMSRAPI MSRCardErase(LIBMSRHANDLE Handle, BOOL EraseTrack1, BOOL EraseTrack2, BOOL EraseTrack3);

/* Read data from an ISO-compliant card.
 * Outputs track data, fully decoded (7-5-5 BPC, 210-75-210 BPI).
 * NOTE: This may be removed in favor of the raw API.
 */
LIBMSRSTATUS LIBMSRAPI MSRCardReadISO(LIBMSRHANDLE Handle, BYTE *pTrack1Buffer, BYTE *pTrack2Buffer, BYTE *pTrack3Buffer);

/* Read raw data from a card.
 * NOTE: Data is returned LSB first (bit-swapped) and right-aligned within a byte.
 * NOTE: The LRC character AND trailing zero bits are also returned.
 */
LIBMSRSTATUS LIBMSRAPI MSRCardReadRaw(LIBMSRHANDLE Handle, 
    BYTE *pTrack1Buffer, SIZE_T *pTrack1Length,
    BYTE *pTrack2Buffer, SIZE_T *pTrack2Length,
    BYTE *pTrack3Buffer, SIZE_T *pTrack3Length);

/* Write raw data to a card.
 * Bytes must contain characters MSB-first, contrary to ReadRaw command.
 */
LIBMSRSTATUS LIBMSRAPI MSRCardWriteRaw(LIBMSRHANDLE Handle, 
    BYTE *pTrack1Buffer, SIZE_T Track1Length,
    BYTE *pTrack2Buffer, SIZE_T Track2Length,
    BYTE *pTrack3Buffer, SIZE_T Track3Length);

/*** Data conversion API ***/

/* Unpack raw data from the reader.
 * This undoes bit swapping and removes parity bit.
 * Source and Dest should be the same size.
 */
LIBMSRSTATUS LIBMSRAPI MSRUnpackData(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest);

/* Pack data to be sent to the encoder.
 * This just adds the parity bit, at the moment.
 * Source and Dest should be the same size.
 */
LIBMSRSTATUS LIBMSRAPI MSRPackData(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest);

/* Convenience functions -- unpack/decode and encode/pack. */
LIBMSRSTATUS LIBMSRAPI MSRDecodeTrack(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest);
LIBMSRSTATUS LIBMSRAPI MSREncodeTrack(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest);

/* Convert data from ISO/IEC 7811-2/-6 defined encoding to ASCII.
 * Source and Dest should be the same size.
 */
LIBMSRSTATUS LIBMSRAPI ISO7811ToAscii(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest);

/* Convert data from ASCII to ISO/IEC 7811-2/-6 defined encoding.
 * Source and Dest should be the same size.
 */
LIBMSRSTATUS LIBMSRAPI AsciiToISO7811(UINT BitsPerChar, BYTE *Source, SIZE_T SourceLen, BYTE *Dest);

#endif /* LIBMSR_H */
