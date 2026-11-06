#pragma once
#include "device.h"
#include <phNfcStatus.h>
struct _TML_READ_NOTIFICATION_ENTRY {
    LIST_ENTRY ListEntry;
    // Own a complete NCI or download frame, including its header.
    // Intentional private layout change: Buffer[515], length offset532;
    // the allocation is536 bytes on x64 and ARM64 (checked by the ABI probes).
    UCHAR Buffer[515];
    USHORT BufferLength;
};
void NfcTmlDeferredReadCompletion(DEVICE_CONTEXT*,UINT_PTR,UINT_PTR,UINT_PTR);
void NfcTmlDeferredWriteCompletion(DEVICE_CONTEXT*,UINT_PTR,UINT_PTR,UINT_PTR);
NTSTATUS NfcTmlReadNotification(DEVICE_CONTEXT*,WDFMEMORY);
EVT_WDF_REQUEST_COMPLETION_ROUTINE NfcTmlWriteCompletion;
