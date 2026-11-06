#include "driver.h"
#include "tml.h"
#include <stdlib.h>
#include "../NxpCoreLib/phNciNfc.h"
#include "NfcProximityBusI2c.h"
#include "NfcNxpProprietary.h"
#include "vendortest.h"
extern "C" {
#include <phOsalNfc.h>
#include <phOsalNfc_Timer.h>
}
#include "nxp_trace.h"
#include "device.tmh"

NTSTATUS GetHardwareId(WDFDEVICE device)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
    WDFMEMORY memory;
    NTSTATUS status=WdfDeviceAllocAndQueryProperty(device,DevicePropertyHardwareID,NonPagedPool,&attributes,&memory);
    if(!NT_SUCCESS(status)) return status;
    size_t size;
    const WCHAR* id=static_cast<const WCHAR*>(WdfMemoryGetBuffer(memory,&size));
    if(!id) return STATUS_UNSUCCESSFUL;
    // The WDF memory remains parented to the device for this borrowed pointer.
    context->HardwareId=id;
    return status;
}

NTSTATUS NfcGetControllerCapabilities(DEVICE_CONTEXT* context,phNciNfc_NfccCapabilities* capabilities)
{
    if(!context || !capabilities) return STATUS_INVALID_PARAMETER;
    AcquireSRWLockShared(&context->ControllerInfoLock);
    NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
    if(!context->Stopping && context->pNciHandle)
        status=phOsalNfc_NtStatusFromNfcStatus(phNciNfc_GetNfccCapabilities(context->pNciHandle,capabilities));
    ReleaseSRWLockShared(&context->ControllerInfoLock);return status;
}

NTSTATUS OnD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    UNREFERENCED_PARAMETER(previous);
    if(context->CoreInitialized) return STATUS_SUCCESS;
    phOsalNfc_Config_t config={};config.pfnCallback=NfcMessageHandler;config.pCallbackContext=context;
    NTSTATUS status=phOsalNfc_NtStatusFromNfcStatus(phOsalNfc_Init(&config));
    if(!NT_SUCCESS(status)) return status;
    context->MsgQueueThreadId=config.dwCallbackThreadId;
    context->StartThreadEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!context->StartThreadEvent) { phOsalNfc_DeInit();return STATUS_INSUFFICIENT_RESOURCES; }
    status=phOsalNfc_NtStatusFromNfcStatus(phTmlNfc_Init(context));
    if(!NT_SUCCESS(status)) { CloseHandle(context->StartThreadEvent);context->StartThreadEvent=nullptr;phOsalNfc_DeInit();return status; }
    context->Stopping=TRUE;
    status=phOsalNfc_NtStatusFromNfcStatus(phNciNfc_Initialise(context,TRUE,&context->pNciHandle));
    if(!NT_SUCCESS(status)) { phTmlNfc_Shutdown(context);CloseHandle(context->StartThreadEvent);context->StartThreadEvent=nullptr;phOsalNfc_DeInit();return status; }
    context->CoreInitialized=TRUE;context->Nfcc_Version=0;context->FWCheckComplete=FALSE;
    InterlockedExchange(&context->Stopping,FALSE);
    return STATUS_SUCCESS;
}

// Run protocol disposal on the message thread, after all lower write
// completions have been posted. Timer and protocol callbacks share this thread.
static void ShutdownCoreOnWorker(DEVICE_CONTEXT* context,UINT_PTR event,UINT_PTR,UINT_PTR)
{
    phOsalNfc_Timer_StopAll();
    if(context->pSeqHandler) NfcSequenceComplete(context,STATUS_CANCELLED);
    NfcFWFreeFullFWImage(context);
    AcquireSRWLockExclusive(&context->ControllerInfoLock);
    if(context->pNciHandle) { phNciNfc_Deinitialise(context->pNciHandle);context->pNciHandle=nullptr; }
    ReleaseSRWLockExclusive(&context->ControllerInfoLock);
    phTmlNfc_Shutdown(context);
    context->pSeqHandler=nullptr;context->SeqNext=context->SeqMax=0;
    if(context->VendorTestEnabled) {
        WdfIoQueuePurgeSynchronously(context->VendorTestContext.ReadQueue);
        WdfWaitLockAcquire(context->VendorTestContext.ReadQueueLock,nullptr);
        auto head=&context->VendorTestContext.ReadNotificationQueue;
        while(head->Flink!=head) { auto entry=head->Flink;head->Flink=entry->Flink;entry->Flink->Blink=head;free(entry); }
        WdfWaitLockRelease(context->VendorTestContext.ReadQueueLock);
    }
    SetEvent(reinterpret_cast<HANDLE>(event));
}
static void ShutdownCore(DEVICE_CONTEXT* context)
{
    if(!context->CoreInitialized) return;
    InterlockedExchange(&context->Stopping,TRUE);
    auto bus=static_cast<CNfcProximityBusI2C*>(context->m_pNfcProximityBus);
    if(bus && bus->SpbIoTarget) WdfIoTargetPurge(bus->SpbIoTarget,WdfIoTargetPurgeIoAndWait);
    // D0Exit runs after interrupts have been disabled by WDF. Flush any queued
    // receive work before freeing the TML queues or its event handles.
    WdfWaitLockAcquire(context->WorkItemLock,nullptr);
    WdfWaitLockRelease(context->WorkItemLock);
    HANDLE finished=context->StartThreadEvent;
    ResetEvent(finished);
    NfcPostMessage(context,2,reinterpret_cast<UINT_PTR>(ShutdownCoreOnWorker),reinterpret_cast<UINT_PTR>(finished),0,0);
    WaitForSingleObject(finished,INFINITE); // memory cannot be released before the barrier
    CloseHandle(finished);context->StartThreadEvent=nullptr;
    phOsalNfc_DeInit();context->CoreInitialized=FALSE;
}
NTSTATUS OnD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    if(target!=WdfPowerDeviceD2) ShutdownCore(GetDeviceContext(device));
    return STATUS_SUCCESS;
}

void OnSelfManagedIoCleanup(WDFDEVICE device)
{ (void)NfcCxDeviceDeinitialize(device); }

void OnDeviceSurpriseRemoval(WDFDEVICE device)
{
    (void)NfcCxReleaseHardwareControl(device);
    (void)NfcCxDeviceDeinitialize(device);
}

static PVOID volatile ActiveDevice=nullptr;

NTSTATUS OnPrepareHardware(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    if(InterlockedCompareExchangePointer(&ActiveDevice,context,nullptr)!=nullptr) return STATUS_DEVICE_BUSY;
    NTSTATUS status=GetHardwareId(device);
    if(!NT_SUCCESS(status)) { InterlockedCompareExchangePointer(&ActiveDevice,nullptr,context);return status; }
    ULONG count=WdfCmResourceListGetCount(translated);
    context->m_pNfcProximityBus=nullptr;
    if(!context->HardwareId || !count) { InterlockedCompareExchangePointer(&ActiveDevice,nullptr,context);return STATUS_DEVICE_CONFIGURATION_ERROR; }
    auto bus=static_cast<CNfcProximityBusI2C*>(CNfcProximityBusI2C::GetInstance(device));
    context->m_pNfcProximityBus=bus;
    if(!bus) { InterlockedCompareExchangePointer(&ActiveDevice,nullptr,context);return STATUS_INSUFFICIENT_RESOURCES; }
    if(bus) {
        context->TransportPlatfrom=TRANSPORT_SPBI2C;
        status=bus->SpbInitialize(context,raw,translated);
    }
    if(!NT_SUCCESS(status) && bus) { bus->SpbDestroy(context);delete bus;context->m_pNfcProximityBus=nullptr;InterlockedCompareExchangePointer(&ActiveDevice,nullptr,context); }
    return status;
}

NTSTATUS OnReleaseHardware(WDFDEVICE device,WDFCMRESLIST)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    ShutdownCore(context);
    if(context->TransportPlatfrom==TRANSPORT_SPBI2C && context->m_pNfcProximityBus) {
        auto bus=static_cast<CNfcProximityBusI2C*>(context->m_pNfcProximityBus);
        bus->SpbDestroy(context);delete bus;
        context->m_pNfcProximityBus=nullptr;
    }
    free(context->RfSettings.Buffer);context->RfSettings={};
    memset(&context->EEPromSettingSignature,0,sizeof(GUID));context->DlmaCfg.Enable=0;
    free(context->DriverStore_EEPromParams);context->DriverStore_EEPromParams=nullptr;
    free(context->UpdateEEPromUserconfigParams);context->UpdateEEPromUserconfigParams=nullptr;
    InterlockedCompareExchangePointer(&ActiveDevice,nullptr,context);
    return STATUS_SUCCESS;
}

NTSTATUS OnD0EntryPostInterruptsEnabled(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    if(previous==WdfPowerDeviceD2) return STATUS_SUCCESS;
    if(context->VendorTestEnabled) WdfIoQueueStart(context->VendorTestContext.ReadQueue);
    if(context->TransportPlatfrom==TRANSPORT_SPBI2C) {
        auto bus=static_cast<CNfcProximityBusI2C*>(context->m_pNfcProximityBus);
        if(!bus) return STATUS_INVALID_DEVICE_STATE;
        if(bus->SpbIoTarget) { NTSTATUS started=WdfIoTargetStart(bus->SpbIoTarget);if(!NT_SUCCESS(started)) return started; }
        NTSTATUS gpio=bus->SpbGpioWrite(context->FxDevice,TRUE,bus->VenPinIoTarget);
        if(!NT_SUCCESS(gpio)) return gpio;
        gpio=bus->SpbGpioWrite(context->FxDevice,FALSE,bus->DownloadPinIoTarget);
        if(!NT_SUCCESS(gpio)) return gpio;
        context->bCxHardwareEvent=TRUE;
    } else if(context->bCxHardwareEvent!=TRUE) return STATUS_SUCCESS;
    NFC_CX_HARDWARE_EVENT event={STATUS_SUCCESS,HostActionStart};
    NTSTATUS status=NfcCxHardwareEvent(device,&event);
    if(!NT_SUCCESS(status)) ++context->HWFailedAttempts;
    else {
        if(!context->HWFailedAttempts) return status;
        context->HWFailedAttempts=0;
    }
    (void)RegistryWritePersistedDeviceParameters(context);
    return status;
}

NTSTATUS OnD0ExitPreInterruptsDisabled(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    NTSTATUS status=STATUS_SUCCESS;
    if(target!=WdfPowerDeviceD2) {
        if(context->bCxHardwareEvent==TRUE) {
            NFC_CX_HARDWARE_EVENT event={STATUS_SUCCESS,HostActionStop};
            status=NfcCxHardwareEvent(device,&event);
            if(NT_SUCCESS(status)) context->bCxHardwareEvent=FALSE;

        }
        if(context->TransportPlatfrom==TRANSPORT_SPBI2C) {
            auto bus=static_cast<CNfcProximityBusI2C*>(context->m_pNfcProximityBus);
            if(bus) { NTSTATUS gpio=bus->SpbGpioWrite(context->FxDevice,FALSE,bus->VenPinIoTarget);if(NT_SUCCESS(status)) status=gpio; }
        }
    }
    return status;
}

void NfcEvtWriteNciPacket(WDFDEVICE device,WDFREQUEST request)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    if(!request) return;
    WDFMEMORY memory;
    NTSTATUS status=WdfRequestRetrieveInputMemory(request,&memory);
    if(NT_SUCCESS(status)) {
        size_t length=0;auto bytes=static_cast<UCHAR*>(WdfMemoryGetBuffer(memory,&length));
        if(context->Stopping || !bytes || length<3 || length>258 || length!=size_t(bytes[2])+3 || !context->ReadInterrupt || !context->m_pNfcProximityBus) { WdfRequestComplete(request,STATUS_INVALID_DEVICE_STATE);return; }
        NxpPrepareNciPacketForWrite(context,memory);
        if(context->TransportPlatfrom==TRANSPORT_SPBI2C) {
            WdfInterruptAcquireLock(context->ReadInterrupt);
            bool updating=context->DeviceState==DEVICE_STATE_UPDATING_MODE;
            WdfInterruptReleaseLock(context->ReadInterrupt);
            if(updating) status=STATUS_INVALID_PARAMETER;
            else status=static_cast<CNfcProximityBusI2C*>(context->m_pNfcProximityBus)->SpbWriteRequest(device,request);
        }
    }
    if(status!=STATUS_PENDING) WdfRequestCompleteWithInformation(request,status,0);
}

void NfcEvtDeviceIoControl(WDFDEVICE device,WDFREQUEST request,size_t outputLength,size_t inputLength,ULONG code)
{
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    NTSTATUS status=STATUS_NOT_SUPPORTED;
    WDFMEMORY outputMemory=nullptr,inputMemory=nullptr;
    UCHAR* output=nullptr;UCHAR* input=nullptr;size_t outputSize=0,inputSize=0;
    if(inputLength>=65536 || outputLength>=65536) status=STATUS_INVALID_PARAMETER;
    else {
        if(outputLength) {
            status=WdfRequestRetrieveOutputMemory(request,&outputMemory);
            if(!NT_SUCCESS(status)) goto complete;
            output=static_cast<UCHAR*>(WdfMemoryGetBuffer(outputMemory,&outputSize));
        }
        if(inputLength) {
            status=WdfRequestRetrieveInputMemory(request,&inputMemory);
            if(!NT_SUCCESS(status)) goto complete;
            input=static_cast<UCHAR*>(WdfMemoryGetBuffer(inputMemory,&inputSize));
        }
        if((inputLength && (!input || inputSize<inputLength)) || (outputLength && (!output || outputSize<outputLength))) { status=STATUS_INVALID_BUFFER_SIZE;goto complete; }
        inputSize=inputLength;outputSize=outputLength;
        if(code>=0x510540 && code<=0x51054c && (!context->VendorTestEnabled || context->Stopping || !context->m_pNfcProximityBus)) { status=STATUS_INVALID_DEVICE_STATE;goto complete; }
        switch(code) {
        case 0x310008: {
            if(inputSize<sizeof(ULONG) || !outputSize) { status=STATUS_INVALID_BUFFER_SIZE;break; }
            ULONG property;memcpy(&property,input,sizeof(property));
            if(property==0x181) status=NxpProprietaryGetDeviceCapabilities(context,output,static_cast<USHORT>(outputSize),request);else status=STATUS_NOT_SUPPORTED;
            break;
        }
        case 0x510540: if(!inputSize) { status=STATUS_INVALID_BUFFER_SIZE;break; } status=NfcVendorTestWrite(context,input,static_cast<USHORT>(inputSize),request);break;
        case 0x510544:
            status=outputSize?NfcVendorTestRead(context,output,static_cast<USHORT>(outputSize),request):STATUS_INVALID_PARAMETER;break;
        case 0x510548:
            status=inputLength==1?NfcVendorTestSetFirmwareMode(context,input[0]):STATUS_INVALID_PARAMETER;break;
        case 0x51054c:
            // Pass the actual output buffer; the unimplemented combined
            // exchange reports STATUS_NOT_SUPPORTED.
            status=NfcVendorTestWriteRead(context,input,static_cast<USHORT>(inputSize),output,static_cast<USHORT>(outputSize));break;
        default: status=STATUS_NOT_SUPPORTED;break;
        }
    }
complete:
    if(status!=STATUS_PENDING && request) WdfRequestComplete(request,status);
}
