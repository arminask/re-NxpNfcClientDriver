#include "NfcNxpProprietary.h"
#include "nci_packet.h"
#include "nxp_nci_context.h"
#include "../NxpCoreLib/phNciNfc.h"
#include "../NxpCoreLib/phNciNfc_Extensions.h"
#include "../NxpCoreLib/phNciNfc_RfConfigProp.h"
#include "acpi.h"
#include "nxp_utils.h"
#include <stdlib.h>
#include "nxp_buffer_validation.h"
extern "C" {
#include <phOsalNfc.h>
#include <phOsalNfc_Timer.h>
}
#include "nxp_trace.h"
#include "NfcNxpProprietary.tmh"

NTSTATUS NxpEnablePropExtn(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    return phOsalNfc_NtStatusFromNfcStatus(phNciNfc_EnablePropExtn(context->pNciHandle,NxpEnablePropExtnCB,context));
}
void NxpEnablePropExtnCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpEnableStandbyMode(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    return phOsalNfc_NtStatusFromNfcStatus(phNciNfc_SetPowerMode(context->pNciHandle,phNciNfc_e_EnableStandby,NxpEnableStandbyModeCB,context));
}
void NxpEnableStandbyModeCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpDelayExecution(DEVICE_CONTEXT* context,NTSTATUS,void* duration,void*)
{
    uint32_t timer=phOsalNfc_Timer_Create();
    NFCSTATUS status=NFCSTATUS_INSUFFICIENT_RESOURCES;
    if(timer) {
        status=phOsalNfc_Timer_Start(timer,static_cast<uint32_t>(reinterpret_cast<UINT_PTR>(duration)),NxpDelayExecutionCB,context);
        if(status==NFCSTATUS_SUCCESS) status=NFCSTATUS_PENDING;
        else phOsalNfc_Timer_Delete(timer);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
void NxpDelayExecutionCB(uint32_t timer,void* context)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    (void)phOsalNfc_Timer_Stop(timer);phOsalNfc_Timer_Delete(timer);
    NfcInternalSequence(device,device->pSeqHandler,STATUS_SUCCESS,nullptr,nullptr);
}
NTSTATUS NxpGetEEPromConfigFromRegistry(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    return EEPROM_Process_CustomEEPromConfigBlob(context)?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
}
NTSTATUS NxpSetNFCEEIDConfig(DEVICE_CONTEXT*,NTSTATUS,void*,void*)
{
    // No verified NFCEE-ID configuration command is available. The optional
    // pre-discovery sequence omits this step.
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS NxpSetRatsTc1Config(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    UCHAR payload[]={1,0x5c,1,2};NFCSTATUS status=NFCSTATUS_SUCCESS;
    if((context->ROMVersion==NFCC_ROM_VERSION_PN547 && context->FwVersion==0x122) ||
        static_cast<UINT32>(context->ROMVersion)-17u<2u)
        status=phNciNfc_SetConfigRaw(context->pNciHandle,payload,sizeof(payload),NxpSetRatsTc1ConfigCB,context);
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
void NxpSetRatsTc1ConfigCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
static NTSTATUS FinishRegistryConfig(DEVICE_CONTEXT* device,bool persist)
{
    NTSTATUS status=STATUS_SUCCESS;
    if(persist && device->DriverStore_EEPromParams) {
        if(!DriverStoreRegKey_Set_EEPROMConfigBlob(device,device->DriverStore_EEPromParams,device->RegistryConfigOriginalLength) ||
           !DriverStoreRegKey_Set_KeepRfData(device,1)) status=STATUS_UNSUCCESSFUL;
    }
    free(device->UpdateEEPromUserconfigParams);device->UpdateEEPromUserconfigParams=nullptr;
    free(device->DriverStore_EEPromParams);device->DriverStore_EEPromParams=nullptr;
    device->RegistryConfigOffset=0;device->RegistryConfigRemaining=0;return status;
}
NTSTATUS NxpUpdateDLMASettings(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(context->DlmaCfg.Enable==1) {
        if(static_cast<UINT32>(context->ROMVersion)-17u>=2u) { FinishRegistryConfig(context,false);return STATUS_NOT_SUPPORTED; }
        status=phNciNfc_SetConfigRaw(context->pNciHandle,context->DlmaCfg.buffer,
            static_cast<USHORT>(context->DlmaCfg.buffer[3]+4),NxpDLMARfSettingsCB,context);
        if(status!=NFCSTATUS_PENDING && status!=NFCSTATUS_SUCCESS) FinishRegistryConfig(context,false);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
void NxpDLMARfSettingsCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    device->DlmaCfg.Enable=0;
    NTSTATUS persisted=FinishRegistryConfig(device,NT_SUCCESS(result));
    if(NT_SUCCESS(result)) result=persisted;
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpSetPropRfConfig(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    phNciNfc_RfDiscConfigPropParams_t config={};
    config.tConfigInfo.SystemConfigParams=1;config.tConfigInfo.SwpConfigParams=1;config.tConfigInfo.NxpExtnRfDiscConfig=1;
    config.tSysConfigParams.SystemConfig.EnableConfig=static_cast<UINT32>(context->ROMVersion)-17u<2u?0x800:0x801;
    config.tSysConfigParams.eSysESeIntfSel=phNciNfc_eSeDwpIntf;
    config.tSwpConfigParams.SwpConfig2.EnableConfig2=3;config.tSwpConfigParams.bSwpInt1Cfg=1;config.tSwpConfigParams.bSwpInt2Cfg=1;
    config.tNxpExtnRfDiscParams.NxpExtnRfDiscConf.EnableConfig=0x1b;
    config.tNxpExtnRfDiscParams.bTagDetector=context->TagDetectorCfg.Enable;
    config.tNxpExtnRfDiscParams.bTagDetectorThresh=context->TagDetectorCfg.Threshold;
    config.tNxpExtnRfDiscParams.bTagDetectorFallBackCnt=context->TagDetectorCfg.FallBackCnt;
    config.tNxpExtnRfDiscParams.bPollProfileSel=0;
    return phOsalNfc_NtStatusFromNfcStatus(phNciNfc_SetConfigPropRfParameters(context->pNciHandle,&config,NxpSetPropRfConfigCB,context));
}
void NxpSetPropRfConfigCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS ParseForDLMA(UCHAR* output,size_t capacity,const UCHAR* input,size_t length,DEVICE_CONTEXT* context)
{
    UCHAR dlma[214];size_t written=0;int found=0;
    if(!NxpSplitEepromBlob(input,length,output,capacity,dlma,&written,&found))
        return STATUS_INVALID_PARAMETER;
    memcpy(context->DlmaCfg.buffer,dlma,sizeof(dlma));
    context->DlmaCfg.Enable=static_cast<UCHAR>(found);
    context->DriverStore_EEPromParamsDataSize=static_cast<ULONG>(written-1);
    context->DriverStore_NoEEPromParams=output[0];
    return STATUS_SUCCESS;
}
NTSTATUS NxpUpdateRfSettings(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    auto& rf=context->RfSettings;
    if(!rf.NumOfParams) return rf.BufferIdx==rf.BufferLen?STATUS_SUCCESS:STATUS_INVALID_PARAMETER;
    UCHAR payload[255];size_t used=0,next=0;UCHAR copied=0;
    if(!NxpBuildRfBatch(rf.Buffer,rf.BufferLen,rf.BufferIdx,rf.NumOfParams,payload,&used,&next,&copied))
        return STATUS_INVALID_PARAMETER;
    NTSTATUS status=phOsalNfc_NtStatusFromNfcStatus(phNciNfc_SetConfigRaw(context->pNciHandle,
        payload,static_cast<USHORT>(used),NxpUpdateRfSettingsCB,context));
    if(NT_SUCCESS(status)) { rf.BufferIdx=static_cast<UINT32>(next);rf.NumOfParams-=copied; }
    return status;
}
void NxpUpdateRfSettingsCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    if(NT_SUCCESS(result)) {
        if(device->RfSettings.NumOfParams) (void)NfcRepeatSequence(device,device->pSeqHandler,1);
        else if(memcmp(&device->EEPromWrittenSignature,&device->EEPromSettingSignature,sizeof(GUID))) {
            device->EEPromWrittenSignature=device->EEPromSettingSignature;
            (void)RegistryWritePersistedDeviceParameters(device);
        }
    }
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpGetSwpInterfaceState(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    UCHAR query[]={2,0xa0,0xec,0xa0,0xed};
    return phOsalNfc_NtStatusFromNfcStatus(phNciNfc_GetConfigRaw(context->pNciHandle,query,sizeof(query),NxpGetSwpInterfaceStateCB,context));
}
void NxpGetSwpInterfaceStateCB(void* context,NFCSTATUS status,void* information)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);auto reply=static_cast<UCHAR*>(information);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    if(NT_SUCCESS(result)) {
        const UCHAR *first=nullptr,*second=nullptr;size_t firstLength=0,secondLength=0;
        if(!NxpFindConfigValue(reply,0xec,&first,&firstLength) || firstLength!=1 ||
           !NxpFindConfigValue(reply,0xed,&second,&secondLength) || secondLength!=1 || reply[2]!=2)
            result=STATUS_INVALID_NETWORK_RESPONSE;
        else { device->SwpIntCfg.SwpInt1Cfg=*first==1;device->SwpIntCfg.SwpInt2Cfg=*second==1; }
    }
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpGetNfceeSessionId(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    UCHAR query[5]={},used=1;NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(context->NfceeCtx.NfceeStatus.Uicc || context->NfceeCtx.NfceeStatus.ESE) {
        if(context->SwpIntCfg.SwpInt1Cfg) { ++query[0];query[used++]=0xa0;query[used++]=0xea; }
        if(context->SwpIntCfg.SwpInt2Cfg) { ++query[0];query[used++]=0xa0;query[used++]=0xeb; }
    }
    if(query[0]) status=phNciNfc_GetConfigRaw(context->pNciHandle,query,used,NxpGetNfceeSessionIdCB,context);
    else (void)NfcSkipSequence(context,context->pSeqHandler,1);
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
void NxpGetNfceeSessionIdCB(void* context,NFCSTATUS status,void* information)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);auto reply=static_cast<UCHAR*>(information);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    const UINT64 uninitialized_session=~UINT64(0);bool complete=true;
    if(NT_SUCCESS(result)) {
        const UCHAR* value=nullptr;size_t length=0;
        if(device->SwpIntCfg.SwpInt1Cfg) {
            if(!NxpFindConfigValue(reply,0xea,&value,&length) || length!=8) result=STATUS_INVALID_NETWORK_RESPONSE;
            else if(!memcmp(value,&uninitialized_session,8)) complete=false;
        }
        if(NT_SUCCESS(result) && device->SwpIntCfg.SwpInt2Cfg) {
            if(!NxpFindConfigValue(reply,0xeb,&value,&length) || length!=8) result=STATUS_INVALID_NETWORK_RESPONSE;
            else if(!memcmp(value,&uninitialized_session,8)) complete=false;
        }
    }
    NfcInternalSequence(device,device->pSeqHandler,result,reinterpret_cast<void*>(complete?UINT_PTR(10):UINT_PTR(1000)),nullptr);
}
static UCHAR NxpEEPromUserDataSize(const DEVICE_CONTEXT* device)
{
    return (device->ROMVersion==NFCC_ROM_VERSION_PN547 && device->FwVersion>0x110) ||
        static_cast<UINT32>(device->ROMVersion)-16u<3u?32:64;
}
NTSTATUS NxpGetEEPromUserData(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    UCHAR query[]={1,0xa0,0x0f};
    return phOsalNfc_NtStatusFromNfcStatus(phNciNfc_GetConfigRaw(context->pNciHandle,query,sizeof(query),NxpGetEEPromUserDataCB,context));
}
void NxpGetEEPromUserDataCB(void* context,NFCSTATUS status,void* information)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);auto reply=static_cast<UCHAR*>(information);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    if(status==NFCSTATUS_SUCCESS) {
        const UCHAR size=NxpEEPromUserDataSize(device);
        const UCHAR* value=nullptr;size_t valueLength=0;
        if(!NxpFindConfigValue(reply,0x0f,&value,&valueLength) || valueLength!=size || reply[2]!=1) {
            NfcInternalSequence(device,device->pSeqHandler,STATUS_INVALID_NETWORK_RESPONSE,nullptr,nullptr);return;
        }
        memcpy(&device->EEPromUserData,value,sizeof(device->EEPromUserData));
        if(device->EEPromUserData.MwVersion==0x300 && !device->EEPromUserData.DnldFlag &&
            !device->RfSettings.BufferLen && !device->GetEEPromUserDataSize)
            (void)NfcSkipSequence(device,device->pSeqHandler,5);
        else device->SeqFlag|=1;
    }
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpUpdateEEPromUserData(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    UCHAR payload[68]={1,0xa0,0x0f};const UCHAR size=NxpEEPromUserDataSize(context);
    context->EEPromUserData.DnldFlag=0;context->EEPromUserData.MwVersion=0x300;
    payload[3]=size;memcpy(payload+4,&context->EEPromUserData,sizeof(context->EEPromUserData));
    return phOsalNfc_NtStatusFromNfcStatus(phNciNfc_SetConfigRaw(context->pNciHandle,payload,static_cast<USHORT>(size+4),NxpUpdateEEPromUserDataCB,context));
}
void NxpUpdateEEPromUserDataCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpSetEEPromConfigsRegistry(DEVICE_CONTEXT* context,NTSTATUS,void*,void*)
{
    NTSTATUS result=STATUS_SUCCESS;
    if(!context->UpdateEEPromUserconfigParams) {
        result=Process_EEPromUserData(context);
        if(!NT_SUCCESS(result)) return result;
        const size_t length=context->DriverStore_EEPromParamsDataSize;
        if(!length) return STATUS_SUCCESS;
        context->RegistryConfigOriginalLength=static_cast<ULONG>(length);
        context->UpdateEEPromUserconfigParams=static_cast<UCHAR*>(calloc(length+1,1));
        if(!context->UpdateEEPromUserconfigParams) { FinishRegistryConfig(context,false);return STATUS_INSUFFICIENT_RESOURCES; }
        result=ParseForDLMA(context->UpdateEEPromUserconfigParams,length+1,context->DriverStore_EEPromParams,length,context);
        if(!NT_SUCCESS(result)) { FinishRegistryConfig(context,false);return result; }
        context->RegistryConfigOffset=0;context->RegistryConfigRemaining=context->DriverStore_NoEEPromParams;
    }
    if(!context->RegistryConfigRemaining) {
        return context->DlmaCfg.Enable?STATUS_SUCCESS:FinishRegistryConfig(context,true);
    }
    UCHAR payload[255],copied=0;size_t used=0,next=0;
    if(!NxpBuildRfBatch(context->UpdateEEPromUserconfigParams+1,context->DriverStore_EEPromParamsDataSize,
        context->RegistryConfigOffset,context->RegistryConfigRemaining,payload,&used,&next,&copied)) result=STATUS_INVALID_PARAMETER;
    else {
        result=phOsalNfc_NtStatusFromNfcStatus(phNciNfc_SetConfigRaw(context->pNciHandle,payload,
            static_cast<USHORT>(used),NxpSetEEPromConfigsRegistryCB,context));
        if(NT_SUCCESS(result)) { context->RegistryConfigOffset=static_cast<ULONG>(next);context->RegistryConfigRemaining-=copied; }
    }
    if(result!=STATUS_PENDING) FinishRegistryConfig(context,false);
    return result;
}
void NxpSetEEPromConfigsRegistryCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    if(NT_SUCCESS(result) && device->RegistryConfigRemaining) result=NfcRepeatSequence(device,device->pSeqHandler,1);
    else if(!NT_SUCCESS(result)) FinishRegistryConfig(device,false);
    else if(!device->DlmaCfg.Enable) result=FinishRegistryConfig(device,true);
    // DLMA remains owned until the following sequence step applies it.
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NxpGetEEPromConfig(DEVICE_CONTEXT* context,NTSTATUS status,void*,void*)
{
    const GUID zero={};UCHAR* input=nullptr;ULONG length=0;
    if(memcmp(&context->EEPromSettingSignature,&zero,sizeof(GUID))) return status;
    status=AcpiGetEEPROMConfig(context,&length,&input);
    if(!NT_SUCCESS(status)) return status;
    UCHAR* records=nullptr;UCHAR* split=nullptr;
    do {
        if(length<19 || length>UINT16_MAX) { status=STATUS_INVALID_PARAMETER;break; }
        size_t cursor=16,count=0,recordBytes=0;
        while(cursor<length) {
            if(length-cursor<2 || !input[cursor+1] || input[cursor+1]>251 ||
               size_t(input[cursor+1])+2>length-cursor || ++count>255) { status=STATUS_INVALID_PARAMETER;break; }
            recordBytes+=size_t(input[cursor+1])+3;cursor+=size_t(input[cursor+1])+2;
        }
        if(!NT_SUCCESS(status) || recordBytes>UINT16_MAX) { status=STATUS_INVALID_PARAMETER;break; }
        records=static_cast<UCHAR*>(malloc(recordBytes));
        split=static_cast<UCHAR*>(calloc(recordBytes+1,1));
        if(!records || !split) { status=STATUS_INSUFFICIENT_RESOURCES;break; }
        cursor=16;size_t used=0;
        while(cursor<length) {
            size_t span=size_t(input[cursor+1])+2;
            if(used>=recordBytes || span>recordBytes-used-1 || span>length-cursor) { status=STATUS_INVALID_PARAMETER;break; }
            records[used++]=0xa0;
            if(memcpy_s(records+used,recordBytes-used,input+cursor,span)!=0) { status=STATUS_INVALID_PARAMETER;break; }
            used+=span;cursor+=span;
        }
        if(!NT_SUCCESS(status)) break;
        UCHAR dlma[214];size_t written=0;int hasDlma=0;
        if(!NxpSplitEepromBlob(records,used,split,recordBytes+1,dlma,&written,&hasDlma)) { status=STATUS_INVALID_PARAMETER;break; }
        GUID signature={};status=NfcUtilsReadGuidFromBuffer(16,input,&signature);
        if(!NT_SUCCESS(status)) break;
        if(memcmp(&signature,&context->EEPromWrittenSignature,sizeof(GUID))) {
            for(cursor=0;cursor<used;cursor+=size_t(records[cursor+2])+3) {
                const UCHAR id=records[cursor+1];
                if(records[cursor+2]==1) {
                    if(id==0x40) context->TagDetectorCfg.Enable=records[cursor+3];
                    if(id==0x41) context->TagDetectorCfg.Threshold=records[cursor+3];
                    if(id==0x43) context->TagDetectorCfg.FallBackCnt=records[cursor+3];
                }
            }
            context->DlmaCfg.Enable=static_cast<UCHAR>(hasDlma);memcpy(context->DlmaCfg.buffer,dlma,sizeof(dlma));
            UCHAR params=split[0];memmove(split,split+1,written-1);
            free(context->RfSettings.Buffer);context->RfSettings.Buffer=split;split=nullptr;
            context->RfSettings.BufferLen=static_cast<UINT32>(written-1);
            context->RfSettings.BufferIdx=0;context->RfSettings.NumOfParams=params;
        }
        context->EEPromSettingSignature=signature;
    } while(false);
    free(records);free(split);AcpiFreeEEPROMConfig(context,input);return status;
}

// Original five outgoing-command entries at RVA 0x2e9a0, distinct from RX.
static const _NCI_PACKET_DISPATCH_ENTRY WriteDispatchHandler[]={
    {1,1,16,NxpProcessRfIsoDepNakPresenceChkCmd},
    {1,1,1,NxpProcessRfListenRoutingTable},
    {1,0,9,NxpProcessCoreSetPowerSubStateCmd},
    {1,0,2,NxpProcessCoreSetConfigCmd},
    {1,1,3,NxpProcessRfDiscoverCmd},
};

void NxpPrepareNciPacketForWrite(DEVICE_CONTEXT* context,WDFMEMORY memory)
{
    size_t length=0;auto buffer=static_cast<UCHAR*>(WdfMemoryGetBuffer(memory,&length));
    NCI_PACKET_HEADER header;
    if(length>258 || !NciPacketHeaderGetFromBuffer(buffer,static_cast<UINT>(length),&header) || !header.MessageType) return;
    for(const auto& entry:WriteDispatchHandler) if(entry.MessageType==header.MessageType && entry.Gid==header.Header.ControlPacket.Gid && entry.Oid==header.Header.ControlPacket.Oid) {
        (void)entry.pfnDispatchHandler(context,buffer,length);return;
    }
}

UCHAR NxpProcessCoreSetConfigCmd(DEVICE_CONTEXT* context,UCHAR* buffer,SIZE_T length)
{
    if(context->bDtaFlag && length>=4) {
        UINT32 offset=4;UCHAR processed=0;
        while(processed<buffer[3]) {
            if(length<=static_cast<UINT32>(offset+1) || buffer[offset]==0xa0) break;
            ++processed;
            UINT32 value=offset+2;
            if(buffer[offset]==0x18 && value<length && buffer[value]==1) { buffer[value]=0x80;break; }
            offset+=2+buffer[offset+1];
        }
    }
    return FALSE;
}

UCHAR NxpProcessCoreSetPowerSubStateCmd(DEVICE_CONTEXT*,UCHAR* buffer,SIZE_T length)
{
    if(length>=4) { buffer[1]=(buffer[1]&0xd5)|0x15;buffer[0]|=0x0f;buffer[3]=buffer[3]!=0; }
    return FALSE;
}

UCHAR NxpProcessRfIsoDepNakPresenceChkCmd(DEVICE_CONTEXT*,UCHAR* buffer,SIZE_T length)
{
    if(length>=3) { buffer[0]|=0x0f;buffer[1]=(buffer[1]&0xd1)|0x11; }
    return FALSE;
}

UCHAR NxpProcessRfDiscoverCmd(DEVICE_CONTEXT* context,UCHAR* buffer,SIZE_T length)
{
    if(context->ROMVersion!=NFCC_ROM_VERSION_PN547 && length>=5 &&
        static_cast<SIZE_T>(buffer[3])*2+1<=length-3) {
        size_t offset=1;
        for(unsigned remaining=buffer[3];remaining;--remaining) {
            if(buffer[static_cast<SIZE_T>(offset)+3]==0x77) buffer[static_cast<SIZE_T>(offset)+3]=0x70;
            offset+=2;
        }
    }
    return FALSE;
}

NTSTATUS NxpProprietaryGetDeviceCapabilities(DEVICE_CONTEXT* context,UCHAR* buffer,USHORT length,WDFREQUEST request)
{
    phNciNfc_NfccCapabilities_t nfcc={};IhvNXP_StackCapabilities stack;
    memset(&stack,0,sizeof(stack));
    if(!buffer || length<sizeof(stack)) return STATUS_BUFFER_TOO_SMALL;
    NTSTATUS status=NfcGetControllerCapabilities(context,&nfcc);
    if(!NT_SUCCESS(status)) return status;
    {
        stack.psDevCapabilities.model_id=nfcc.ManufacturerId;stack.psDevCapabilities.hw_version=nfcc.HwVersion;
        stack.psDevCapabilities.fw_version=static_cast<UCHAR>(nfcc.FwVersion);
    }
    memcpy(stack.psDevCapabilities.vendor_name,"NXP",4);
    auto& map=stack.psMappingCapabilities;
    map.MifareUL=map.MifareStd=map.MifareULC=map.ISO14443_4A=map.ISO14443_4B=map.ISO15693=map.FeliCa=map.Jewel=map.Desfire=1;
    if(length<sizeof(stack)) return STATUS_UNSUCCESSFUL;
    memset(buffer,0,length);memcpy(buffer,&stack,sizeof(stack));
    WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(stack));
    return STATUS_PENDING;
}

// Original eleven 16-byte entries at RVA 0x2e9f0. See PROPRIETARY-DISPATCH.csv.
static const _NCI_PACKET_DISPATCH_ENTRY DispatchHandler[]={
    {2,15,17,NxpProcessRfIsoDepNakPresenceChkRsp},
    {3,15,17,NxpProcessRfIsoDepNakPresenceChkNtf},
    {2,15,21,NxpProcessCoreSetPowerSubStateRsp},
    {2,0,3,NxpProcessCoreGetConfigRsp},
    {2,2,0,NxpProcessNfceeMgmtDiscRsp},
    {2,0,1,NxpProcessCoreInitRsp},
    {3,2,0,NxpProcessNfceeMgmtDiscNtf},
    {3,1,2,NxpProcessRfListenRoutingTable},
    {3,1,5,NxpProcessRfIntfActivatedNtf},
    {2,0,0,NxpProcessCoreResetRsp},
    {3,0,0,NxpProcessCoreResetNtf},
};

UCHAR NxpProprietaryNciPacketFilter(DEVICE_CONTEXT* device,WDFMEMORY memory)
{
    size_t length=0;auto buffer=static_cast<UCHAR*>(WdfMemoryGetBuffer(memory,&length));
    if(buffer && length>=3 && length<=258 && length==size_t(buffer[2])+3) {
        NCI_PACKET_HEADER header;
        if(NciPacketHeaderGetFromBuffer(buffer,static_cast<UINT>(length),&header)) {
            if(header.MessageType==0 || header.PBF) return FALSE;
            for(const auto& entry:DispatchHandler) {
                if(entry.MessageType==header.MessageType && entry.Gid==header.Header.ControlPacket.Gid && entry.Oid==header.Header.ControlPacket.Oid)
                    return entry.pfnDispatchHandler(device,buffer,length);
            }
            return header.Header.ControlPacket.Gid==15;
        }
    }
    TRACE_LINE(LEVEL_ERROR,"Invalid proprietary NCI packet");
    return FALSE;
}

UCHAR NxpProcessRfIsoDepNakPresenceChkRsp(DEVICE_CONTEXT*,UCHAR* packet,SIZE_T length)
{
    if(length>=3) { packet[0]=(packet[0]&0xf1)|1;packet[1]=(packet[1]&0xd0)|0x10; }
    return FALSE;
}
UCHAR NxpProcessRfIsoDepNakPresenceChkNtf(DEVICE_CONTEXT*,UCHAR* packet,SIZE_T length)
{
    if(length>=4) {
        packet[0]=(packet[0]&0xf1)|1;packet[1]=(packet[1]&0xd0)|0x10;
        if(packet[3]==0) packet[3]=3;else if(packet[3]==1) packet[3]=0;
    }
    return FALSE;
}
UCHAR NxpProcessCoreSetPowerSubStateRsp(DEVICE_CONTEXT*,UCHAR* packet,SIZE_T length)
{
    if(length>=3) { packet[0]&=0xf0;packet[1]=(packet[1]&0xc9)|9; }
    return FALSE;
}
UCHAR NxpProcessCoreGetConfigRsp(DEVICE_CONTEXT*,UCHAR* packet,SIZE_T length)
{
    if(!packet || length<5) return FALSE;
    size_t cursor=5,count=0;bool proprietary=false;
    while(cursor<length && count<packet[4]) {
        size_t header=packet[cursor]==0xa0?3:2;
        if(header>length-cursor) return FALSE;
        size_t span=header+packet[cursor+header-1];
        if(span>length-cursor) return FALSE;
        if(header==3) proprietary=true;
        cursor+=span;++count;
    }
    return cursor==length && count==packet[4] && proprietary;
}
UCHAR NxpProcessNfceeMgmtDiscRsp(DEVICE_CONTEXT* device,UCHAR* packet,SIZE_T length)
{
    if(length==5 && device->ROMVersion==NFCC_ROM_VERSION_PN547 && packet[3]==0)
        packet[4]=static_cast<UCHAR>(packet[4]+device->SwpIntCfg.SwpInt1Cfg+device->SwpIntCfg.SwpInt2Cfg);
    return FALSE;
}
UCHAR NxpProcessNfceeMgmtDiscNtf(DEVICE_CONTEXT* device,UCHAR* packet,SIZE_T length)
{
    if(length>=5) {
        const unsigned active=packet[4]!=2;
        if(packet[3]==1) device->NfceeCtx.NfceeStatus.HciNwk=active;
        else if(packet[3]==2) device->NfceeCtx.NfceeStatus.Uicc=active;
        else device->NfceeCtx.NfceeStatus.ESE=active;
    }
    return FALSE;
}
UCHAR NxpProcessCoreInitRsp(DEVICE_CONTEXT* device,UCHAR* packet,SIZE_T length)
{
    if(length>=4 && length<=258 && device->Nfcc_Version!=0x20) {
        phNciNfc_Data_t response={packet+3,static_cast<USHORT>(length-3)};
        AcquireSRWLockExclusive(&device->ControllerInfoLock);
        (void)phNciNfc_ProcessInitRsp(device->pNciHandle,&response);
        ReleaseSRWLockExclusive(&device->ControllerInfoLock);
    }
    return FALSE;
}
UCHAR NxpProcessCoreResetNtf(DEVICE_CONTEXT* device,UCHAR* packet,SIZE_T length)
{
    // Notifications without a version byte still pass through to NfcCx.
    if(length>=6 && length-4<0xfffc) {
        phNciNfc_Data_t response={packet+3,static_cast<USHORT>(length-3)};
        device->Nfcc_Version=packet[5];
        AcquireSRWLockExclusive(&device->ControllerInfoLock);
        (void)phNciNfc_ProcessResetNtf(device->pNciHandle,&response);
        ReleaseSRWLockExclusive(&device->ControllerInfoLock);
    }
    return FALSE;
}
UCHAR NxpProcessCoreResetRsp(DEVICE_CONTEXT* device,UCHAR* packet,SIZE_T length)
{
    if(length==6) device->Nfcc_Version=packet[4];
    return FALSE;
}
UCHAR NxpProcessRfIntfActivatedNtf(DEVICE_CONTEXT* device,UCHAR* packet,SIZE_T length)
{
    if(device->ROMVersion!=NFCC_ROM_VERSION_PN547 && length>=11) {
        if(packet[5]==0x81) packet[5]=0x8a;
        if(packet[6]==0x70) packet[6]=0x77;
    }
    return FALSE;
}
UCHAR NxpProcessRfListenRoutingTable(DEVICE_CONTEXT*,UCHAR* packet,SIZE_T length)
{
    if(length>=5) {
        const size_t payloadLength=length-3;
        UCHAR count=0;size_t offset=2;
        while(offset+2<payloadLength) {
            if(count>=packet[4]) break;
            const UCHAR type=packet[offset+3],entryLength=packet[offset+4];
            const size_t next=offset+2+entryLength;
            if(next>payloadLength) break;
            UCHAR* power=nullptr;
            if(type<3 && entryLength>=2) power=packet+offset+6;
            else if(type==3 && entryLength>=1) power=packet+offset+5;
            if(power) {
                UCHAR translated=static_cast<UCHAR>((*power>>2)&0x20);
                if((packet[0]&0xe0)==0x20) translated=static_cast<UCHAR>((*power&0xe0)<<2);
                *power=translated|(*power&7);
            }
            offset=next;++count;
        }
    }
    return FALSE;
}
