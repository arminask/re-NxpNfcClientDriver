#include "phNciNfc_RfConfigProp.h"
#include <stdlib.h>
#include <string.h>
#include "nxp_trace.h"
#include "phNciNfc_RfConfigProp.tmh"

static phNciNfc_SequenceParams_t gphNciNfc_SetConfigRawSequence[]={
    {phNciNfc_SetConfigProp,phNciNfc_SetConfigPropRsp},{NULL,phNciNfc_CompleteSetConfigProp}
};
static phNciNfc_SequenceParams_t gphNciNfc_GetConfigRawSequence[]={
    {phNciNfc_GetConfigProp,phNciNfc_GetConfigRawRsp},{NULL,phNciNfc_CompleteGetConfigRaw}
};
static phNciNfc_SequenceParams_t gphNciNfc_GetConfigPropSequence[]={
    {phNciNfc_GetConfigProp,phNciNfc_GetConfigPropRsp},{NULL,phNciNfc_CompleteGetConfigProp}
};
static phNciNfc_SequenceParams_t gphNciNfc_SetConfigPropSequence[]={
    {phNciNfc_GetConfigPropOpt,phNciNfc_GetConfigPropOptRsp},
    {phNciNfc_SetConfigProp,phNciNfc_SetConfigPropRsp},{NULL,phNciNfc_CompleteSetConfigProp}
};

// Published C subsets are included here to retain the original NXP C filename
// and translation unit. These aliases use the recovered private client ABI.
typedef phNciNfc_Context_t* pphNciNfc_Context_t;
typedef void (*pphNciNfc_IfNotificationCb_t)(void*,NFCSTATUS,void*);
#define phOsalNfc_GetMemory malloc
#define phOsalNfc_FreeMemory free
#define phOsalNfc_MemCopy memcpy
#define phOsalNfc_SetMemory memset
#define PHNCINFC_INIT_SEQUENCE(CONTEXT,SEQUENCE) \
    (CONTEXT)->pSeqHandler=(SEQUENCE); \
    (CONTEXT)->SeqNext=0; \
    (CONTEXT)->SeqMax=phNciNfc_getSequenceLength(SEQUENCE)
#define gphNciNfc_SetConfigSequence gphNciNfc_SetConfigRawSequence
#define phNciNfc_GetConfig phNciNfc_GetConfigProp
#include "../../../../third_party/microsoft-nfc/fragments/phNciNfc_RawApi.inc"
#include "../../../../third_party/microsoft-nfc/fragments/phNciNfc_RawRfConfig.inc"

NFCSTATUS phNciNfc_SetConfigProp(void* context)
{
    phNciNfc_Context_t* nci=context;
    phNciNfc_CoreTxInfo_t command={0};uint8_t count=0;uint16_t bytes=0;NFCSTATUS status;
    if(!nci) return NFCSTATUS_FAILED;
    if(nci->tSetConfPropInfo.pSetConfParams) {
        uint16_t size;uint8_t* payload;
        if(!nci->tSetConfPropInfo.pSetConfParams->tConfigInfo.SystemConfigParams &&
           !nci->tSetConfPropInfo.pSetConfParams->tConfigInfo.SwpConfigParams &&
           !nci->tSetConfPropInfo.pSetConfParams->tConfigInfo.NxpExtnRfDiscConfig &&
           !nci->tSetConfPropInfo.pSetConfParams->tConfigInfo.PropConfig) {
            (void)phNciNfc_SkipSequenceSeq(nci,nci->pSeqHandler,1);return NFCSTATUS_SUCCESS;
        }
        status=phNciNfc_ValidateSetConfPropParams(nci->tSetConfPropInfo.pSetConfParams,&bytes,&count);
        if(status!=NFCSTATUS_SUCCESS) return status;
        if(!count) {
            (void)phNciNfc_SkipSequenceSeq(nci,nci->pSeqHandler,1);return NFCSTATUS_SUCCESS;
        }
        size=(uint16_t)((uint16_t)count*2+bytes+1);
        if(size>255) return NFCSTATUS_BUFFER_TOO_SMALL;
        payload=calloc(size,1);
        if(!payload) return NFCSTATUS_FAILED;
        payload[0]=count;
        status=phNciNfc_BuildSetConfPropPayload(nci->tSetConfPropInfo.pSetConfParams,payload,size);
        if(status!=NFCSTATUS_SUCCESS) {
            free(payload);return status;
        }
        nci->tSendPayload.pBuff=payload;nci->tSendPayload.wPayloadSize=size;
    }
    command.Buff=nci->tSendPayload.pBuff;command.wLen=nci->tSendPayload.wPayloadSize;
    command.tHeaderInfo.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlCmd;
    command.tHeaderInfo.Group_ID=phNciNfc_e_CoreNciCoreGid;command.tHeaderInfo.Opcode_ID.Val=2;
    return phNciNfc_CoreIfTxRx(&nci->NciCoreContext,&command,&nci->RspBuffInfo,2000,phNciNfc_GenericSequence,nci);
}

NFCSTATUS phNciNfc_SetConfigPropRsp(void* context,NFCSTATUS ignored)
{
    phNciNfc_Context_t* nci=context;
    if(ignored!=NFCSTATUS_SUCCESS) return ignored;
    if(!nci) return NFCSTATUS_INVALID_PARAMETER;
    return nci->RspBuffInfo.wLen>1 && nci->RspBuffInfo.pBuff && nci->RspBuffInfo.pBuff[0]==0?NFCSTATUS_SUCCESS:NFCSTATUS_FAILED;
}

NFCSTATUS phNciNfc_CompleteSetConfigProp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(nci) {
        phNciNfc_ClearConfigParams(nci);
        if(nci->tSendPayload.pBuff) { free(nci->tSendPayload.pBuff);nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0; }
        if(nci->tSetConfPropInfo.pSetConfParams) { free(nci->tSetConfPropInfo.pSetConfParams);nci->tSetConfPropInfo.pSetConfParams=NULL; }
        if(nci->tSetConfPropInfo.pGetCfgParams) { free(nci->tSetConfPropInfo.pGetCfgParams);nci->tSetConfPropInfo.pGetCfgParams=NULL; }
        phNciNfc_Notify(nci,status,NULL);
    }
    return status;
}

// The raw-path fixture replaces these calls above; the actual parameter
// algorithms below remain independently callable in the native test target.
#undef phNciNfc_ValidateSetConfPropParams
#undef phNciNfc_BuildSetConfPropPayload

// Count increments precede most validation checks. Failed parameters therefore
// contribute a count but no bytes; both caller-owned counters wrap naturally.
#define COUNT_PARAMETER() (*count=(uint8_t)(*count+1))
#define ADD_PARAMETER_BYTES(n) (*bytes=(uint16_t)(*bytes+(n)))

NFCSTATUS phNciNfc_ValidateSysConfigParams(phNciNfc_SysConfigParams_t* config,uint16_t* bytes,uint8_t* count)
{
    const uint32_t mask=config->SystemConfig.EnableConfig;
    if(!mask || (mask&~0x3fffu)) return NFCSTATUS_INVALID_PARAMETER;
    if(mask&1) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(3); }
    if(mask&2) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&4) { COUNT_PARAMETER();if(config->bClkReqCfg>2) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x10) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&8) {
        COUNT_PARAMETER();
        if(config->ClkSelCfg.ClkSrc==phNciNfc_InvalidClkSrc || (int32_t)config->ClkSelCfg.ClkIn>5) return NFCSTATUS_INVALID_PARAMETER;
        ADD_PARAMETER_BYTES(2);
    }
    if(mask&0x20) { COUNT_PARAMETER();if(config->bIrqPolarityCfg>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x40) { COUNT_PARAMETER();if(config->bVbatMonitorEnCfg>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x80) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&0x100) { COUNT_PARAMETER();if(config->bPwrByFieldEnCfg>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x200) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(3); }
    if(mask&0x400) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(5); }
    if(mask&0x800) {
        // This particular enum is checked before its count is incremented.
        if(config->eSysESeIntfSel!=phNciNfc_eSeSwp2Intf && config->eSysESeIntfSel!=phNciNfc_eSeDwpIntf) return NFCSTATUS_INVALID_PARAMETER;
        COUNT_PARAMETER();ADD_PARAMETER_BYTES(2);
    }
    if(mask&0x1000) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&0x2000) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(7); }
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_ValidateSwpConfigParams(phNciNfc_SwpConfig_t* config,uint16_t* bytes,uint8_t* count)
{
    if((config->SwpConfig.EnableConfig&~1u) || (config->SwpConfig2.EnableConfig2&~7u)) return NFCSTATUS_FEATURE_NOT_SUPPORTED;
    if(!config->SwpConfig.EnableConfig && !config->SwpConfig2.EnableConfig2) return NFCSTATUS_INVALID_PARAMETER;
    if(config->SwpConfig.EnableConfig&1) {
        COUNT_PARAMETER();
        if((uint8_t)(config->bSwpBitRateInt1-1)>6) return NFCSTATUS_INVALID_PARAMETER;
        ADD_PARAMETER_BYTES(2);
    }
    if(config->SwpConfig2.EnableConfig2&1) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(config->SwpConfig2.EnableConfig2&2) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(config->SwpConfig2.EnableConfig2&4) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_ValidateNxpExtnRfDiscConfigParams(phNciNfc_NxpExtnRfDiscConfig_t* config,uint16_t* bytes,uint8_t* count)
{
    const uint32_t mask=config->NxpExtnRfDiscConf.EnableConfig;
    unsigned key;
    if(mask&~0x3fffu) return NFCSTATUS_FEATURE_NOT_SUPPORTED;
    if(!mask && !config->ConfigMifKeys.ConfigKey) return NFCSTATUS_INVALID_PARAMETER;
    // No verified serializer exists for these historical selector bits.
    if(mask&0x1c0) return NFCSTATUS_FEATURE_NOT_SUPPORTED;
    if(mask&1) { COUNT_PARAMETER();if(config->bTagDetector>0x83) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&2) { COUNT_PARAMETER();if(config->bTagDetectorThresh>15) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&4) { COUNT_PARAMETER();if((uint16_t)(config->wTagDetectorPeriod-0x20)>0x7b0) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(3); }
    if(mask&8) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&0x10) { COUNT_PARAMETER();if(config->bPollProfileSel>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x20) { COUNT_PARAMETER();if(config->bP15693BailOut>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x40) { COUNT_PARAMETER();if(config->bPHidBailOut>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x80) { COUNT_PARAMETER();if(config->bEpcGenBailOut>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x100) { COUNT_PARAMETER();if(config->bKovioBailOut>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x200) { COUNT_PARAMETER();if(!config->bGtNfcAACfg) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x400) { COUNT_PARAMETER();if(!config->bGtNfcAPCfg) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x800) { COUNT_PARAMETER();if(!config->bGtNfcBCfg) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x1000) { COUNT_PARAMETER();if(!config->bGtNfcFCfg) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    if(mask&0x2000) { COUNT_PARAMETER();if(!config->bGtNfc15693Cfg) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    for(key=0;key<16;++key) if(config->ConfigMifKeys.ConfigKey&(1u<<key)) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(7); }
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_ValidatePropConfigParams(phNciNfc_PropDiscParams_t* config,uint16_t* bytes,uint8_t* count)
{
    const uint32_t mask=config->PropConfig.EnableConfig;
    if(mask&~0xfu) return NFCSTATUS_FEATURE_NOT_SUPPORTED;
    if(!mask) return NFCSTATUS_INVALID_PARAMETER;
    if(mask&1) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&2) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(3); }
    if(mask&4) { COUNT_PARAMETER();ADD_PARAMETER_BYTES(2); }
    if(mask&8) { COUNT_PARAMETER();if(config->bLstnProfileSel>1) return NFCSTATUS_INVALID_PARAMETER;ADD_PARAMETER_BYTES(2); }
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_ValidateSetConfPropParams(phNciNfc_RfDiscConfigPropParams_t* config,uint16_t* bytes,uint8_t* count)
{
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(!config || !bytes || !count) return NFCSTATUS_INVALID_PARAMETER;
    if(!config->tConfigInfo.SystemConfigParams && !config->tConfigInfo.SwpConfigParams && !config->tConfigInfo.NxpExtnRfDiscConfig && !config->tConfigInfo.PropConfig) return NFCSTATUS_INVALID_PARAMETER;
    if(config->tConfigInfo.SystemConfigParams) { status=phNciNfc_ValidateSysConfigParams(&config->tSysConfigParams,bytes,count);if(status!=NFCSTATUS_SUCCESS) return status; }
    if(config->tConfigInfo.SwpConfigParams) { status=phNciNfc_ValidateSwpConfigParams(&config->tSwpConfigParams,bytes,count);if(status!=NFCSTATUS_SUCCESS) return status; }
    if(config->tConfigInfo.NxpExtnRfDiscConfig) { status=phNciNfc_ValidateNxpExtnRfDiscConfigParams(&config->tNxpExtnRfDiscParams,bytes,count);if(status!=NFCSTATUS_SUCCESS) return status; }
    if(config->tConfigInfo.PropConfig) status=phNciNfc_ValidatePropConfigParams(&config->tPropDiscParams,bytes,count);
    return status;
}

#undef COUNT_PARAMETER
#undef ADD_PARAMETER_BYTES

// All proprietary tags have ID1 a0; length includes the one-byte ID2. Each
// builder returns a boolean failure, rather than the detailed TLV status.
#define APPEND_PARAMETER(mask,bit,id,length,data) \
    if((mask)&(bit)) { if(phNciNfc_TlvUtilsAddTlv(tlv,0xa0,length,(uint8_t*)(data),id)) return 1; }

uint8_t phNciNfc_BuildSysConfigParams(phNciNfc_TlvUtilInfo_t* tlv,phNciNfc_SysConfigParams_t* config)
{
    const uint32_t mask=config->SystemConfig.EnableConfig;
    uint8_t notification=(uint8_t)(config->SystemNtfCfg.Info_Txldo_OverCurrent |
        config->SystemNtfCfg.Info_Pmuvcc<<1 | config->SystemNtfCfg.Info_Mem_Violation<<2 |
        config->SystemNtfCfg.Info_Temp_OverHeat<<3 | config->SystemNtfCfg.Info_Svdd_OverCurr<<4);
    uint8_t clock=(uint8_t)(((uint8_t)config->ClkSelCfg.ClkSrc<<3)|(uint8_t)config->ClkSelCfg.ClkIn);
    uint8_t ven=(uint8_t)(config->VenCfg.VenValue | config->VenCfg.VenPullDown*2);
    APPEND_PARAMETER(mask,1,0x00,3,&config->wHostIfCharCfg);
    APPEND_PARAMETER(mask,2,0x01,2,&notification);
    APPEND_PARAMETER(mask,4,0x02,2,&config->bClkReqCfg);
    APPEND_PARAMETER(mask,0x10,0x04,2,&config->bClkTimeOutCfg);
    APPEND_PARAMETER(mask,8,0x03,2,&clock);
    APPEND_PARAMETER(mask,0x20,0x05,2,&config->bIrqPolarityCfg);
    APPEND_PARAMETER(mask,0x40,0x06,2,&config->bVbatMonitorEnCfg);
    APPEND_PARAMETER(mask,0x80,0x07,2,&ven);
    APPEND_PARAMETER(mask,0x100,0x08,2,&config->bPwrByFieldEnCfg);
    APPEND_PARAMETER(mask,0x200,0x09,3,&config->bToBeforeStandByCfg);
    APPEND_PARAMETER(mask,0x400,0x11,5,&config->PllXtalCfg);
    APPEND_PARAMETER(mask,0x800,0x12,2,&config->eSysESeIntfSel);
    APPEND_PARAMETER(mask,0x1000,0x5f,2,&config->bFelicaTSN);
    APPEND_PARAMETER(mask,0x2000,0x0d,7,config->aRfRegCfg);
    return 0;
}

uint8_t phNciNfc_BuildSwpConfigParams(phNciNfc_TlvUtilInfo_t* tlv,phNciNfc_SwpConfig_t* config)
{
    APPEND_PARAMETER(config->SwpConfig.EnableConfig,1,0xc0,2,&config->bSwpBitRateInt1);
    APPEND_PARAMETER(config->SwpConfig2.EnableConfig2,1,0xec,2,&config->bSwpInt1Cfg);
    APPEND_PARAMETER(config->SwpConfig2.EnableConfig2,2,0xed,2,&config->bSwpInt2Cfg);
    APPEND_PARAMETER(config->SwpConfig2.EnableConfig2,4,0xee,2,&config->bSwpSvddOn);
    return 0;
}

uint8_t phNciNfc_BuildConfigPropParams(phNciNfc_TlvUtilInfo_t* tlv,phNciNfc_PropDiscParams_t* config)
{
    const uint32_t mask=config->PropConfig.EnableConfig;
    APPEND_PARAMETER(mask,1,0x4b,2,&config->bFelicaReqCode);
    APPEND_PARAMETER(mask,2,0x4c,3,&config->wFelicaSysCode);
    APPEND_PARAMETER(mask,4,0x80,2,&config->bToRfOff);
    APPEND_PARAMETER(mask,8,0x81,2,&config->bLstnProfileSel);
    return 0;
}

uint8_t phNciNfc_BuildNxpRfDiscConfigParams(phNciNfc_TlvUtilInfo_t* tlv,phNciNfc_NxpExtnRfDiscConfig_t* config)
{
    const uint32_t mask=config->NxpExtnRfDiscConf.EnableConfig;
    uint8_t status=0;unsigned index;
    struct {uint32_t bit;uint8_t id,length;uint8_t* data;} detector[]={
        {1,0x40,2,&config->bTagDetector},{2,0x41,2,&config->bTagDetectorThresh},
        {4,0x42,3,(uint8_t*)&config->wTagDetectorPeriod},
        {8,0x43,2,&config->bTagDetectorFallBackCnt},{0x10,0x44,2,&config->bPollProfileSel}
    };
    uint8_t* keys[]={config->aMifKey0Cfg,config->aMifKey1Cfg,config->aMifKey2Cfg,config->aMifKey3Cfg,
        config->aMifKey4Cfg,config->aMifKey5Cfg,config->aMifKey6Cfg,config->aMifKey7Cfg,
        config->aMifKey8Cfg,config->aMifKey9Cfg,config->aMifKey10Cfg,config->aMifKey11Cfg,
        config->aMifKey12Cfg,config->aMifKey13Cfg,config->aMifKey14Cfg,config->aMifKey15Cfg};
    for(index=0;index<sizeof(detector)/sizeof(detector[0]);++index) if(mask&detector[index].bit) {
        status=phNciNfc_TlvUtilsAddTlv(tlv,0xa0,detector[index].length,detector[index].data,detector[index].id);
        if(status) break;
    }
    // The original does not guard this call with the previous result. A
    // successful15693 bailout write can mask an earlier detector write failure.
    if(status) return 1;
    if(mask&0x20) status=phNciNfc_TlvUtilsAddTlv(tlv,0xa0,2,&config->bP15693BailOut,0x45);
    if(status) return 1;
    // The three other validated bailout bits6..8 are never serialized here.
    APPEND_PARAMETER(mask,0x200,0x46,2,&config->bGtNfcAACfg);
    APPEND_PARAMETER(mask,0x400,0x47,2,&config->bGtNfcAPCfg);
    APPEND_PARAMETER(mask,0x800,0x48,2,&config->bGtNfcBCfg);
    APPEND_PARAMETER(mask,0x1000,0x49,2,&config->bGtNfcFCfg);
    APPEND_PARAMETER(mask,0x2000,0x4a,2,&config->bGtNfc15693Cfg);
    for(index=0;index<16;++index) if(config->ConfigMifKeys.ConfigKey&(1u<<index)) {
        if(phNciNfc_TlvUtilsAddTlv(tlv,0xa0,7,keys[index],(uint8_t)(0x4d+index))) return 1;
    }
    return 0;
}

NFCSTATUS phNciNfc_BuildSetConfPropCmdPayload(phNciNfc_RfDiscConfigPropParams_t* config,phNciNfc_TlvUtilInfo_t* tlv)
{
    if(config->tConfigInfo.SystemConfigParams && phNciNfc_BuildSysConfigParams(tlv,&config->tSysConfigParams)) return 1;
    if(config->tConfigInfo.SwpConfigParams && phNciNfc_BuildSwpConfigParams(tlv,&config->tSwpConfigParams)) return 1;
    if(config->tConfigInfo.NxpExtnRfDiscConfig && phNciNfc_BuildNxpRfDiscConfigParams(tlv,&config->tNxpExtnRfDiscParams)) return 1;
    if(config->tConfigInfo.PropConfig) return phNciNfc_BuildConfigPropParams(tlv,&config->tPropDiscParams);
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_BuildSetConfPropPayload(phNciNfc_RfDiscConfigPropParams_t* config,uint8_t* payload,uint16_t size)
{
    phNciNfc_TlvUtilInfo_t tlv={0};
    NFCSTATUS status;
    if(!config || !payload) return NFCSTATUS_INVALID_PARAMETER;
    tlv.pBuffer=payload;tlv.dwLength=size;tlv.sizeInfo.dwOffset=1;
    status=phNciNfc_BuildSetConfPropCmdPayload(config,&tlv);
    if(status) return status;
    return tlv.sizeInfo.dwpacketSize==size?NFCSTATUS_SUCCESS:NFCSTATUS_INVALID_PARAMETER;
}
#undef APPEND_PARAMETER

NFCSTATUS phNciNfc_ClearConfigParams(void* context)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return NFCSTATUS_FAILED;
    nci->tRfConfContext.bReqParamNum=0;
    if(nci->tSendPayload.pBuff==nci->tRfConfContext.pReqParamList) {
        nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0;
    }
    if(nci->tRfConfContext.pReqParamList) free(nci->tRfConfContext.pReqParamList);
    nci->tRfConfContext.pReqParamList=NULL;
    nci->tRfConfContext.bReqParamLen=0;
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_GetRfPropParams(phNciNfc_RfDiscConfigPropParams_t* config,uint8_t* ids,uint8_t* count,uint8_t* length)
{
    uint8_t used=0,parameters=0;unsigned index;
    static const uint8_t system_ids[]={0,1,2,3,4,5,6,7,8,9,0xff,0x12,0x5f};
    static const uint8_t discovery_ids[]={0x40,0x41,0x42,0x43,0x44,0x45,0xff,0xff,0xff,0x46,0x47,0x48,0x49,0x4a};
    static const uint8_t property_ids[]={0x4b,0x4c,0x80,0x81};
    if(!ids || !count || !length) return NFCSTATUS_INVALID_PARAMETER;
#define REQUEST_ID(id) do { ids[used++]=0xa0;ids[used++]=(id);++parameters; } while(0)
    if(config->tConfigInfo.SystemConfigParams) for(index=0;index<sizeof(system_ids);++index)
        if(system_ids[index]!=0xff && (config->tSysConfigParams.SystemConfig.EnableConfig&(1u<<index))) REQUEST_ID(system_ids[index]);
    if(config->tConfigInfo.SwpConfigParams) {
        if(config->tSwpConfigParams.SwpConfig.EnableConfig&1) REQUEST_ID(0xc0);
        if(config->tSwpConfigParams.SwpConfig2.EnableConfig2&1) REQUEST_ID(0xec);
        if(config->tSwpConfigParams.SwpConfig2.EnableConfig2&2) REQUEST_ID(0xed);
        if(config->tSwpConfigParams.SwpConfig2.EnableConfig2&4) REQUEST_ID(0xee);
    }
    if(config->tConfigInfo.NxpExtnRfDiscConfig) for(index=0;index<sizeof(discovery_ids);++index)
        if(discovery_ids[index]!=0xff && (config->tNxpExtnRfDiscParams.NxpExtnRfDiscConf.EnableConfig&(1u<<index))) REQUEST_ID(discovery_ids[index]);
    if(config->tConfigInfo.PropConfig) for(index=0;index<sizeof(property_ids);++index)
        if(config->tPropDiscParams.PropConfig.EnableConfig&(1u<<index)) REQUEST_ID(property_ids[index]);
    *count=parameters;*length=used;
#undef REQUEST_ID
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_StoreRfPropParams(uint8_t* parameter,uint16_t length,phNciNfc_RfDiscConfigPropParams_t* config)
{
    NFCSTATUS status=NFCSTATUS_INVALID_PARAMETER;
    if(!parameter || !config || length<3 || parameter[0]!=0xa0 ||
       (size_t)parameter[2]+3!=length || !parameter[2]) return status;
    // These values are serialized as two bytes; require both before copying.
    if((parameter[1]==0 || parameter[1]==9 || parameter[1]==0x42 || parameter[1]==0x4c) &&
       parameter[2]!=2) return status;
    if(parameter[1]==0x0d && parameter[2]>sizeof(config->tSysConfigParams.aRfRegCfg)) return status;
#define STORE_PARAMETER(id,group,mask,bit,field,value) \
    if(parameter[1]==(id) && parameter[2]) { config->group.mask|=(bit); \
        config->group.field=(value);status=NFCSTATUS_SUCCESS; }
    if(parameter[1]==0 && parameter[2]) {
        config->tSysConfigParams.SystemConfig.EnableConfig|=1;config->tConfigInfo.SystemConfigParams=1;
        memcpy(&config->tSysConfigParams.wHostIfCharCfg,parameter+3,2);status=NFCSTATUS_SUCCESS;
    }
    if(parameter[1]==1 && parameter[2]) {
        config->tSysConfigParams.SystemNtfCfg.Info_Txldo_OverCurrent=parameter[3]&1;
        config->tSysConfigParams.SystemNtfCfg.Info_Pmuvcc=(parameter[3]>>1)&1;
        config->tSysConfigParams.SystemNtfCfg.Info_Mem_Violation=(parameter[3]>>2)&1;
        config->tSysConfigParams.SystemNtfCfg.Info_Temp_OverHeat=(parameter[3]>>3)&1;
        config->tSysConfigParams.SystemNtfCfg.Info_Svdd_OverCurr=(parameter[3]>>4)&1;
        config->tSysConfigParams.SystemConfig.EnableConfig|=2;config->tConfigInfo.SystemConfigParams=1;status=NFCSTATUS_SUCCESS;
    }
#define STORE_SYSTEM(id,bit,field) \
    STORE_PARAMETER(id,tSysConfigParams,SystemConfig.EnableConfig,bit,field,parameter[3]); \
    if(parameter[1]==(id) && parameter[2]) config->tConfigInfo.SystemConfigParams=1
    STORE_SYSTEM(2,4,bClkReqCfg);
    if(parameter[1]==3 && parameter[2]) {
        if(parameter[3]==8) config->tSysConfigParams.ClkSelCfg.ClkSrc=phNciNfc_Xtal;
        else if(parameter[3]>=0x10 && parameter[3]<=0x15) {
            config->tSysConfigParams.ClkSelCfg.ClkSrc=phNciNfc_Pll;
            config->tSysConfigParams.ClkSelCfg.ClkIn=(phNciNfc_SysClkFreq_t)(parameter[3]-0x10);
        }
        config->tSysConfigParams.SystemConfig.EnableConfig|=8;config->tConfigInfo.SystemConfigParams=1;status=NFCSTATUS_SUCCESS;
    }
    STORE_SYSTEM(4,0x10,bClkTimeOutCfg);
    STORE_SYSTEM(5,0x20,bIrqPolarityCfg);
    STORE_SYSTEM(6,0x40,bVbatMonitorEnCfg);
    if(parameter[1]==7 && parameter[2]) {
        if(parameter[3]&1) config->tSysConfigParams.VenCfg.VenValue=1;
        if(parameter[3]&2) config->tSysConfigParams.VenCfg.VenPullDown=1;
        config->tSysConfigParams.SystemConfig.EnableConfig|=0x80;config->tConfigInfo.SystemConfigParams=1;status=NFCSTATUS_SUCCESS;
    }
    STORE_SYSTEM(8,0x100,bPwrByFieldEnCfg);
    if(parameter[1]==9 && parameter[2]) {
        memcpy(&config->tSysConfigParams.bToBeforeStandByCfg,parameter+3,2);
        config->tSysConfigParams.SystemConfig.EnableConfig|=0x200;config->tConfigInfo.SystemConfigParams=1;status=NFCSTATUS_SUCCESS;
    }
    if(parameter[1]==0x0d && parameter[2] && parameter[2]<7) {
        memcpy(config->tSysConfigParams.aRfRegCfg,parameter+3,parameter[2]);
        config->tSysConfigParams.SystemConfig.EnableConfig|=0x2000;config->tConfigInfo.SystemConfigParams=1;status=NFCSTATUS_SUCCESS;
    }
    if(parameter[1]==0x12 && parameter[2]) {
        config->tSysConfigParams.eSysESeIntfSel=(phNciNfc_eSeIntfSel_t)parameter[3];
        config->tSysConfigParams.SystemConfig.EnableConfig|=0x800;config->tConfigInfo.SystemConfigParams=1;status=NFCSTATUS_SUCCESS;
    }
    STORE_SYSTEM(0x5f,0x1000,bFelicaTSN);
#undef STORE_SYSTEM
#define STORE_SWP(id,mask,bit,field) \
    STORE_PARAMETER(id,tSwpConfigParams,mask,bit,field,parameter[3]); \
    if(parameter[1]==(id) && parameter[2]) config->tConfigInfo.SwpConfigParams=1
    STORE_SWP(0xc0,SwpConfig.EnableConfig,1,bSwpBitRateInt1);
    STORE_SWP(0xec,SwpConfig2.EnableConfig2,1,bSwpInt1Cfg);
    STORE_SWP(0xed,SwpConfig2.EnableConfig2,2,bSwpInt2Cfg);
    STORE_SWP(0xee,SwpConfig2.EnableConfig2,4,bSwpSvddOn);
#undef STORE_SWP
#define STORE_DISCOVERY(id,bit,field) \
    STORE_PARAMETER(id,tNxpExtnRfDiscParams,NxpExtnRfDiscConf.EnableConfig,bit,field,parameter[3]); \
    if(parameter[1]==(id) && parameter[2]) config->tConfigInfo.NxpExtnRfDiscConfig=1
    STORE_DISCOVERY(0x40,1,bTagDetector);
    STORE_DISCOVERY(0x41,2,bTagDetectorThresh);
    if(parameter[1]==0x42) {
        memcpy(&config->tNxpExtnRfDiscParams.wTagDetectorPeriod,parameter+3,2);
        config->tNxpExtnRfDiscParams.NxpExtnRfDiscConf.EnableConfig|=4;
        config->tConfigInfo.NxpExtnRfDiscConfig=1;status=NFCSTATUS_SUCCESS;
    }
    STORE_DISCOVERY(0x43,8,bTagDetectorFallBackCnt);
    STORE_DISCOVERY(0x44,0x10,bPollProfileSel);
    STORE_DISCOVERY(0x45,0x20,bP15693BailOut);
    STORE_DISCOVERY(0x46,0x200,bGtNfcAACfg);
    STORE_DISCOVERY(0x47,0x400,bGtNfcAPCfg);
    STORE_DISCOVERY(0x48,0x800,bGtNfcBCfg);
    STORE_DISCOVERY(0x49,0x1000,bGtNfcFCfg);
    STORE_DISCOVERY(0x4a,0x2000,bGtNfc15693Cfg);
#undef STORE_DISCOVERY
#define STORE_PROPERTY(id,bit,field) \
    STORE_PARAMETER(id,tPropDiscParams,PropConfig.EnableConfig,bit,field,parameter[3]); \
    if(parameter[1]==(id) && parameter[2]) config->tConfigInfo.PropConfig=1
    STORE_PROPERTY(0x80,4,bToRfOff);
    STORE_PROPERTY(0x81,8,bLstnProfileSel);
    if(parameter[1]==0x4c && parameter[2]) {
        memcpy(&config->tPropDiscParams.wFelicaSysCode,parameter+3,2);
        config->tPropDiscParams.PropConfig.EnableConfig|=2;config->tConfigInfo.PropConfig=1;status=NFCSTATUS_SUCCESS;
    }
    STORE_PROPERTY(0x4b,1,bFelicaReqCode);
#undef STORE_PROPERTY
#undef STORE_PARAMETER
    return status;
}

NFCSTATUS phNciNfc_UpdateReqParamIDs(void* context,uint8_t* processed,uint8_t processed_count,uint8_t processed_length)
{
    phNciNfc_Context_t* nci=context;
    uint8_t remaining[254]={0};size_t position,used=0,search;
    uint8_t* request;size_t length;
    if(!nci || !nci->tRfConfContext.pReqParamList ||
       (processed_length && !processed) || (size_t)processed_count*2!=processed_length)
        return NFCSTATUS_INVALID_PARAMETER;
    request=nci->tRfConfContext.pReqParamList+1;length=nci->tRfConfContext.bReqParamLen;
    if(length%2 || length>sizeof(remaining) || length/2!=nci->tRfConfContext.bReqParamNum)
        return NFCSTATUS_INVALID_PARAMETER;
    for(search=0;search<processed_length;search+=2)
        if(processed[search]!=0xa0) return NFCSTATUS_INVALID_PARAMETER;
    for(position=0;position<length;position+=2) {
        int found=0;
        if(request[position]!=0xa0) return NFCSTATUS_INVALID_PARAMETER;
        for(search=0;search<processed_length;search+=2)
            if(!memcmp(request+position,processed+search,2)) { found=1;break; }
        if(!found) {
            if(used>sizeof(remaining)-2) return NFCSTATUS_INVALID_PARAMETER;
            memcpy(remaining+used,request+position,2);used+=2;
        }
    }
    if(used>length || used>sizeof(remaining)) return NFCSTATUS_INVALID_PARAMETER;
    if(used) memcpy(request,remaining,used);
    nci->tRfConfContext.bReqParamNum=(uint8_t)(used/2);
    nci->tRfConfContext.bReqParamLen=(uint8_t)used;
    nci->tRfConfContext.pReqParamList[0]=(uint8_t)(used/2);
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_ProcessRfPropParams(void* context,uint16_t length,uint8_t* parameters,uint8_t store_values)
{
    phNciNfc_Context_t* nci=context;
    phNciNfc_RfDiscConfigPropParams_t observed={0};
    uint8_t processed[254];size_t position=0,used=0;
    NFCSTATUS status;
    if(!nci || length>255 || (length && !parameters) || store_values>1 ||
       (store_values && !nci->pUpperLayerInfo)) return NFCSTATUS_INVALID_PARAMETER;
    if(store_values) observed=*(phNciNfc_RfDiscConfigPropParams_t*)nci->pUpperLayerInfo;
    while(position<length) {
        size_t span;
        if(length-position<3 || parameters[position]!=0xa0 || used+2>sizeof(processed))
            return NFCSTATUS_INVALID_PARAMETER;
        span=(size_t)parameters[position+2]+3;
        if(span>length-position) return NFCSTATUS_INVALID_PARAMETER;
        if(store_values) {
            status=phNciNfc_StoreRfPropParams(parameters+position,(uint16_t)span,&observed);
            if(status) return status;
        } else if(parameters[position+2]) return NFCSTATUS_INVALID_PARAMETER;
        processed[used++]=0xa0;processed[used++]=parameters[position+1];
        position+=span;
    }
    // A nonempty request must make progress, even on a partial response.
    {
        uint8_t previous=nci->tRfConfContext.bReqParamNum;
        status=phNciNfc_UpdateReqParamIDs(nci,processed,(uint8_t)(used/2),(uint8_t)used);
        if(status) return status;
        if(previous && nci->tRfConfContext.bReqParamNum>=previous) return NFCSTATUS_FAILED;
    }
    if(store_values) *(phNciNfc_RfDiscConfigPropParams_t*)nci->pUpperLayerInfo=observed;
    return NFCSTATUS_SUCCESS;
}

void phNciNfc_CmpParamsSetConfigPropOpt(phNciNfc_RfDiscConfigPropParams_t* desired,phNciNfc_RfDiscConfigPropParams_t* observed)
{
    // Comparisons include the original object padding. Whole-group equality
    // clears its selector without changing its leaf masks; partial equality
    // clears only the explicitly compared fields below.
    if(!memcmp(desired,observed,sizeof(*desired))) { memset(&desired->tConfigInfo,0,sizeof(desired->tConfigInfo));return; }
#define CLEAR_EQUAL(mask,bit,field) \
    if(((mask)&(bit)) && !memcmp(&desired->field,&observed->field,sizeof(desired->field))) (mask)&=~(uint32_t)(bit)
    if(desired->tConfigInfo.SystemConfigParams) {
        if(!memcmp(&desired->tSysConfigParams,&observed->tSysConfigParams,sizeof(desired->tSysConfigParams))) desired->tConfigInfo.SystemConfigParams=0;
        else {
#define SYSTEM_EQUAL(bit,field) CLEAR_EQUAL(desired->tSysConfigParams.SystemConfig.EnableConfig,bit,tSysConfigParams.field)
            SYSTEM_EQUAL(1,wHostIfCharCfg);SYSTEM_EQUAL(2,SystemNtfCfg);SYSTEM_EQUAL(4,bClkReqCfg);
            SYSTEM_EQUAL(0x10,bClkTimeOutCfg);SYSTEM_EQUAL(8,ClkSelCfg);SYSTEM_EQUAL(0x20,bIrqPolarityCfg);
            SYSTEM_EQUAL(0x40,bVbatMonitorEnCfg);SYSTEM_EQUAL(0x80,VenCfg);SYSTEM_EQUAL(0x100,bPwrByFieldEnCfg);
            SYSTEM_EQUAL(0x200,bToBeforeStandByCfg);SYSTEM_EQUAL(0x800,eSysESeIntfSel);SYSTEM_EQUAL(0x1000,bFelicaTSN);
#undef SYSTEM_EQUAL
            if(!desired->tSysConfigParams.SystemConfig.EnableConfig) desired->tConfigInfo.SystemConfigParams=0;
        }
    }
    if(desired->tConfigInfo.SwpConfigParams) {
        if(!memcmp(&desired->tSwpConfigParams,&observed->tSwpConfigParams,sizeof(desired->tSwpConfigParams))) desired->tConfigInfo.SwpConfigParams=0;
        else {
            CLEAR_EQUAL(desired->tSwpConfigParams.SwpConfig.EnableConfig,1,tSwpConfigParams.bSwpBitRateInt1);
            CLEAR_EQUAL(desired->tSwpConfigParams.SwpConfig2.EnableConfig2,1,tSwpConfigParams.bSwpInt1Cfg);
            CLEAR_EQUAL(desired->tSwpConfigParams.SwpConfig2.EnableConfig2,2,tSwpConfigParams.bSwpInt2Cfg);
            CLEAR_EQUAL(desired->tSwpConfigParams.SwpConfig2.EnableConfig2,4,tSwpConfigParams.bSwpSvddOn);
        }
    }
    if(desired->tConfigInfo.NxpExtnRfDiscConfig) {
        if(!memcmp(&desired->tNxpExtnRfDiscParams,&observed->tNxpExtnRfDiscParams,sizeof(desired->tNxpExtnRfDiscParams))) desired->tConfigInfo.NxpExtnRfDiscConfig=0;
        else {
#define DISCOVERY_EQUAL(bit,field) CLEAR_EQUAL(desired->tNxpExtnRfDiscParams.NxpExtnRfDiscConf.EnableConfig,bit,tNxpExtnRfDiscParams.field)
            DISCOVERY_EQUAL(1,bTagDetector);DISCOVERY_EQUAL(2,bTagDetectorThresh);DISCOVERY_EQUAL(4,wTagDetectorPeriod);
            DISCOVERY_EQUAL(8,bTagDetectorFallBackCnt);DISCOVERY_EQUAL(0x10,bPollProfileSel);DISCOVERY_EQUAL(0x20,bP15693BailOut);
            DISCOVERY_EQUAL(0x200,bGtNfcAACfg);DISCOVERY_EQUAL(0x400,bGtNfcAPCfg);DISCOVERY_EQUAL(0x800,bGtNfcBCfg);
            DISCOVERY_EQUAL(0x1000,bGtNfcFCfg);DISCOVERY_EQUAL(0x2000,bGtNfc15693Cfg);
#undef DISCOVERY_EQUAL
        }
    }
    if(desired->tConfigInfo.PropConfig) {
        if(!memcmp(&desired->tPropDiscParams,&observed->tPropDiscParams,sizeof(desired->tPropDiscParams))) desired->tConfigInfo.PropConfig=0;
        else {
            CLEAR_EQUAL(desired->tPropDiscParams.PropConfig.EnableConfig,1,tPropDiscParams.bFelicaReqCode);
            CLEAR_EQUAL(desired->tPropDiscParams.PropConfig.EnableConfig,2,tPropDiscParams.wFelicaSysCode);
            CLEAR_EQUAL(desired->tPropDiscParams.PropConfig.EnableConfig,4,tPropDiscParams.bToRfOff);
            CLEAR_EQUAL(desired->tPropDiscParams.PropConfig.EnableConfig,8,tPropDiscParams.bLstnProfileSel);
        }
    }
#undef CLEAR_EQUAL
}

NFCSTATUS phNciNfc_GetConfigPropRsp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;uint8_t* response;
    if(status) return status;
    if(!nci || !nci->RspBuffInfo.pBuff || nci->RspBuffInfo.wLen<2 ||
       nci->RspBuffInfo.wLen>255) return NFCSTATUS_INVALID_PARAMETER;
    response=nci->RspBuffInfo.pBuff;
    {
        size_t cursor=2,count=0;
        while(cursor<nci->RspBuffInfo.wLen) {
            size_t span;
            if(nci->RspBuffInfo.wLen-cursor<3 || response[cursor]!=0xa0) return NFCSTATUS_INVALID_PARAMETER;
            span=(size_t)response[cursor+2]+3;
            if(span>nci->RspBuffInfo.wLen-cursor) return NFCSTATUS_INVALID_PARAMETER;
            cursor+=span;++count;
        }
        if(count!=response[1]) return NFCSTATUS_INVALID_PARAMETER;
    }
    if(response[0]==0) {
        if(nci->tRfConfContext.bReqParamNum!=response[1]) return NFCSTATUS_FAILED;
        status=phNciNfc_ProcessRfPropParams(nci,(uint16_t)(nci->RspBuffInfo.wLen-2),response+2,1);
        if(!nci->tRfConfContext.bReqParamNum) status=NFCSTATUS_SUCCESS;
        return status;
    }
    if(response[0]==9) return phNciNfc_ProcessRfPropParams(nci,(uint16_t)(nci->RspBuffInfo.wLen-2),response+2,0);
    if(response[0]==10 && nci->tRfConfContext.bReqParamNum>=response[1])
        return phNciNfc_ProcessRfPropParams(nci,(uint16_t)(nci->RspBuffInfo.wLen-2),response+2,1);
    return NFCSTATUS_FAILED;
}

NFCSTATUS phNciNfc_CompleteGetConfigProp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return NFCSTATUS_SUCCESS;
    if(status) {
        phNciNfc_Notify(nci,status,nci->pUpperLayerInfo);phNciNfc_ClearConfigParams(nci);return status;
    }
    if(!nci->tRfConfContext.bReqParamNum) {
        if(nci->tSendPayload.pBuff) { nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0; }
        phNciNfc_Notify(nci,NFCSTATUS_SUCCESS,nci->pUpperLayerInfo);
        return phNciNfc_ClearConfigParams(nci);
    }
    PHNCINFC_INIT_SEQUENCE(nci,gphNciNfc_GetConfigPropSequence);
    nci->tSendPayload.pBuff=nci->tRfConfContext.pReqParamList;
    nci->tSendPayload.wPayloadSize=(uint16_t)(nci->tRfConfContext.bReqParamLen+1);
    return phNciNfc_GenericSequence(nci,NULL,NFCSTATUS_SUCCESS);
}

NFCSTATUS phNciNfc_GetConfigPropOpt(void* context)
{
    // Original C static local at RVA34760; shared across invocations/contexts.
    phNciNfc_RfDiscConfigPropParams_t* pGetConfigParams;
    phNciNfc_Context_t* nci=context;
    phNciNfc_RfDiscConfigPropParams_t* desired;
    uint8_t ids[256],count=0,length=0;NFCSTATUS status;phNciNfc_CoreTxInfo_t command={0};
    if(!nci) return NFCSTATUS_FAILED;
    pGetConfigParams=malloc(sizeof(*pGetConfigParams));nci->tSetConfPropInfo.pGetCfgParams=pGetConfigParams;
    if(!pGetConfigParams) return NFCSTATUS_FAILED;
    memset(pGetConfigParams,0,sizeof(*pGetConfigParams));desired=nci->tSetConfPropInfo.pSetConfParams;
    pGetConfigParams->tConfigInfo=desired->tConfigInfo;
    pGetConfigParams->tSysConfigParams.SystemConfig=desired->tSysConfigParams.SystemConfig;
    pGetConfigParams->tSwpConfigParams.SwpConfig=desired->tSwpConfigParams.SwpConfig;
    pGetConfigParams->tSwpConfigParams.SwpConfig2=desired->tSwpConfigParams.SwpConfig2;
    pGetConfigParams->tNxpExtnRfDiscParams.NxpExtnRfDiscConf=desired->tNxpExtnRfDiscParams.NxpExtnRfDiscConf;
    pGetConfigParams->tNxpExtnRfDiscParams.ConfigMifKeys=desired->tNxpExtnRfDiscParams.ConfigMifKeys;
    pGetConfigParams->tPropDiscParams.PropConfig=desired->tPropDiscParams.PropConfig;
    status=phNciNfc_GetRfPropParams(pGetConfigParams,ids,&count,&length);
    if(status) return status;
    if(count) {
        uint8_t* request=malloc((unsigned)length+1);
        if(!request) return NFCSTATUS_FAILED;
        request[0]=count;memcpy(request+1,ids,length);
        // The original rereads its static pointer after allocation/copy.
        nci->pUpperLayerInfo=pGetConfigParams;nci->tSendPayload.pBuff=request;nci->tSendPayload.wPayloadSize=(uint16_t)(length+1);
        memset(pGetConfigParams,0,sizeof(*pGetConfigParams));
        nci->tRfConfContext.bReqParamNum=count;nci->tRfConfContext.bReqParamLen=length;nci->tRfConfContext.pReqParamList=request;
    }
    command.Buff=nci->tSendPayload.pBuff;command.wLen=nci->tSendPayload.wPayloadSize;
    command.tHeaderInfo.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlCmd;
    command.tHeaderInfo.Group_ID=phNciNfc_e_CoreNciCoreGid;command.tHeaderInfo.Opcode_ID.Val=3;
    return phNciNfc_CoreIfTxRx(&nci->NciCoreContext,&command,&nci->RspBuffInfo,2000,phNciNfc_GenericSequence,nci);
}

NFCSTATUS phNciNfc_GetConfigPropOptRsp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;uint8_t* response;
    if(status) return status;
    if(!nci || !nci->RspBuffInfo.pBuff || nci->RspBuffInfo.wLen<2 ||
       nci->RspBuffInfo.wLen>255) return NFCSTATUS_INVALID_PARAMETER;
    response=nci->RspBuffInfo.pBuff;
    {
        size_t cursor=2,count=0;
        while(cursor<nci->RspBuffInfo.wLen) {
            size_t span;
            if(nci->RspBuffInfo.wLen-cursor<3 || response[cursor]!=0xa0) return NFCSTATUS_INVALID_PARAMETER;
            span=(size_t)response[cursor+2]+3;
            if(span>nci->RspBuffInfo.wLen-cursor) return NFCSTATUS_INVALID_PARAMETER;
            cursor+=span;++count;
        }
        if(count!=response[1]) return NFCSTATUS_INVALID_PARAMETER;
    }
    if(response[0]==0) {
        if(nci->tRfConfContext.bReqParamNum!=response[1]) return NFCSTATUS_FAILED;
        status=phNciNfc_ProcessRfPropParams(nci,(uint16_t)(nci->RspBuffInfo.wLen-2),response+2,1);
        if(!nci->tRfConfContext.bReqParamNum) status=NFCSTATUS_SUCCESS;
    } else if(response[0]==9) status=phNciNfc_ProcessRfPropParams(nci,(uint16_t)(nci->RspBuffInfo.wLen-2),response+2,0);
    else if(response[0]==10 && nci->tRfConfContext.bReqParamNum>=response[1]) status=phNciNfc_ProcessRfPropParams(nci,(uint16_t)(nci->RspBuffInfo.wLen-2),response+2,1);
    else return NFCSTATUS_FAILED;
    if(status) return status;
    if(!nci->tRfConfContext.bReqParamNum) {
        if(nci->tSendPayload.pBuff) { nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0; }
        phNciNfc_CmpParamsSetConfigPropOpt(nci->tSetConfPropInfo.pSetConfParams,nci->pUpperLayerInfo);
        return phNciNfc_ClearConfigParams(nci);
    }
    nci->tSendPayload.pBuff=nci->tRfConfContext.pReqParamList;nci->tSendPayload.wPayloadSize=(uint16_t)(nci->tRfConfContext.bReqParamLen+1);
    (void)phNciNfc_RepeatSequenceSeq(nci,nci->pSeqHandler,1);
    return status;
}

NFCSTATUS phNciNfc_SetConfigPropRfParameters(void* context,phNciNfc_RfDiscConfigPropParams_t* config,void (*callback)(void*,NFCSTATUS,void*),void* callback_context)
{
    phNciNfc_Context_t* nci=context;uint16_t bytes=0;uint8_t count=0;NFCSTATUS status;
    if(!nci || nci!=gpphNciNfc_Context) return NFCSTATUS_NOT_INITIALISED;
    if(!config || !callback) return NFCSTATUS_INVALID_PARAMETER;
    status=phNciNfc_ValidateSetConfPropParams(config,&bytes,&count);
    if(status) return status;
    nci->tSetConfPropInfo.pSetConfParams=malloc(sizeof(*config));
    if(!nci->tSetConfPropInfo.pSetConfParams) return NFCSTATUS_FAILED;
    memset(nci->tSetConfPropInfo.pSetConfParams,0,sizeof(*config));
    memcpy(nci->tSetConfPropInfo.pSetConfParams,config,sizeof(*config));
    PHNCINFC_INIT_SEQUENCE(nci,gphNciNfc_SetConfigPropSequence);
    nci->IfNtf=callback;nci->IfNtfCtx=callback_context;
    return phNciNfc_GenericSequence(nci,NULL,NFCSTATUS_SUCCESS);
}
