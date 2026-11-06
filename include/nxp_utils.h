#pragma once
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <wdf.h>

// Signatures recovered from the exact-match x64 PDB, utils.obj.
NTSTATUS NfcUtilsGuidFromUnicodeString(UNICODE_STRING* text, GUID* guid);
NTSTATUS NfcUtilsReadGuidFromBuffer(ULONG length, UCHAR* buffer, GUID* guid);
