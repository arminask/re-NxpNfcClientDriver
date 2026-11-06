#include "driver.h"
#include "nxp_trace.h"
// Retain DLL attach/detach tracing, using the WDK's user-mode compatibility
// branch rather than moving initialization to DriverEntry in this port.
#undef WPP_MACRO_USE_KM_VERSION_FOR_UM
#include "driver.tmh"

// Reference image RVA 0x2e6b0, with the exact PDB public symbol attribution.
const GUID GUID_NFC_VENDOR_TEST_INTERFACE={0x1a492896,0xcd06,0x4bad,{0xb9,0x4e,0xe9,0x08,0xfa,0x75,0x40,0x22}};

extern "C" BOOL WINAPI DllMain(HINSTANCE,DWORD reason,void*)
{
    if(reason==DLL_PROCESS_ATTACH) { WPP_INIT_TRACING(L"Microsoft\\Nfc\\NfcI2C"); }
    else if(reason==DLL_PROCESS_DETACH) { WPP_CLEANUP(); }
    return TRUE;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config,OnDeviceAdd);
    config.EvtDriverUnload=OnDriverUnload;
    config.DriverPoolTag=0x4963664e; // Original little-endian 'NfcI' tag.
    return WdfDriverCreate(driver,registryPath,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}

void OnDriverUnload(WDFDRIVER) { }

NTSTATUS OnDeviceAdd(WDFDRIVER,PWDFDEVICE_INIT deviceInit)
{
    WdfDeviceInitSetPowerPolicyOwnership(deviceInit,FALSE);
    WDF_PNPPOWER_EVENT_CALLBACKS callbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);
    callbacks.EvtDevicePrepareHardware=OnPrepareHardware;
    callbacks.EvtDeviceReleaseHardware=OnReleaseHardware;
    callbacks.EvtDeviceD0Entry=OnD0Entry;callbacks.EvtDeviceD0Exit=OnD0Exit;
    callbacks.EvtDeviceD0EntryPostInterruptsEnabled=OnD0EntryPostInterruptsEnabled;
    callbacks.EvtDeviceD0ExitPreInterruptsDisabled=OnD0ExitPreInterruptsDisabled;
    callbacks.EvtDeviceSelfManagedIoCleanup=OnSelfManagedIoCleanup;
    callbacks.EvtDeviceSurpriseRemoval=OnDeviceSurpriseRemoval;
    WdfDeviceInitSetPnpPowerEventCallbacks(deviceInit,&callbacks);

    NFC_CX_CLIENT_CONFIG client;
    NFC_CX_CLIENT_CONFIG_INIT(&client,NFC_CX_TRANSPORT_CUSTOM);
    client.IsPowerPolicyOwner=WdfFalse;client.DriverFlags=0x392;
    client.EvtNfcCxWriteNciPacket=NfcEvtWriteNciPacket;
    client.EvtNfcCxDeviceIoControl=NfcEvtDeviceIoControl;
    NTSTATUS status=RegistryGetDeviceInitSettings(deviceInit,&client);
    if(!NT_SUCCESS(status)) return status;
    status=NfcCxDeviceInitConfig(deviceInit,&client);
    if(!NT_SUCCESS(status)) return status;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,DEVICE_CONTEXT);
    WDFDEVICE device;
    status=WdfDeviceCreate(&deviceInit,&attributes,&device);
    if(!NT_SUCCESS(status)) return status;
    DEVICE_CONTEXT* context=GetDeviceContext(device);
    InitializeSRWLock(&context->ControllerInfoLock);
    context->FxDevice=device;context->DeviceMode=DEVICE_MODE_NCI;
    context->FirmwareMode=FIRMWARE_MODE_RAW;context->DeviceState=DEVICE_STATE_WORKING;
    context->TotalDuration=300;context->PollConfig=0x7f;
    context->NfcIPMode=0x7f;context->NfcIPTgtMode=0x0f;context->NfcCEMode=7;
    context->Miu=1024;context->LinkTimeout=100;context->RecvWindowSize=5;
    context->FWSkipDownload=FALSE;context->FWForceDownload=FALSE;
    context->TagDetectorCfg={1,2,4};context->EEPromFlashRequired=TRUE;
    context->bIsACPIAvailableFlag=FALSE;context->bDtaFlag=client.DeviceMode==NFC_CX_DEVICE_MODE_DTA;
    status=RegistryGetDeviceSettings(context);
    if(!NT_SUCCESS(status)) return status;
    status=RegistryReadPersistedDeviceParameters(context);
    if(!NT_SUCCESS(status)) return status;
    (void)CustomRegKey_Get_I2CAddress(context);

    WDF_DEVICE_STATE state;
    WDF_DEVICE_STATE_INIT(&state);
    state.NotDisableable=WdfFalse;
    WdfDeviceSetDeviceState(device,&state);
    context->ReadList.Flink=context->ReadList.Blink=&context->ReadList;
    context->GetEEPromUserDataList.Flink=context->GetEEPromUserDataList.Blink=&context->GetEEPromUserDataList;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=context->FxDevice;
    status=WdfWaitLockCreate(&attributes,&context->SeqWaitLock);
    if(!NT_SUCCESS(status)) return status;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=context->FxDevice;
    status=WdfWaitLockCreate(&attributes,&context->WorkItemLock);
    if(!NT_SUCCESS(status)) return status;
    status=NfcCxDeviceInitialize(device);
    if(!NT_SUCCESS(status)) return status;

    if(client.DeviceMode==NFC_CX_DEVICE_MODE_NCI) {
        NFC_CX_RF_DISCOVERY_CONFIG discovery;
        NFC_CX_RF_DISCOVERY_CONFIG_INIT(&discovery);
        discovery.TotalDuration=context->TotalDuration;discovery.PollConfig=context->PollConfig;
        discovery.NfcIPMode=context->NfcIPMode;discovery.NfcIPTgtMode=context->NfcIPTgtMode;discovery.NfcCEMode=context->NfcCEMode;
        status=NfcCxSetRfDiscoveryConfig(device,&discovery);
        if(!NT_SUCCESS(status)) return status;
        NFC_CX_LLCP_CONFIG llcp;
        NFC_CX_LLCP_CONFIG_INIT(&llcp);
        llcp.Miu=context->Miu;llcp.LinkTimeout=context->LinkTimeout;llcp.RecvWindowSize=context->RecvWindowSize;
        status=NfcCxSetLlcpConfig(device,&llcp);
        if(!NT_SUCCESS(status)) return status;
        return NfcRegisterSequenceHandlers(context);
    }
    if(client.DeviceMode==NFC_CX_DEVICE_MODE_RAW) {
        context->VendorTestContext.ReadNotificationQueue.Flink=context->VendorTestContext.ReadNotificationQueue.Blink=&context->VendorTestContext.ReadNotificationQueue;
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=context->FxDevice;
        status=WdfWaitLockCreate(&attributes,&context->VendorTestContext.ReadQueueLock);
        if(!NT_SUCCESS(status)) return status;
        // Original initializes only Size and DispatchType; PowerManaged is false.
        WDF_IO_QUEUE_CONFIG queue={};queue.Size=sizeof(queue);queue.DispatchType=WdfIoQueueDispatchManual;
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=context->FxDevice;
        status=WdfIoQueueCreate(context->FxDevice,&queue,&attributes,&context->VendorTestContext.ReadQueue);
        if(!NT_SUCCESS(status)) return status;
        status=WdfDeviceCreateDeviceInterface(context->FxDevice,&GUID_NFC_VENDOR_TEST_INTERFACE,nullptr);
        if(!NT_SUCCESS(status)) return status;
        context->VendorTestEnabled=TRUE;context->DeviceMode=DEVICE_MODE_NFCEE_SESSIONID;
    }
    return status;
}
