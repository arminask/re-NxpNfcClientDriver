#include "tml.h"
#include "nfcproximitybusinterface.h"
#include <stdlib.h>
extern "C" {
#include <phOsalNfc.h>
}
#include "nxp_trace.h"
#include "tml.tmh"

void NfcTmlDeferredReadCompletion(DEVICE_CONTEXT* device,UINT_PTR buffer,UINT_PTR length,UINT_PTR generation)
{
    if(device->Stopping || generation!=static_cast<ULONG>(device->TmlReadGeneration)) { if(buffer) free(reinterpret_cast<void*>(buffer));return; }
    auto& read=device->TmlContext.Read;
    phTmlNfc_TransactInfo_t info={};
    bool complete=false;
    if(buffer==0) {
        // ReadAbort clears the reader but leaves already-posted messages/nodes.
        // Re-registration can also leave an extra message after the last node.
        if(!read.pBuffer || !read.pTmlCompletion || read.Queue.Flink==&read.Queue)
            return;
        // Reject a queued packet that exceeds either recorded object extent.
        // Complete the accepted reader with no partial data before it can rearm.
        auto entry=reinterpret_cast<_TML_READ_NOTIFICATION_ENTRY*>(read.Queue.Flink);
        read.Queue.Flink=entry->ListEntry.Flink;entry->ListEntry.Flink->Blink=&read.Queue;
        info.pBuff=read.pBuffer;
        if(entry->BufferLength>sizeof(entry->Buffer) || entry->BufferLength>read.wLength)
            info.wStatus=NFCSTATUS_BUFFER_TOO_SMALL;
        else {
            memcpy(read.pBuffer,entry->Buffer,entry->BufferLength);
            info.wLength=entry->BufferLength;
        }
        free(entry);complete=true;
    } else if(!read.pBuffer) {
        // Queue an owned complete NCI frame, including its three-byte header.
        // Reject larger input before touching its length or adjacent allocation.
        if(length<=sizeof(_TML_READ_NOTIFICATION_ENTRY::Buffer)) {
            auto entry=static_cast<_TML_READ_NOTIFICATION_ENTRY*>(malloc(sizeof(_TML_READ_NOTIFICATION_ENTRY)));
            if(entry) {
                entry->ListEntry.Flink=entry->ListEntry.Blink=&entry->ListEntry;
                memcpy(entry->Buffer,reinterpret_cast<void*>(buffer),length);
                entry->BufferLength=static_cast<USHORT>(length);
                entry->ListEntry.Flink=&read.Queue;entry->ListEntry.Blink=read.Queue.Blink;
                read.Queue.Blink->Flink=&entry->ListEntry;read.Queue.Blink=&entry->ListEntry;
            }
        }
    } else if(length<=read.wLength) {
        memcpy(read.pBuffer,reinterpret_cast<void*>(buffer),length);
        info.pBuff=read.pBuffer;info.wLength=static_cast<USHORT>(length);complete=true;
    } else {
        // Match the queued-reader error contract without a partial copy or a
        // silently pending accepted read. The shared path clears ownership first.
        info.pBuff=read.pBuffer;info.wStatus=NFCSTATUS_BUFFER_TOO_SMALL;complete=true;
    }
    if(complete) {
        const auto callback=read.pTmlCompletion;void* context=read.pContext;
        read.pTmlCompletion=nullptr;read.pContext=nullptr;read.pBuffer=nullptr;read.wLength=0;
        callback(context,&info);
    }
    if(buffer) { free(reinterpret_cast<void*>(buffer));if(read.hEvent) SetEvent(read.hEvent); }
}

void NfcTmlDeferredWriteCompletion(DEVICE_CONTEXT* device,UINT_PTR status,UINT_PTR length,UINT_PTR generation)
{
    if(device->Stopping || generation!=device->TmlWriteGeneration) return;
    auto& write=device->TmlContext.Write;
    if(!write.pTmlCompletion) return;
    device->TmlWriteCookie=nullptr;
    phTmlNfc_TransactInfo_t info={};
    info.wStatus=NT_SUCCESS(static_cast<NTSTATUS>(status))?NFCSTATUS_SUCCESS:NFCSTATUS_BOARD_COMMUNICATION_ERROR;
    info.wLength=static_cast<USHORT>(length);
    const auto callback=write.pTmlCompletion;void* context=write.pContext;
    write.pTmlCompletion=nullptr;write.pContext=nullptr;
    (void)SetEvent(write.hEvent);
    callback(context,&info);
}

NTSTATUS NfcTmlReadNotification(DEVICE_CONTEXT* device,WDFMEMORY memory)
{
    if(device->Stopping || !device->CoreInitialized) return STATUS_INVALID_DEVICE_STATE;
    if(WaitForSingleObject(device->TmlContext.Write.hEvent,2000)!=WAIT_OBJECT_0) return STATUS_IO_TIMEOUT;
    size_t length=0;
    const void* buffer=WdfMemoryGetBuffer(memory,&length);
    if(!buffer || !length || length>515) return STATUS_INVALID_BUFFER_SIZE;
    void* owned=malloc(length);
    if(!owned) return STATUS_INSUFFICIENT_RESOURCES;
    memcpy(owned,buffer,length);
    NfcPostMessage(device,2,reinterpret_cast<UINT_PTR>(NfcTmlDeferredReadCompletion),
        reinterpret_cast<UINT_PTR>(owned),length,static_cast<ULONG>(InterlockedCompareExchange(&device->TmlReadGeneration,0,0)));
    return STATUS_SUCCESS;
}

struct TML_WRITE_COOKIE { DEVICE_CONTEXT* Device;ULONG Generation; };
void NfcTmlWriteCompletion(WDFREQUEST,WDFIOTARGET,WDF_REQUEST_COMPLETION_PARAMS* parameters,void* context)
{
    auto cookie=static_cast<TML_WRITE_COOKIE*>(context);
    NfcPostMessage(cookie->Device,2,reinterpret_cast<UINT_PTR>(NfcTmlDeferredWriteCompletion),
        static_cast<UINT_PTR>(static_cast<LONG_PTR>(parameters->IoStatus.Status)),
        parameters->IoStatus.Information,cookie->Generation);
    free(cookie);
}

extern "C" NFCSTATUS phTmlNfc_Init(void* context)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    memset(&device->TmlContext,0,sizeof(device->TmlContext));
    auto& tml=device->TmlContext;
    tml.Read.Queue.Flink=tml.Read.Queue.Blink=&tml.Read.Queue;
    tml.Read.hEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(tml.Read.hEvent) {
        tml.Write.hEvent=CreateEventW(nullptr,TRUE,TRUE,nullptr);
        if(tml.Write.hEvent) return NFCSTATUS_SUCCESS;
    }
    phTmlNfc_Shutdown(context);
    return phOsalNfc_NfcStatusFromNtStatus(STATUS_INSUFFICIENT_RESOURCES);
}

extern "C" NFCSTATUS phTmlNfc_Shutdown(void* context)
{
    auto& tml=static_cast<DEVICE_CONTEXT*>(context)->TmlContext;
    while(tml.Read.Queue.Flink && tml.Read.Queue.Flink!=&tml.Read.Queue) {
        auto entry=tml.Read.Queue.Flink;
        tml.Read.Queue.Flink=entry->Flink;entry->Flink->Blink=&tml.Read.Queue;free(entry);
    }
    if(tml.Read.hEvent) { (void)CloseHandle(tml.Read.hEvent);tml.Read.hEvent=nullptr; }
    if(tml.Write.hEvent) (void)CloseHandle(tml.Write.hEvent);
    memset(&tml,0,sizeof(tml));return NFCSTATUS_SUCCESS;
}

extern "C" NFCSTATUS phTmlNfc_Read(void* context,UCHAR* buffer,USHORT length,
    pphTmlNfc_TransactCompletionCb_t callback,void* callbackContext)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(!device || device->Stopping || !buffer || !length || !callback) return NFCSTATUS_INVALID_PARAMETER;
    auto& read=device->TmlContext.Read;
    if(read.pTmlCompletion) return phOsalNfc_NfcStatusFromNtStatus(STATUS_INVALID_DEVICE_STATE);
    read.pContext=callbackContext;read.pBuffer=buffer;read.wLength=length;read.pTmlCompletion=callback;
    if(read.Queue.Flink!=&read.Queue)
        NfcPostMessage(device,2,reinterpret_cast<UINT_PTR>(NfcTmlDeferredReadCompletion),0,0,static_cast<ULONG>(device->TmlReadGeneration));
    return phOsalNfc_NfcStatusFromNtStatus(STATUS_PENDING);
}

extern "C" NFCSTATUS phTmlNfc_ReadAbort(void* context)
{
    if(!context) return NFCSTATUS_INVALID_PARAMETER;
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    InterlockedIncrement(&device->TmlReadGeneration);
    auto& read=device->TmlContext.Read;
    while(read.Queue.Flink && read.Queue.Flink!=&read.Queue) {
        auto entry=read.Queue.Flink;read.Queue.Flink=entry->Flink;entry->Flink->Blink=&read.Queue;free(entry);
    }
    read.pBuffer=nullptr;read.wLength=0;read.pTmlCompletion=nullptr;read.pContext=nullptr;
    return NFCSTATUS_SUCCESS;
}

extern "C" NFCSTATUS phTmlNfc_Write(void* context,UCHAR* buffer,USHORT length,
    pphTmlNfc_TransactCompletionCb_t callback,void* callbackContext)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(!device || device->Stopping || !device->m_pNfcProximityBus || !buffer || !length || !callback)
        return NFCSTATUS_INVALID_PARAMETER;
    auto& write=device->TmlContext.Write;
    if(write.pTmlCompletion) return NFCSTATUS_BUSY;
    auto cookie=static_cast<TML_WRITE_COOKIE*>(malloc(sizeof(TML_WRITE_COOKIE)));
    if(!cookie) return NFCSTATUS_INSUFFICIENT_RESOURCES;
    cookie->Device=device;cookie->Generation=++device->TmlWriteGeneration;
    device->TmlWriteCookie=cookie;
    write.pContext=callbackContext;write.pTmlCompletion=callback;
    (void)ResetEvent(write.hEvent);
    NTSTATUS status=device->m_pNfcProximityBus->WriteFile(device->FxDevice,buffer,length,NfcTmlWriteCompletion,cookie);
    if(status!=STATUS_PENDING) {
        // Rejected submissions have no asynchronous owner or callback.
        free(cookie);device->TmlWriteCookie=nullptr;write.pTmlCompletion=nullptr;write.pContext=nullptr;
        (void)SetEvent(write.hEvent);
    }
    return phOsalNfc_NfcStatusFromNtStatus(status);
}

extern "C" NFCSTATUS phTmlNfc_WriteAbort(void* context)
{
    if(!context) return NFCSTATUS_INVALID_PARAMETER;
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    // Invalidate the callback before cancelling the lower request. The bus
    // retains its owned bytes and request until Send/Cancel/completion all exit.
    ++device->TmlWriteGeneration;
    if(device->TmlWriteCookie && device->m_pNfcProximityBus) device->m_pNfcProximityBus->CancelWrite(device->TmlWriteCookie);
    device->TmlWriteCookie=nullptr;
    device->TmlContext.Write.pTmlCompletion=nullptr;
    device->TmlContext.Write.pContext=nullptr;
    if(device->TmlContext.Write.hEvent) SetEvent(device->TmlContext.Write.hEvent);
    return NFCSTATUS_SUCCESS;
}
