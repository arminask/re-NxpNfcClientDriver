#define RESHUB_USE_HELPER_ROUTINES
#include "NfcProximityBusI2c.h"
#include <new>
#include "nxp_buffer_validation.h"
#include "nci_packet.h"
#include "tml.h"
#include <reshub.h>
#include "nxp_trace.h"
#include "NfcProximityBusI2c.tmh"

INfcProximityBus* INfcProximityBus::m_BusBaseInstance=nullptr;
INfcProximityBus* INfcProximityBus::m_BusBaseInstance2=nullptr;

CNfcProximityBusI2C::CNfcProximityBusI2C(WDFDEVICE)
    : InterruptWaitLock(nullptr), SpbIoTarget(nullptr), DownloadPinIoTarget(nullptr), VenPinIoTarget(nullptr)
{
    InitializeSRWLock(&RawWriteLock);RawWrites.Flink=RawWrites.Blink=&RawWrites;
    m_bInitialized=false;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    m_bInitialized=true;
    // Local hardening: original target fields retained pool bytes, which early
    // SpbInitialize failure cleanup could pass to WdfObjectDelete. Resource
    // acquisition still belongs to SpbInitialize; empty ownership starts here.
}
CNfcProximityBusI2C::~CNfcProximityBusI2C()
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    m_BusBaseInstance=nullptr;
}
INfcProximityBus* CNfcProximityBusI2C::GetInstance(WDFDEVICE device)
{
    if(!m_BusBaseInstance) {
        auto instance=new(std::nothrow) CNfcProximityBusI2C(device);
        if(!instance) return nullptr;
        if(!instance->IsInitialized()) { delete instance;return nullptr; }
        m_BusBaseInstance=instance;
    }
    return m_BusBaseInstance;
}

void CNfcProximityBusI2C::SpbDestroy(DEVICE_CONTEXT* device)
{
    if(SpbIoTarget) { WdfIoTargetPurge(SpbIoTarget,WdfIoTargetPurgeIoAndWait);WdfObjectDelete(SpbIoTarget);SpbIoTarget=nullptr; }
    if(device->ReadInterrupt) {
        WdfInterruptAcquireLock(device->ReadInterrupt);
        auto head=&device->ReadList;
        while(head->Flink!=head) {
            auto entry=reinterpret_cast<_READ_REQUEST_CONTEXT*>(head->Flink);
            head->Flink=entry->ListEntry.Flink;entry->ListEntry.Flink->Blink=head;
            if(entry->Request) WdfObjectDelete(entry->Request);
        }
        WdfInterruptReleaseLock(device->ReadInterrupt);
    }
    if(device->ReadInterrupt) { WdfObjectDelete(device->ReadInterrupt);device->ReadInterrupt=nullptr; }
    if(InterruptWaitLock) { WdfObjectDelete(InterruptWaitLock);InterruptWaitLock=nullptr; }
    if(DownloadPinIoTarget) { WdfObjectDelete(DownloadPinIoTarget);DownloadPinIoTarget=nullptr; }
    if(VenPinIoTarget) { WdfObjectDelete(VenPinIoTarget);VenPinIoTarget=nullptr; }
}

NTSTATUS CNfcProximityBusI2C::SpbGpioWrite(WDFDEVICE device,UCHAR set,WDFIOTARGET target)
{
    if(!target) return STATUS_INVALID_DEVICE_STATE;
    UCHAR value=set!=0;
    WDFREQUEST request=nullptr;
    WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
    NTSTATUS status=WdfRequestCreate(&attributes,target,&request);
    if(NT_SUCCESS(status)) {
        WDFMEMORY memory=nullptr;
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=request;
        status=WdfMemoryCreatePreallocated(&attributes,&value,sizeof(value),&memory);
        if(NT_SUCCESS(status)) {
            // Original RVA 0x8533/0x853e supplies the same one-byte memory
            // object for both the output and input IOCTL arguments.
            status=WdfIoTargetFormatRequestForIoctl(target,request,0x480004,memory,nullptr,memory,nullptr);
            if(NT_SUCCESS(status)) {
                WDF_REQUEST_SEND_OPTIONS options;WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
                WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(2));
                const BOOLEAN sent=WdfRequestSend(request,target,&options);
                status=WdfRequestGetStatus(request);
                if(!sent && NT_SUCCESS(status)) status=STATUS_IO_DEVICE_ERROR;
            }
        }
    }
    if(request) WdfObjectDelete(request);
    return status;
}

// A submission and its terminal callback each own the raw request. Abort adds
// an owner before leaving the list lock. Only the final owner deletes it, so a
// completion inside Send/Cancel cannot delete a request still used by either.
static void ReleaseRawWrite(WRITE_REQUEST_CONTEXT* write)
{
    if(InterlockedDecrement(&write->Owners)==0) WdfObjectDelete(write->Request);
}
static void RemoveRawWrite(WRITE_REQUEST_CONTEXT* write)
{
    write->Link.Blink->Flink=write->Link.Flink;write->Link.Flink->Blink=write->Link.Blink;
    write->Completed=true;
}
void CNfcProximityBusI2C::CancelWrite(void* context)
{
    WRITE_REQUEST_CONTEXT* found=nullptr;
    AcquireSRWLockExclusive(&RawWriteLock);
    for(auto link=RawWrites.Flink;link!=&RawWrites;link=link->Flink) {
        auto write=CONTAINING_RECORD(link,WRITE_REQUEST_CONTEXT,Link);
        if(write->CompletionContext==context) {
            write->Aborted=true;
            if(write->Sent) { InterlockedIncrement(&write->Owners);found=write; }
            break;
        }
    }
    ReleaseSRWLockExclusive(&RawWriteLock);
    if(found) { (void)WdfRequestCancelSentRequest(found->Request);ReleaseRawWrite(found); }
}

// Raw TML/vendor writes own a snapshot until the lower target completes. No
// automatic retry is safe after a possibly partial controller command.
NTSTATUS CNfcProximityBusI2C::WriteFile(WDFDEVICE device,UCHAR* buffer,USHORT length,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE callback,void* context)
{
    if(!SpbIoTarget) return STATUS_INVALID_DEVICE_STATE;
    if(!buffer || !length || !callback) return STATUS_INVALID_PARAMETER;
    WDFREQUEST request=nullptr;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,WRITE_REQUEST_CONTEXT);attributes.ParentObject=device;
    NTSTATUS status=WdfRequestCreate(&attributes,SpbIoTarget,&request);
    if(NT_SUCCESS(status)) {
        auto write=GetWriteRequestContext(request);
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=request;
        void* owned=nullptr;
        status=WdfMemoryCreate(&attributes,PagedPool,0,length,&write->Memory,&owned);
        if(NT_SUCCESS(status)) {
            memcpy(owned,buffer,length);
            write->CompletionRoutine=callback;write->CompletionContext=context;
            write->ExpectedLength=length;
            status=WdfIoTargetFormatRequestForWrite(SpbIoTarget,request,write->Memory,nullptr,nullptr);
            if(NT_SUCCESS(status)) {
                WdfRequestSetCompletionRoutine(request,SpbWriteToDeviceRequestCompletion,nullptr);
                WDF_REQUEST_SEND_OPTIONS options;WDF_REQUEST_SEND_OPTIONS_INIT(&options,0);
                WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(2));
                write->Bus=this;write->Request=request;write->Owners=2;
                AcquireSRWLockExclusive(&RawWriteLock);
                write->Link.Flink=&RawWrites;write->Link.Blink=RawWrites.Blink;
                RawWrites.Blink->Flink=&write->Link;RawWrites.Blink=&write->Link;
                ReleaseSRWLockExclusive(&RawWriteLock);
                const BOOLEAN sent=WdfRequestSend(request,SpbIoTarget,&options);
                bool cancel=false;
                AcquireSRWLockExclusive(&RawWriteLock);
                if(!sent) RemoveRawWrite(write);
                else if(!write->Completed) { write->Sent=true;cancel=write->Aborted; }
                ReleaseSRWLockExclusive(&RawWriteLock);
                if(!sent) { status=WdfRequestGetStatus(request);if(NT_SUCCESS(status)) status=STATUS_IO_DEVICE_ERROR;ReleaseRawWrite(write); }
                else if(cancel) (void)WdfRequestCancelSentRequest(request);
                ReleaseRawWrite(write);
                return sent?STATUS_PENDING:status;
            }
        }
    }
    if(request) WdfObjectDelete(request);
    return status;
}

NTSTATUS CNfcProximityBusI2C::SpbWriteRequest(WDFDEVICE,WDFREQUEST request)
{
    if(!SpbIoTarget) return STATUS_INVALID_DEVICE_STATE;
    WDFMEMORY memory=nullptr;
    NTSTATUS status=WdfRequestRetrieveInputMemory(request,&memory);
    if(!NT_SUCCESS(status)) return status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,WRITE_REQUEST_CONTEXT);
    WRITE_REQUEST_CONTEXT* write=nullptr;
    status=WdfObjectAllocateContext(request,&attributes,reinterpret_cast<void**>(&write));
    if(!NT_SUCCESS(status)) return status;
    write->CompletionRoutine=nullptr;
    (void)WdfMemoryGetBuffer(memory,&write->ExpectedLength);
    status=WdfIoTargetFormatRequestForWrite(SpbIoTarget,request,memory,nullptr,nullptr);
    if(!NT_SUCCESS(status)) return status;
    // The forwarded request is owned by the target until its completion. WDF
    // propagates cancellation; there is no separately cancelable parent/child.
    WdfRequestSetCompletionRoutine(request,SpbWriteToDeviceRequestCompletion,nullptr);
    WDF_REQUEST_SEND_OPTIONS options;WDF_REQUEST_SEND_OPTIONS_INIT(&options,0);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(2));
    if(WdfRequestSend(request,SpbIoTarget,&options)) return STATUS_PENDING;
    status=WdfRequestGetStatus(request);
    return NT_SUCCESS(status)?STATUS_IO_DEVICE_ERROR:status; // caller completes a rejected send
}

void CNfcProximityBusI2C::SpbWriteToDeviceRequestCompletion(WDFREQUEST request,WDFIOTARGET target,
    WDF_REQUEST_COMPLETION_PARAMS* parameters,void*)
{
    auto write=GetWriteRequestContext(request);
    NTSTATUS status=parameters->IoStatus.Status;
    ULONG_PTR transferred=parameters->IoStatus.Information;
    if(NT_SUCCESS(status) && transferred!=write->ExpectedLength) status=STATUS_DEVICE_DATA_ERROR;
    if(!NT_SUCCESS(status)) transferred=0;
    if(write->CompletionRoutine) {
        AcquireSRWLockExclusive(&write->Bus->RawWriteLock);
        RemoveRawWrite(write);
        ReleaseSRWLockExclusive(&write->Bus->RawWriteLock);
        WDF_REQUEST_COMPLETION_PARAMS completion=*parameters;
        completion.IoStatus.Status=status;completion.IoStatus.Information=transferred;
        write->CompletionRoutine(request,target,&completion,write->CompletionContext);
        ReleaseRawWrite(write);
    } else WdfRequestCompleteWithInformation(request,status,transferred);
}

NTSTATUS CNfcProximityBusI2C::SpbAllocateResourceForRead(WDFDEVICE device,WDFIOTARGET target,WDFREQUEST* output)
{
    auto deviceContext=GetDeviceContext(device);
    WDFREQUEST request=nullptr;
    WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
    NTSTATUS status=WdfRequestCreate(&attributes,target,&request);
    if(NT_SUCCESS(status)) {
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,READ_REQUEST_CONTEXT);
        READ_REQUEST_CONTEXT* read=nullptr;
        status=WdfObjectAllocateContext(request,&attributes,reinterpret_cast<void**>(&read));
        if(NT_SUCCESS(status)) {
            const size_t capacity=deviceContext->DeviceMode==DEVICE_MODE_FIRMWARE &&
                deviceContext->FirmwareMode==FIRMWARE_MODE_RAW?515:258;
            WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=request;
            WDFMEMORY memory=nullptr;
            status=WdfMemoryCreate(&attributes,PagedPool,0,capacity,&memory,nullptr);
            if(NT_SUCCESS(status)) {
                read->ListEntry.Flink=read->ListEntry.Blink=&read->ListEntry;
                read->Memory=memory;read->Mode=deviceContext->FirmwareMode;read->Request=request;
            }
        }
    }
    if(!NT_SUCCESS(status)) { if(request) WdfObjectDelete(request); }
    else *output=request;
    return status;
}

NTSTATUS CNfcProximityBusI2C::SpbRead(WDFDEVICE device,WDFREQUEST* output)
{
    if(!output || !SpbIoTarget) return STATUS_INVALID_PARAMETER;
    *output=nullptr;
    auto deviceContext=GetDeviceContext(device);
    WDFREQUEST request=nullptr;
    NTSTATUS status=SpbAllocateResourceForRead(device,SpbIoTarget,&request);
    if(NT_SUCCESS(status)) do {
        auto read=GetReadRequestContext(request);
        const WDFMEMORY storage=read->Memory;
        size_t capacity;
        auto buffer=static_cast<UCHAR*>(WdfMemoryGetBuffer(storage,&capacity));
        // Original leaves a successful allocation status and caller output
        // untouched when GetBuffer unexpectedly returns null.
        if(!buffer) { status=STATUS_INSUFFICIENT_RESOURCES;break; }
        const size_t headerLength=deviceContext->DeviceMode==DEVICE_MODE_FIRMWARE &&
            deviceContext->FirmwareMode==FIRMWARE_MODE_RAW?2:3;
        if(capacity<headerLength) { status=STATUS_INVALID_BUFFER_SIZE;break; }
        WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=request;
        WDFMEMORY headerMemory=nullptr,payloadMemory=nullptr;
        status=WdfMemoryCreatePreallocated(&attributes,buffer,headerLength,&headerMemory);
        if(!NT_SUCCESS(status)) break;
        WDF_MEMORY_DESCRIPTOR descriptor;WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&descriptor,headerMemory,nullptr);
        ULONG_PTR transferred=0;
        WDF_REQUEST_SEND_OPTIONS options;WDF_REQUEST_SEND_OPTIONS_INIT(&options,0);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(2));
        status=WdfIoTargetSendReadSynchronously(SpbIoTarget,request,&descriptor,nullptr,&options,&transferred);
        if(!NT_SUCCESS(status)) break;
        if(transferred!=headerLength) { status=STATUS_INVALID_PARAMETER;break; }
        size_t payloadLength;
        if(deviceContext->DeviceMode==DEVICE_MODE_FIRMWARE && deviceContext->FirmwareMode==FIRMWARE_MODE_RAW) {
            payloadLength=NxpDownloadPayloadLength(buffer);
        } else {
            NCI_PACKET_HEADER header={};
            if(!NciPacketHeaderGetFromBuffer(buffer,3,&header)) { status=STATUS_INVALID_PARAMETER;break; }
            payloadLength=header.PayloadLength;
        }
        if(payloadLength>capacity-headerLength) { status=STATUS_INVALID_BUFFER_SIZE;break; }
        if(payloadLength) {
            status=WdfMemoryCreatePreallocated(&attributes,buffer+headerLength,payloadLength,&payloadMemory);
            if(!NT_SUCCESS(status)) break;
            WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&descriptor,payloadMemory,nullptr);
            WDF_REQUEST_REUSE_PARAMS reuse;WDF_REQUEST_REUSE_PARAMS_INIT(&reuse,0,STATUS_SUCCESS);
            status=WdfRequestReuse(request,&reuse);
            if(!NT_SUCCESS(status)) break;
            transferred=0;
            status=WdfIoTargetSendReadSynchronously(SpbIoTarget,request,&descriptor,nullptr,&options,&transferred);
            if(!NT_SUCCESS(status)) break;
            if(transferred!=payloadLength) { status=STATUS_INVALID_PARAMETER;break; }
        }
        void* data=WdfMemoryGetBuffer(storage,&capacity);
        status=WdfMemoryCreatePreallocated(&attributes,data,headerLength+payloadLength,&read->Memory);
        if(NT_SUCCESS(status)) *output=request;
    } while(false);
    if(!NT_SUCCESS(status) && request) WdfObjectDelete(request);
    return status;
}

NTSTATUS CNfcProximityBusI2C::SpbOpenIoTarget(WDFDEVICE device,WDFCMRESLIST resources,ULONG index,
    ULONG type,WDFIOTARGET* output)
{
    if(!output) return STATUS_INVALID_PARAMETER;
    *output=nullptr;
    UNICODE_STRING path={};
    path.Buffer=static_cast<WCHAR*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,136));
    if(!path.Buffer) return STATUS_INSUFFICIENT_RESOURCES;
    path.Buffer[0]=0;path.MaximumLength=136;
    auto resource=WdfCmResourceListGetDescriptor(resources,index);
    if(!resource || resource->Type!=CmResourceTypeConnection) { HeapFree(GetProcessHeap(),0,path.Buffer);return STATUS_DEVICE_CONFIGURATION_ERROR; }
    WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    WDFIOTARGET target=nullptr;
    NTSTATUS status=WdfIoTargetCreate(device,&attributes,&target);
    if(NT_SUCCESS(status)) {
        status=RESOURCE_HUB_CREATE_PATH_FROM_ID(&path,resource->u.Connection.IdLowPart,resource->u.Connection.IdHighPart);
        if(NT_SUCCESS(status)) {
            WDF_IO_TARGET_OPEN_PARAMS open;
            WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,type==1?GENERIC_READ|GENERIC_WRITE:GENERIC_WRITE);
            // UMDF uses Win32 dispositions. Open the existing Resource Hub
            // connection, as documented for user-mode SPB peripherals.
            open.CreateDisposition=OPEN_EXISTING;
            open.FileAttributes=FILE_FLAG_OVERLAPPED;
            status=WdfIoTargetOpen(target,&open);
        }
    }
    if(NT_SUCCESS(status)) *output=target;
    else if(target) WdfObjectDelete(target);
    (void)HeapFree(GetProcessHeap(),0,path.Buffer);
    return status;
}

NTSTATUS CNfcProximityBusI2C::SpbInitialize(DEVICE_CONTEXT* device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    const ULONG count=WdfCmResourceListGetCount(translated);
    if(!count || WdfCmResourceListGetCount(raw)!=count) return STATUS_DEVICE_CONFIGURATION_ERROR;
    ULONG serialIndex=0,interruptIndex=0,downloadIndex=0,venIndex=0,gpios=0,serials=0,interrupts=0;
    for(ULONG index=0;index<count;++index) {
        auto resource=WdfCmResourceListGetDescriptor(translated,index);
        if(!resource) return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(resource->Type==CmResourceTypeInterrupt) { ++interrupts;interruptIndex=index; }
        else if(resource->Type==CmResourceTypeConnection) {
            if(resource->u.Connection.Class==CM_RESOURCE_CONNECTION_CLASS_GPIO && resource->u.Connection.Type==CM_RESOURCE_CONNECTION_TYPE_GPIO_IO) {
                if(!gpios) downloadIndex=index;else if(gpios==1) venIndex=index;
                ++gpios;
            } else if(resource->u.Connection.Class==CM_RESOURCE_CONNECTION_CLASS_SERIAL && resource->u.Connection.Type==CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C) {
                ++serials;serialIndex=index;
            }
        }
    }
    if(serials!=1 || interrupts!=1 || gpios<2) return STATUS_DEVICE_CONFIGURATION_ERROR;
    auto rawInterrupt=WdfCmResourceListGetDescriptor(raw,interruptIndex);
    if(!rawInterrupt || rawInterrupt->Type!=CmResourceTypeInterrupt) return STATUS_DEVICE_CONFIGURATION_ERROR;
    // Existing two-GPIO boards retain their recorded DOWNLOAD, VEN order.
    // Other boards can explicitly identify both translated resource indices;
    // connection IDs themselves contain no pin-role information.
    WDFKEY key=nullptr;ULONG configuredDownload=0,configuredVen=0;
    bool hasDownload=false,hasVen=false;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        UNICODE_STRING name;RtlInitUnicodeString(&name,L"DownloadGpioResourceIndex");
        NTSTATUS downloadStatus=WdfRegistryQueryULong(key,&name,&configuredDownload);
        RtlInitUnicodeString(&name,L"VenGpioResourceIndex");
        NTSTATUS venStatus=WdfRegistryQueryULong(key,&name,&configuredVen);WdfRegistryClose(key);
        if((!NT_SUCCESS(downloadStatus) && downloadStatus!=STATUS_OBJECT_NAME_NOT_FOUND) ||
           (!NT_SUCCESS(venStatus) && venStatus!=STATUS_OBJECT_NAME_NOT_FOUND)) return STATUS_DEVICE_CONFIGURATION_ERROR;
        hasDownload=NT_SUCCESS(downloadStatus);hasVen=NT_SUCCESS(venStatus);
    } else if(status!=STATUS_OBJECT_NAME_NOT_FOUND) return status;
    if(hasDownload!=hasVen || (gpios!=2 && !hasDownload)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    if(hasDownload) { downloadIndex=configuredDownload;venIndex=configuredVen; }
    auto isGpio=[&](ULONG index) {
        if(index>=count) return false;
        auto resource=WdfCmResourceListGetDescriptor(translated,index);
        return resource && resource->Type==CmResourceTypeConnection && resource->u.Connection.Class==CM_RESOURCE_CONNECTION_CLASS_GPIO && resource->u.Connection.Type==CM_RESOURCE_CONNECTION_TYPE_GPIO_IO;
    };
    if(downloadIndex==venIndex || !isGpio(downloadIndex) || !isGpio(venIndex)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    auto downloadResource=WdfCmResourceListGetDescriptor(translated,downloadIndex);
    auto venResource=WdfCmResourceListGetDescriptor(translated,venIndex);
    if(downloadResource->u.Connection.IdLowPart==venResource->u.Connection.IdLowPart &&
       downloadResource->u.Connection.IdHighPart==venResource->u.Connection.IdHighPart) return STATUS_DEVICE_CONFIGURATION_ERROR;
    status=SpbOpenIoTarget(device->FxDevice,translated,serialIndex,1,&SpbIoTarget);
    if(NT_SUCCESS(status)) status=SpbOpenIoTarget(device->FxDevice,translated,downloadIndex,2,&DownloadPinIoTarget);
    if(NT_SUCCESS(status)) status=SpbOpenIoTarget(device->FxDevice,translated,venIndex,2,&VenPinIoTarget);
    if(NT_SUCCESS(status)) {
        WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device->FxDevice;
        status=WdfWaitLockCreate(&attributes,&InterruptWaitLock);
        if(NT_SUCCESS(status)) {
            WDF_INTERRUPT_CONFIG config;WDF_INTERRUPT_CONFIG_INIT(&config,OnInterruptIsr,nullptr);
            config.WaitLock=InterruptWaitLock;config.InterruptTranslated=WdfCmResourceListGetDescriptor(translated,interruptIndex);
            config.InterruptRaw=rawInterrupt;config.EvtInterruptWorkItem=OnInterruptWorkItem;
            status=WdfInterruptCreate(device->FxDevice,&config,WDF_NO_OBJECT_ATTRIBUTES,&device->ReadInterrupt);
        }
    }
    if(!NT_SUCCESS(status)) SpbDestroy(device);
    return status;
}

BOOLEAN CNfcProximityBusI2C::OnInterruptIsr(WDFINTERRUPT interrupt,ULONG)
{
    auto device=GetDeviceContext(WdfInterruptGetDevice(interrupt));
    WDFREQUEST request=nullptr;
    auto bus=static_cast<CNfcProximityBusI2C*>(device->m_pNfcProximityBus);
    if(!bus || device->Stopping) return TRUE;
    const NTSTATUS status=bus->SpbRead(WdfInterruptGetDevice(interrupt),&request);
    if(NT_SUCCESS(status)) {
        auto read=GetReadRequestContext(request);auto head=&device->ReadList;
        read->ListEntry.Flink=head;read->ListEntry.Blink=head->Blink;
        head->Blink->Flink=&read->ListEntry;head->Blink=&read->ListEntry;
        (void)WdfInterruptQueueWorkItemForIsr(interrupt);
    }
    if(!NT_SUCCESS(status) && request) WdfObjectDelete(request);
    return TRUE;
}

void CNfcProximityBusI2C::OnInterruptWorkItem(WDFINTERRUPT interrupt,WDFOBJECT associatedObject)
{
    auto device=GetDeviceContext(WdfInterruptGetDevice(interrupt));
    (void)WdfWaitLockAcquire(device->WorkItemLock,nullptr);
    if(device->Stopping) { WdfWaitLockRelease(device->WorkItemLock);return; }
    WdfInterruptAcquireLock(interrupt);
    auto head=&device->ReadList;
    while(head->Flink!=head) {
        auto read=reinterpret_cast<READ_REQUEST_CONTEXT*>(head->Flink);
        head->Flink=read->ListEntry.Flink;read->ListEntry.Flink->Blink=head;
        if(device->DeviceState==DEVICE_STATE_UPDATING_MODE) {
            if(read->Request) WdfObjectDelete(read->Request);
            continue;
        }
        WdfInterruptReleaseLock(interrupt);
        if(device->VendorTestEnabled) (void)NfcVendorTestReadNotification(device,read->Memory);
        else if(device->DeviceMode==DEVICE_MODE_NCI && !NxpProprietaryNciPacketFilter(device,read->Memory) &&
                !NfcIsSequenceInProgress(device)) {
            (void)NfcCxNciReadNotification(static_cast<WDFDEVICE>(associatedObject),read->Memory);
        } else (void)NfcTmlReadNotification(device,read->Memory);
        if(read->Request) WdfObjectDelete(read->Request);
        WdfInterruptAcquireLock(interrupt);
    }
    WdfInterruptReleaseLock(interrupt);WdfWaitLockRelease(device->WorkItemLock);
}
