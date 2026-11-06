#include "../NxpCoreLib/phNciNfc.h"
extern "C" {
#include <phOsalNfc.h>
}
#include "device.h"
#include <stdlib.h>
#include "nxp_nci_context.h"
#include "nxp_trace.h"
#include "handler.tmh"

// Sequence steps resolve in their original firmware/proprietary compilation
// units. Unrecovered steps have no production substitute implementations.
struct _SEQUENCE_HANDLER_DISPATCH_ENTRY {
    NFC_CX_SEQUENCE Sequence;
    NTSTATUS (*pfnDispatchHandler)(DEVICE_CONTEXT*);
};
// Original table at RVA 0x2e820, ten 16-byte records. Values/pointers are recorded
// independently in docs/evidence/NFC-DISPATCH.csv.
static const _SEQUENCE_HANDLER_DISPATCH_ENTRY g_NfcDispatch[] = {
    {SequencePreInit,NfcPreInitSequenceHandler},
    {SequenceInitComplete,NfcInitCompleteSequenceHandler},
    {SequencePreRfDiscStart,NfcPreRfDiscStartSequenceHandler},
    {SequenceRfDiscStartComplete,NfcRfDiscStartCompleteSequenceHandler},
    {SequencePreRfDiscStop,NfcPreRfDiscStopSequenceHandler},
    {SequenceRfDiscStopComplete,NfcRfDiscStopCompleteSequenceHandler},
    {SequencePreNfceeDisc,NfcPreNfceeDiscSequenceHandler},
    {SequenceNfceeDiscComplete,NfcNfceeDiscCompleteSequenceHandler},
    {SequencePreShutdown,NfcPreShutdownSequenceHandler},
    {SequenceShutdownComplete,NfcShutdownCompleteSequenceHandler},
};

NTSTATUS NfcExecuteSequence(DEVICE_CONTEXT* device,NFC_CX_SEQUENCE sequence,
    PFN_NFC_CX_SEQUENCE_COMPLETION_ROUTINE completion,void* context)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    NTSTATUS status=STATUS_SUCCESS;
    WdfWaitLockAcquire(device->SeqWaitLock,nullptr);
    if(device->SeqContext.CompletionRoutine) {
        TRACE_LINE(LEVEL_ERROR,"A sequence is already in progress..");
        status=STATUS_INVALID_DEVICE_REQUEST;
        WdfWaitLockRelease(device->SeqWaitLock);
    } else {
        WdfWaitLockRelease(device->SeqWaitLock);
        device->SeqFlag=0;
        for(const auto& dispatch:g_NfcDispatch) {
            if(dispatch.Sequence==sequence) { status=dispatch.pfnDispatchHandler(device);break; }
        }
        if(status==STATUS_PENDING) {
            WdfWaitLockAcquire(device->SeqWaitLock,nullptr);
            device->SeqContext.CompletionRoutine=completion;
            device->SeqContext.CompletionContext=context;
            WdfWaitLockRelease(device->SeqWaitLock);
        } else {
            completion(device->FxDevice,status,device->SeqFlag,context);
        }
    }
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

UCHAR NfcIsSequenceInProgress(DEVICE_CONTEXT* device)
{
    WdfWaitLockAcquire(device->SeqWaitLock,nullptr);
    const bool pending=device->SeqContext.CompletionRoutine!=nullptr;
    WdfWaitLockRelease(device->SeqWaitLock);
    return pending;
}

void NfcSequenceComplete(DEVICE_CONTEXT* device,NTSTATUS status)
{
    WdfWaitLockAcquire(device->SeqWaitLock,nullptr);
    auto completion=device->SeqContext.CompletionRoutine;
    void* context=device->SeqContext.CompletionContext;
    if(completion) {
        device->SeqContext.CompletionRoutine=nullptr;
        device->SeqContext.CompletionContext=nullptr;
    }
    WdfWaitLockRelease(device->SeqWaitLock);
    // Clear ownership and release the lock before invoking external code.
    if(completion) completion(device->FxDevice,status,device->SeqFlag,context);
}

void NfcEvtSequenceHandler(WDFDEVICE fxDevice,NFC_CX_SEQUENCE sequence,
    PFN_NFC_CX_SEQUENCE_COMPLETION_ROUTINE completion,void* context)
{
    DEVICE_CONTEXT* device=GetDeviceContext(fxDevice);
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    if(completion) {
        if(device->Stopping) { completion(fxDevice,STATUS_CANCELLED,0,context);return; }
        NfcPostMessage(device,0,static_cast<UINT_PTR>(static_cast<LONG_PTR>(sequence)),
            reinterpret_cast<UINT_PTR>(completion),reinterpret_cast<UINT_PTR>(context),0);
    } else {
        TRACE_LINE(LEVEL_ERROR,"CompletionRoutine should not be NULL");
    }
    TRACE_FUNCTION_EXIT(LEVEL_VERBOSE);
}

// The reference bodies at 0x6920/0x6cf0 perform only tracing and return success.
// These are observed no-op callbacks, not invented successful implementations.
NTSTATUS NfcRfDiscStartCompleteSequenceHandler(DEVICE_CONTEXT*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS NfcRfDiscStopCompleteSequenceHandler(DEVICE_CONTEXT* device)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    device->DiscStopTimestamp=GetTickCount64();
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS NfcShutdownCompleteSequenceHandler(DEVICE_CONTEXT*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS NfcPreRfDiscStartSequenceHandler(DEVICE_CONTEXT* device)
{
    const ULONGLONG elapsed=GetTickCount64()-device->DiscStopTimestamp;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    // The original sleeps a full 25 ms, rather than the remaining interval.
    if(elapsed<25) Sleep(25);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS NfcPreRfDiscStopSequenceHandler(DEVICE_CONTEXT*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS NfcPreShutdownSequenceHandler(DEVICE_CONTEXT*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS NfcInitializeSeqComplete(DEVICE_CONTEXT* device,NTSTATUS status,void*,void*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    if(device->RfSettings.Buffer) free(device->RfSettings.Buffer);
    // Zero the whole PDB structure, including padding, as in the original.
    memset(&device->RfSettings,0,sizeof(device->RfSettings));
    device->DeviceMode=device->PreviousDeviceMode;
    device->EEPromFlashRequired=FALSE;
    NfcSequenceComplete(device,status);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcNFCEESetSeqComplete(DEVICE_CONTEXT* device,NTSTATUS status,void*,void*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    device->DeviceMode=device->PreviousDeviceMode;
    NfcSequenceComplete(device,status);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcNfceeDiscCompleteSeqComplete(DEVICE_CONTEXT* device,NTSTATUS status,void*,void*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    // This callback observes NFCEE mode; restoration follows completion.
    NfcSequenceComplete(device,status);
    device->DeviceMode=device->PreviousDeviceMode;
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcRegisterSequenceHandlers(DEVICE_CONTEXT* device)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    NTSTATUS status=STATUS_SUCCESS;
    for(const auto& dispatch:g_NfcDispatch) {
        status=NfcCxRegisterSequenceHandler(device->FxDevice,dispatch.Sequence,NfcEvtSequenceHandler);
        if(!NT_SUCCESS(status)) {
            TRACE_LINE(LEVEL_ERROR,"Failed to register sequence handler");
            break;
        }
    }
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcPreInitSequenceHandler(DEVICE_CONTEXT* device)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    NTSTATUS status=STATUS_SUCCESS;
    if(device->FWDownloadRequired) {
        static SEQUENCE FWDownloadSequence[]={
            {NfcFWDnldSetup,nullptr},
            {NfcFWDnldGetSessionState,nullptr},
            {NfcFWDnldGetVersion,nullptr},
            {NfcFWDnldReadEEPromUserData,nullptr},
            {NfcFWDnldWrite,nullptr},
            {NfcFWDnldGetSessionState,nullptr},
            {NfcFWDnldGetVersion,nullptr},
            {NfcFWDnldUpdateEEPromUserData,nullptr},
            {NfcFWDnldCheckIntegrity,nullptr},
            {nullptr,nullptr}
        };
        FWDownloadSequence[9].SequenceProcess=NfcFWDownloadSeqComplete;
        device->pSeqHandler=FWDownloadSequence;
        device->SeqNext=0;
        device->SeqMax=NfcGetSequenceLength(FWDownloadSequence);
        status=NfcSequenceHandler(device,STATUS_SUCCESS,nullptr,nullptr);
    }
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcInitCompleteSequenceHandler(DEVICE_CONTEXT* device)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    phNciNfc_NfccCapabilities_t caps={};
    NTSTATUS capabilityStatus=NfcGetControllerCapabilities(device,&caps);
    if(!NT_SUCCESS(capabilityStatus)) return capabilityStatus;
    if(caps.RomVersion!=8 && (caps.RomVersion<16 || caps.RomVersion>18)) return STATUS_NOT_SUPPORTED;
    device->HWVersion=caps.HwVersion;device->ROMVersion=static_cast<_NFCC_ROM_VERSION>(caps.RomVersion);
    device->FwVersion=caps.FwVersion;
    device->FWDownloadRequired=NfcFWCheckFWDownload(device);
    NTSTATUS status=STATUS_SUCCESS;
    if(device->FWDownloadRequired) {
        device->SeqFlag|=1;
    } else if(device->EEPromFlashRequired) {
        static SEQUENCE InitializeSequence[]={
            {NxpGetEEPromConfig,nullptr},{NxpEnablePropExtn,nullptr},
            {NxpGetEEPromUserData,nullptr},{NxpUpdateRfSettings,nullptr},
            {NxpUpdateDLMASettings,nullptr},{NxpSetPropRfConfig,nullptr},
            {NxpSetRatsTc1Config,nullptr},{NxpEnableStandbyMode,nullptr},
            {NxpUpdateEEPromUserData,nullptr},{NxpGetSwpInterfaceState,nullptr},
            {nullptr,nullptr}
        };
        static SEQUENCE InitializeRegSequence[]={
            {NxpGetEEPromConfigFromRegistry,nullptr},{NxpEnablePropExtn,nullptr},
            {NxpGetEEPromUserData,nullptr},{NxpSetEEPromConfigsRegistry,nullptr},
            {NxpUpdateDLMASettings,nullptr},{NxpSetRatsTc1Config,nullptr},
            {NxpEnableStandbyMode,nullptr},{NxpUpdateEEPromUserData,nullptr},
            {NxpGetSwpInterfaceState,nullptr},{nullptr,nullptr}
        };
        NfcIsACPIAvailable(device);
        device->PreviousDeviceMode=device->DeviceMode;
        device->DeviceMode=DEVICE_MODE_NFCEE_SESSIONID;
        if(device->bIsACPIAvailableFlag==TRUE) {
            InitializeSequence[10].SequenceProcess=NfcInitializeSeqComplete;
            device->pSeqHandler=InitializeSequence;
        } else {
            InitializeRegSequence[9].SequenceProcess=NfcInitializeSeqComplete;
            device->pSeqHandler=InitializeRegSequence;
        }
        device->SeqNext=0;
        device->SeqMax=NfcGetSequenceLength(device->pSeqHandler);
        status=NfcSequenceHandler(device,STATUS_SUCCESS,nullptr,nullptr);
    }
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcPreNfceeDiscSequenceHandler(DEVICE_CONTEXT* device)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    memset(&device->NfceeCtx.NfceeStatus,0,sizeof(device->NfceeCtx.NfceeStatus));
    device->PreviousDeviceMode=device->DeviceMode;
    device->DeviceMode=DEVICE_MODE_NFCEE_SESSIONID;
    static SEQUENCE NFCEESetSessionIDSequence[]={
        {nullptr,nullptr}
    };
    NFCEESetSessionIDSequence[0].SequenceProcess=NfcNFCEESetSeqComplete;
    device->pSeqHandler=NFCEESetSessionIDSequence;
    device->SeqNext=0;
    device->SeqMax=NfcGetSequenceLength(NFCEESetSessionIDSequence);
    const NTSTATUS status=NfcSequenceHandler(device,STATUS_SUCCESS,nullptr,nullptr);
    // This write follows synchronous callbacks, not the initial mode change.
    device->SeqFlag=0;
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcNfceeDiscCompleteSequenceHandler(DEVICE_CONTEXT* device)
{
    device->PreviousDeviceMode=device->DeviceMode;
    device->DeviceMode=DEVICE_MODE_NFCEE_SESSIONID;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    static SEQUENCE NfceeDiscCompleteSequence[]={
        {NxpGetSwpInterfaceState,nullptr},{NxpGetNfceeSessionId,nullptr},
        {NxpDelayExecution,nullptr},{nullptr,nullptr}
    };
    NfceeDiscCompleteSequence[3].SequenceProcess=NfcNfceeDiscCompleteSeqComplete;
    device->pSeqHandler=NfceeDiscCompleteSequence;
    device->SeqNext=0;
    device->SeqMax=NfcGetSequenceLength(NfceeDiscCompleteSequence);
    const NTSTATUS status=NfcSequenceHandler(device,STATUS_SUCCESS,nullptr,nullptr);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}

NTSTATUS NfcFWDownloadSeqComplete(DEVICE_CONTEXT* device,NTSTATUS status,void*,void*)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    auto& fw=device->FWDnldContext;
    if(device->FirmwareLogPending) {
        device->FirmwareLogPending=FALSE;
        if(!NT_SUCCESS(fw.DnldStatus)) status=fw.DnldStatus;
    }
    if(fw.RetryDnld && fw.DnldAttempts>=3) { fw.RetryDnld=FALSE;status=STATUS_IO_DEVICE_ERROR; }
    if(!NT_SUCCESS(status)) {
        if(fw.DnldAttempts<3 && device->FirmwareRecoveryCycles<3) {
            fw.DnldRecovery=TRUE;
            if(status==STATUS_DEVICE_BUSY) fw.SkipForce=TRUE;
        } else {
            fw.DnldRecovery=FALSE;
            fw.RetryDnld=FALSE;
        }
    }
    if(fw.DnldInitiated) {
        static SEQUENCE FWDownloadCompleteSequence[]={
            {NfcFWDnldUpdateEEPromUserData,nullptr},{nullptr,nullptr}
        };
        FWDownloadCompleteSequence[1].SequenceProcess=NfcFWDownloadSeqComplete;
        fw.DnldStatus=status;device->FirmwareLogPending=TRUE;
        fw.DnldInitiated=TRUE;
        device->pSeqHandler=FWDownloadCompleteSequence;
    } else if(fw.DnldRecovery) {
        ++device->FirmwareRecoveryCycles;
        static SEQUENCE FWDownloadRecoverySequence[]={
            {NfcFWDnldReset,nullptr},{NfcFWDnldForce,nullptr},
            {NfcFWDnldRecovery,nullptr},{nullptr,nullptr}
        };
        FWDownloadRecoverySequence[3].SequenceProcess=NfcFWDownloadSeqComplete;
        device->pSeqHandler=FWDownloadRecoverySequence;
    } else if(fw.RetryDnld) {
        static SEQUENCE FWDownloadRetrySequence[]={
            {NfcFWDnldGetSessionState,nullptr},{NfcFWDnldGetVersion,nullptr},
            {NfcFWDnldReadEEPromUserData,nullptr},{NfcFWDnldWrite,nullptr},
            {NfcFWDnldGetSessionState,nullptr},{NfcFWDnldGetVersion,nullptr},
            {NfcFWDnldUpdateEEPromUserData,nullptr},{NfcFWDnldCheckIntegrity,nullptr},
            {nullptr,nullptr}
        };
        fw.PrevSessionOpen=FALSE;
        fw.DnldInitiated=FALSE;
        fw.DnldRecovery=FALSE;
        fw.ForceDnld=FALSE;
        fw.SkipSequence=FALSE;
        fw.SkipForce=FALSE;
        FWDownloadRetrySequence[8].SequenceProcess=NfcFWDownloadSeqComplete;
        device->pSeqHandler=FWDownloadRetrySequence;
    } else {
        NfcFWFreeFullFWImage(device);
        NTSTATUS modeStatus=NfcFWSetMode(device,FALSE);
        if(NT_SUCCESS(status)) status=modeStatus;
        if(fw.DnldAttemptFailed) status=fw.DnldStatus;
        if(NT_SUCCESS(status)) {
            memset(&device->EEPromWrittenSignature,0,sizeof(GUID));
            memset(&device->EEPromSettingSignature,0,sizeof(GUID));
            (void)RegistryWritePersistedDeviceParameters(device);
            device->SeqFlag|=2;
        }
        memset(&fw,0,sizeof(fw));
        device->FWDownloadRequired=FALSE;
        AcquireSRWLockExclusive(&device->ControllerInfoLock);
        NFCSTATUS initialized=phNciNfc_Initialise(device,TRUE,&device->pNciHandle);
        ReleaseSRWLockExclusive(&device->ControllerInfoLock);
        if(initialized!=NFCSTATUS_SUCCESS && NT_SUCCESS(status)) status=phOsalNfc_NtStatusFromNfcStatus(initialized);
        NfcSequenceComplete(device,status);
        TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
        return status;
    }
    device->SeqNext=0;
    device->SeqMax=NfcGetSequenceLength(device->pSeqHandler);
    status=NfcSequenceHandler(device,STATUS_SUCCESS,nullptr,nullptr);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status;
}
