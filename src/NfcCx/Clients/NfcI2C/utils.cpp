#include "nxp_utils.h"
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <stdio.h>
#include <string.h>
#include "nxp_trace.h"
#include "utils.tmh"

// RVA 0x129c8. The CRT receives DWORD-sized %x destinations, then the binary
// narrows the results to GUID fields. Data1 is scanned directly into the output.
NTSTATUS NfcUtilsGuidFromUnicodeString(UNICODE_STRING* text, GUID* guid)
{
    ULONG fields[10] = {};
    WCHAR terminated[39] = {};
    NTSTATUS status = STATUS_SUCCESS;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    *guid = {};
    if (text->Length > 76) {
        TRACE_LINE(LEVEL_ERROR, "String too large");
        status = STATUS_INVALID_PARAMETER;
    } else {
        memcpy(terminated, text->Buffer, text->Length);
        const int converted = swscanf_s(terminated,
            L"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
            &guid->Data1, &fields[0], &fields[1], &fields[2], &fields[3],
            &fields[4], &fields[5], &fields[6], &fields[7], &fields[8], &fields[9]);
        if (converted != 11) {
            TRACE_LINE(LEVEL_ERROR, "Failed to convert the string to a GUID");
            status = STATUS_INVALID_PARAMETER;
        } else {
            guid->Data2 = static_cast<USHORT>(fields[0]);
            guid->Data3 = static_cast<USHORT>(fields[1]);
            for (unsigned i=0; i<8; ++i) guid->Data4[i]=static_cast<UCHAR>(fields[i+2]);
        }
    }
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE, status);
    return status;
}

// RVA 0x12bd0. Vendor blob byte order reverses the GUID's eight tail bytes.
// memcpy avoids unaligned typed loads on the ARM64 target while preserving x64
// little-endian field interpretation. Short input leaves the output unchanged.
NTSTATUS NfcUtilsReadGuidFromBuffer(ULONG length, UCHAR* buffer, GUID* guid)
{
    if (length < sizeof(GUID)) return STATUS_INVALID_PARAMETER;
    memcpy(&guid->Data1, buffer+12, sizeof(guid->Data1));
    memcpy(&guid->Data2, buffer+10, sizeof(guid->Data2));
    memcpy(&guid->Data3, buffer+8, sizeof(guid->Data3));
    for (unsigned i=0; i<8; ++i) guid->Data4[i]=buffer[7-i];
    return STATUS_SUCCESS;
}
