#pragma once
#include "device.h"
using FN_NCI_PACKET_DISPATCH_HANDLER=UCHAR (*)(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
struct _NCI_PACKET_DISPATCH_ENTRY {
    UCHAR MessageType,Gid,Oid;
    FN_NCI_PACKET_DISPATCH_HANDLER pfnDispatchHandler;
};
UCHAR NxpProprietaryNciPacketFilter(DEVICE_CONTEXT*,WDFMEMORY);
UCHAR NxpProcessRfIsoDepNakPresenceChkRsp(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessRfIsoDepNakPresenceChkNtf(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessCoreSetPowerSubStateRsp(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessCoreGetConfigRsp(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessNfceeMgmtDiscRsp(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessNfceeMgmtDiscNtf(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessCoreInitRsp(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessCoreResetNtf(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessCoreResetRsp(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessRfIntfActivatedNtf(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessRfListenRoutingTable(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
void NxpPrepareNciPacketForWrite(DEVICE_CONTEXT*,WDFMEMORY);
UCHAR NxpProcessCoreSetConfigCmd(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessCoreSetPowerSubStateCmd(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessRfDiscoverCmd(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
UCHAR NxpProcessRfIsoDepNakPresenceChkCmd(DEVICE_CONTEXT*,UCHAR*,SIZE_T);
NTSTATUS NxpProprietaryGetDeviceCapabilities(DEVICE_CONTEXT*,UCHAR*,USHORT,WDFREQUEST);
void NxpEnablePropExtnCB(void*,NFCSTATUS,void*);
void NxpEnableStandbyModeCB(void*,NFCSTATUS,void*);
void NxpDelayExecutionCB(uint32_t,void*);
void NxpSetRatsTc1ConfigCB(void*,NFCSTATUS,void*);
void NxpDLMARfSettingsCB(void*,NFCSTATUS,void*);
void NxpSetPropRfConfigCB(void*,NFCSTATUS,void*);
NTSTATUS ParseForDLMA(UCHAR*,size_t,const UCHAR*,size_t,DEVICE_CONTEXT*);
void NxpUpdateRfSettingsCB(void*,NFCSTATUS,void*);
void NxpGetSwpInterfaceStateCB(void*,NFCSTATUS,void*);
void NxpGetNfceeSessionIdCB(void*,NFCSTATUS,void*);
void NxpGetEEPromUserDataCB(void*,NFCSTATUS,void*);
void NxpUpdateEEPromUserDataCB(void*,NFCSTATUS,void*);
void NxpSetEEPromConfigsRegistryCB(void*,NFCSTATUS,void*);

struct IhvNfc_sSupProtocol_t {
    unsigned MifareUL:1,MifareStd:1,ISO14443_4A:1,ISO14443_4B:1;
    unsigned ISO15693:1,Felica:1,NFC:1,Jewel:1;
};
struct IhvNfc_sSupportedTagInfo_t {
    unsigned MifareUL:1,MifareStd:1,MifareULC:1,ISO14443_4A:1,ISO14443_4B:1;
    unsigned ISO15693:1,FeliCa:1,Jewel:1,Desfire:1;
};
struct IhvNfc_sDeviceCapabilities_t {
    UCHAR hal_version,fw_version,hw_version,model_id,hci_version;
    UCHAR vendor_name[20],full_version[11];
    IhvNfc_sSupProtocol_t ReaderSupProtocol,EmulationSupProtocol;
    UCHAR firmware_update_info;
};
struct IhvNXP_StackCapabilities {
    IhvNfc_sDeviceCapabilities_t psDevCapabilities;
    IhvNfc_sSupportedTagInfo_t psMappingCapabilities,psFormatCapabilities;
};
