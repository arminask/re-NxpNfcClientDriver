#pragma once
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <wdf.h>
#include <nfccx.h>
#include <phNfcTypes.h>
extern "C" {
#include <phTmlNfc.h>
}

// Layout and enum values recovered from the exact PDB. System handle types use
// WDK declarations. See docs/evidence/DEVICE-TYPES.json and device_layout.h.
class INfcProximityBus;
struct _DEVICE_CONTEXT;
using SEQUENCE_CALLBACK = NTSTATUS (*)(_DEVICE_CONTEXT*,NTSTATUS,void*,void*);
struct _SEQUENCE {
    SEQUENCE_CALLBACK SequenceInitiate;
    SEQUENCE_CALLBACK SequenceProcess;
};
using SEQUENCE = _SEQUENCE;

enum _DEVICE_MODE {
    DEVICE_MODE_NCI=0, DEVICE_MODE_FIRMWARE=1, DEVICE_MODE_EEPROM=2,
    DEVICE_MODE_NFCEE_SESSIONID=3, DEVICE_MODE_CLIENT_CONTROL=4
};
enum _FIRMWARE_MODE { FIRMWARE_MODE_RAW=0, FIRMWARE_MODE_NCI=1 };
enum _DEVICE_STATE { DEVICE_STATE_WORKING=0, DEVICE_STATE_UPDATING_MODE=1 };
enum _TRANSPORT_PLATFORM {
    TRANSPORT_SPBI2C=0, TRANSPORT_USBI2C=1, TRANSPORT_Abend=2,
    TRANSPORT_HECI=3, TRANSPORT_USBI2CSPI=4, TRANSPORT_LPCUSBI2C=5
};
enum _NFCC_ROM_VERSION {
    NFCC_ROM_VERSION_PN547=8, NFCC_ROM_VERSION_PN548=16,
    NFCC_ROM_VERSION_PN553=17, NFCC_ROM_VERSION_PN557=18
};
#ifndef NXP_DOWNLOAD_ROM_VERSION_DEFINED
#define NXP_DOWNLOAD_ROM_VERSION_DEFINED
enum _phDnldNfc_NFCC_ROM_VERSION {
    phDnldNfc_NFCC_ROM_Version_PN547=8, phDnldNfc_NFCC_ROM_Version_PN548=16,
    phDnldNfc_NFCC_ROM_Version_PN553=17, phDnldNfc_NFCC_ROM_Version_PN557=18
};
#endif
struct _DH_EEPROM_USER_DATA {
    USHORT MwVersion;
    USHORT FwVersion;
    USHORT NumOfDnldAttempted;
    USHORT NumOfDnldSuccess;
    USHORT NumOfDnldFailed;
    USHORT DnldFailCount;
    UCHAR DnldFlag;
};
struct _FW_DOWNLOAD_CONTEXT {
    UCHAR SkipSequence;
    UCHAR SkipReset;
    UCHAR SkipForce;
    UCHAR PrevSessionOpen;
    UCHAR DnldInitiated;
    UCHAR DnldRecovery;
    UCHAR ForceDnld;
    UCHAR RetryDnld;
    UCHAR DnldAttemptFailed;
    UCHAR DnldAttempts;
    UCHAR HWVersion;
    _phDnldNfc_NFCC_ROM_VERSION NFCCROMVersion;
    _DH_EEPROM_USER_DATA EEPromUserData;
    NTSTATUS DnldStatus;
};
struct _RF_SETTINGS { UCHAR NumOfParams; UINT32 BufferIdx; UINT32 BufferLen; UCHAR* Buffer; };
struct _TAG_DETECTOR_SETTINGS { UCHAR Enable; UCHAR Threshold; UCHAR FallBackCnt; };
struct _SWP_INTERFACE_CONFIG { UCHAR SwpInt1Cfg; UCHAR SwpInt2Cfg; };
struct _NFCEE_CONTEXT {
    struct { unsigned int HciNwk:1; unsigned int Uicc:1; unsigned int ESE:1; } NfceeStatus;
};
struct _DYNAMIC_LMA_SETTINGS { UCHAR Enable; UCHAR buffer[214]; };
struct _SEQUENCE_CONTEXT {
    PFN_NFC_CX_SEQUENCE_COMPLETION_ROUTINE CompletionRoutine;
    void* CompletionContext;
};
using TML_COMPLETION = void (*)(void*,phTmlNfc_TransactInfo_t*);
struct _TML_CONTEXT {
    struct { TML_COMPLETION pTmlCompletion; void* pContext; HANDLE hEvent; } Write;
    struct {
        UCHAR* pBuffer;
        USHORT wLength;
        TML_COMPLETION pTmlCompletion;
        void* pContext;
        HANDLE hEvent;
        LIST_ENTRY Queue;
    } Read;
};
struct _VENDOR_TEST_CONTEXT {
    WDFQUEUE ReadQueue;
    LIST_ENTRY ReadNotificationQueue;
    WDFWAITLOCK ReadQueueLock;
};
struct _DEVICE_CONTEXT {
    WDFDEVICE FxDevice;
    _DEVICE_MODE DeviceMode;
    _DEVICE_MODE PreviousDeviceMode;
    _FIRMWARE_MODE FirmwareMode;
    UCHAR DsmFunctionsSupported;
    _DEVICE_STATE DeviceState;
    WDFINTERRUPT ReadInterrupt;
    WDFWAITLOCK WorkItemLock;
    LIST_ENTRY ReadList;
    USHORT TotalDuration;
    ULONG PollConfig;
    UCHAR NfcIPMode;
    UCHAR NfcIPTgtMode;
    UCHAR NfcCEMode;
    USHORT Miu;
    UCHAR LinkTimeout;
    UCHAR RecvWindowSize;
    GUID EEPromWrittenSignature;
    GUID EEPromSettingSignature;
    ULONG HWFailedAttempts;
    UCHAR FWDownloadRequired;
    UCHAR FWCheckComplete;
    UCHAR FWSkipDownload;
    UCHAR FWForceDownload;
    USHORT FWVersion;
    WDFSTRING FirmwareFileName;
    phNfc_sData_t FirmwareImage;
    _FW_DOWNLOAD_CONTEXT FWDnldContext;
    void* pNciHandle;
    _RF_SETTINGS RfSettings;
    _DH_EEPROM_USER_DATA EEPromUserData;
    _TAG_DETECTOR_SETTINGS TagDetectorCfg;
    _SWP_INTERFACE_CONFIG SwpIntCfg;
    _NFCEE_CONTEXT NfceeCtx;
    UINT64 DiscStopTimestamp;
    _DYNAMIC_LMA_SETTINGS DlmaCfg;
    ULONG MsgQueueThreadId;
    SEQUENCE* pSeqHandler;
    UCHAR bSeqHandler;
    UCHAR SeqNext;
    UCHAR SeqMax;
    _SEQUENCE_CONTEXT SeqContext;
    WDFWAITLOCK SeqWaitLock;
    ULONG SeqFlag;
    _TML_CONTEXT TmlContext;
    _TRANSPORT_PLATFORM TransportPlatfrom; // Original spelling in PDB.
    UCHAR VendorTestEnabled;
    _VENDOR_TEST_CONTEXT VendorTestContext;
    UCHAR bDtaFlag;
    UCHAR bIsACPIAvailableFlag;
    UCHAR HWVersion;
    _NFCC_ROM_VERSION ROMVersion;
    USHORT FwVersion;
    UCHAR EEPromFlashRequired;
    UCHAR* DriverStore_EEPromParams;
    UCHAR DriverStore_NoEEPromParams;
    ULONG DriverStore_EEPromParamsDataSize;
    UCHAR* UpdateEEPromUserconfigParams;
    UCHAR* GetEEPromUserDataConfigParams;
    ULONG GetEEPromUserDataSize;
    UCHAR GetEEPromUserDataNoConfigsParams;
    LIST_ENTRY GetEEPromUserDataList;
    INfcProximityBus* m_pNfcProximityBus;
    const WCHAR* HardwareId;
    UCHAR I2CAddress;
    HANDLE StartThreadEvent;
    UCHAR bCxHardwareEvent;
    UCHAR Nfcc_Version;
    // Local lifecycle state appended to the recovered private layout.
    volatile LONG Stopping;
    BOOLEAN CoreInitialized;
    ULONG TmlWriteGeneration;
    UCHAR FirmwareRecoveryCycles;
    BOOLEAN FirmwareLogPending;
    ULONG RegistryConfigOffset;
    ULONG RegistryConfigOriginalLength;
    UCHAR RegistryConfigRemaining;
    void* TmlWriteCookie;
    volatile LONG TmlReadGeneration;
    SRWLOCK ControllerInfoLock;
};
using DEVICE_CONTEXT = _DEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext);

struct phNciNfc_NfccCapabilities;
NTSTATUS NfcGetControllerCapabilities(DEVICE_CONTEXT*,phNciNfc_NfccCapabilities*);

UCHAR NfcGetSequenceLength(SEQUENCE* sequence);
void NfcInternalSequence(DEVICE_CONTEXT*,SEQUENCE*,NTSTATUS,void*,void*);
NTSTATUS NfcRepeatSequence(DEVICE_CONTEXT*,SEQUENCE*,UCHAR);
NTSTATUS NfcSkipSequence(DEVICE_CONTEXT*,SEQUENCE*,UCHAR);
NTSTATUS NfcSequenceHandler(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
void NfcPostMessage(void*,UINT32,UINT_PTR,UINT_PTR,UINT_PTR,UINT_PTR);
void NfcMessageHandler(void*,UINT32,UINT_PTR,UINT_PTR,UINT_PTR,UINT_PTR);
NTSTATUS NfcExecuteSequence(DEVICE_CONTEXT*,NFC_CX_SEQUENCE,PFN_NFC_CX_SEQUENCE_COMPLETION_ROUTINE,void*);
UCHAR EEProm_Parse_ConfigBlob(DEVICE_CONTEXT*,UCHAR*,ULONG);
UCHAR EEPROM_Process_CustomEEPromConfigBlob(DEVICE_CONTEXT*);
UCHAR CustomRegKey_Get_CustomKeepRfData(DEVICE_CONTEXT*,ULONG*);
UCHAR CustomRegKey_IsExist_CustomEEPROMConfigBlob(DEVICE_CONTEXT*);
UCHAR CustomRegKey_Get_CustomEEPROMConfigBlobLength(DEVICE_CONTEXT*,ULONG*);
UCHAR CustomRegKey_Get_CustomEEPROMConfigBlob(DEVICE_CONTEXT*,UCHAR*,ULONG*);
UCHAR CustomRegKey_Get_I2CAddress(DEVICE_CONTEXT*);
UCHAR CustomRegKey_Set_CustomKeepRfData(DEVICE_CONTEXT*,ULONG);
UCHAR DriverStoreRegKey_Get_KeepRfData(DEVICE_CONTEXT*,ULONG*);
UCHAR DriverStoreRegKey_Set_KeepRfData(DEVICE_CONTEXT*,ULONG);
NTSTATUS Process_EEPromUserData(DEVICE_CONTEXT*);
NTSTATUS RegistryGetDeviceInitSettings(PWDFDEVICE_INIT,NFC_CX_CLIENT_CONFIG*);
NTSTATUS RegistryGetDeviceSettings(DEVICE_CONTEXT*);
NTSTATUS RegistryReadPersistedDeviceParameters(DEVICE_CONTEXT*);
UCHAR DriverStoreRegKey_Set_EEPROMConfigBlob(DEVICE_CONTEXT*,UCHAR*,ULONG);
UCHAR NfcIsSequenceInProgress(DEVICE_CONTEXT*);
void NfcSequenceComplete(DEVICE_CONTEXT*,NTSTATUS);
EVT_NFC_CX_SEQUENCE_HANDLER NfcEvtSequenceHandler;
NTSTATUS NfcRfDiscStartCompleteSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcRfDiscStopCompleteSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcShutdownCompleteSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcPreInitSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcInitCompleteSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcPreRfDiscStartSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcPreRfDiscStopSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcPreNfceeDiscSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcNfceeDiscCompleteSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcPreShutdownSequenceHandler(DEVICE_CONTEXT*);
NTSTATUS NfcInitializeSeqComplete(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcNFCEESetSeqComplete(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcNfceeDiscCompleteSeqComplete(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcRegisterSequenceHandlers(DEVICE_CONTEXT*);
NTSTATUS NfcFWDownloadSeqComplete(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
void NfcFWFreeFullFWImage(DEVICE_CONTEXT*);
NTSTATUS NfcFWReadFullFWImage(DEVICE_CONTEXT*);
NTSTATUS NfcFWGetVersion(DEVICE_CONTEXT*,ULONG);
NTSTATUS NfcFWSetMode(DEVICE_CONTEXT*,UCHAR);
UCHAR NfcFWCheckFWDownload(DEVICE_CONTEXT*);
NTSTATUS RegistryWritePersistedDeviceParameters(DEVICE_CONTEXT*);
extern "C" void NfcIsACPIAvailable(DEVICE_CONTEXT*);

// Sequence-step definitions remain in their original firmware/proprietary units.
NTSTATUS NfcFWDnldSetup(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldGetSessionState(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldGetVersion(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldReadEEPromUserData(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldWrite(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldUpdateEEPromUserData(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldCheckIntegrity(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldReset(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldForce(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NfcFWDnldRecovery(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpGetEEPromConfig(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpEnablePropExtn(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpGetEEPromUserData(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpUpdateRfSettings(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpUpdateDLMASettings(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpSetPropRfConfig(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpSetRatsTc1Config(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpEnableStandbyMode(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpUpdateEEPromUserData(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpGetSwpInterfaceState(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpGetEEPromConfigFromRegistry(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpSetEEPromConfigsRegistry(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpSetNFCEEIDConfig(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpGetNfceeSessionId(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
NTSTATUS NxpDelayExecution(DEVICE_CONTEXT*,NTSTATUS,void*,void*);
