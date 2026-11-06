#include "vendortest.h"
#include "nfcproximitybusinterface.h"
#include <stdlib.h>
#include "nxp_trace.h"
#include "vendortest.tmh"

NTSTATUS NfcVendorTestRead(DEVICE_CONTEXT* device,UCHAR* buffer,USHORT capacity,WDFREQUEST request)
{
    auto& vendor=device->VendorTestContext;
    (void)WdfWaitLockAcquire(vendor.ReadQueueLock,nullptr);
    auto head=&vendor.ReadNotificationQueue;
    NTSTATUS status=STATUS_PENDING;
    if(head->Flink==head) {
        status=WdfRequestForwardToIoQueue(request,vendor.ReadQueue);
        if(NT_SUCCESS(status)) status=STATUS_PENDING;
    } else {
        auto entry=reinterpret_cast<_VENDOR_TEST_READ_NOTIFICATION_ENTRY*>(head->Flink);
        if(capacity<entry->BufferLength) status=STATUS_INVALID_DEVICE_REQUEST;
        else {
            memcpy(buffer,entry->Buffer,entry->BufferLength);
            head->Flink=entry->ListEntry.Flink;entry->ListEntry.Flink->Blink=head;
            WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,entry->BufferLength);
            free(entry);
        }
    }
    WdfWaitLockRelease(vendor.ReadQueueLock);
    return status;
}

NTSTATUS NfcVendorTestReadNotification(DEVICE_CONTEXT* device,WDFMEMORY memory)
{
    size_t length;
    void* source=WdfMemoryGetBuffer(memory,&length);
    auto& vendor=device->VendorTestContext;
    (void)WdfWaitLockAcquire(vendor.ReadQueueLock,nullptr);
    WDFREQUEST request=nullptr;
    NTSTATUS status=WdfIoQueueRetrieveNextRequest(vendor.ReadQueue,&request);
    if(!NT_SUCCESS(status)) {
        auto entry=static_cast<_VENDOR_TEST_READ_NOTIFICATION_ENTRY*>(malloc(sizeof(_VENDOR_TEST_READ_NOTIFICATION_ENTRY)));
        if(!entry) status=STATUS_INSUFFICIENT_RESOURCES;
        else if(length<=sizeof(entry->Buffer)) {
            entry->ListEntry.Flink=entry->ListEntry.Blink=&entry->ListEntry;
            memcpy(entry->Buffer,source,length);entry->BufferLength=static_cast<USHORT>(length);
            auto head=&vendor.ReadNotificationQueue;
            entry->ListEntry.Flink=head;entry->ListEntry.Blink=head->Blink;
            head->Blink->Flink=&entry->ListEntry;head->Blink=&entry->ListEntry;
        } else {
            // This entry was never queued; retain the original drop/status.
            free(entry);status=STATUS_INVALID_BUFFER_SIZE;
        }
        WdfWaitLockRelease(vendor.ReadQueueLock);
    } else {
        WdfWaitLockRelease(vendor.ReadQueueLock);
        WDFMEMORY output=nullptr;
        status=WdfRequestRetrieveOutputMemory(request,&output);
        if(NT_SUCCESS(status)) {
            size_t capacity=0;void* destination=WdfMemoryGetBuffer(output,&capacity);
            if(capacity<length) status=STATUS_INVALID_BUFFER_SIZE;
            else {
                memcpy(destination,source,length);
                WdfRequestCompleteWithInformation(request,status,length);
            }
        }
        // Successful retrieval transferred ownership from the manual queue.
        // Complete errors outside its lock; do not strand the owned request.
        if(!NT_SUCCESS(status)) WdfRequestCompleteWithInformation(request,status,0);
    }
    return status;
}

NTSTATUS NfcVendorTestSetFirmwareMode(DEVICE_CONTEXT* device,UCHAR mode)
{
    NTSTATUS status=NfcFWSetMode(device,mode);
    if(NT_SUCCESS(status)) {
        auto& vendor=device->VendorTestContext;
        (void)WdfWaitLockAcquire(vendor.ReadQueueLock,nullptr);
        auto head=&vendor.ReadNotificationQueue;
        while(head->Flink!=head) {
            auto entry=head->Flink;head->Flink=entry->Flink;entry->Flink->Blink=head;free(entry);
        }
        WdfWaitLockRelease(vendor.ReadQueueLock);
    }
    return status;
}

NTSTATUS NfcVendorTestWrite(DEVICE_CONTEXT* device,UCHAR* buffer,USHORT length,WDFREQUEST request)
{
    return device->m_pNfcProximityBus->WriteFile(device->FxDevice,buffer,length,NfcVendorTestWriteCompletion,request);
}
void NfcVendorTestWriteCompletion(WDFREQUEST,WDFIOTARGET,WDF_REQUEST_COMPLETION_PARAMS* completion,void* context)
{
    WdfRequestCompleteWithInformation(static_cast<WDFREQUEST>(context),completion->IoStatus.Status,completion->IoStatus.Information);
}
NTSTATUS NfcVendorTestWriteRead(DEVICE_CONTEXT*,UCHAR*,USHORT,UCHAR*,USHORT)
{
    return STATUS_NOT_SUPPORTED;
}
