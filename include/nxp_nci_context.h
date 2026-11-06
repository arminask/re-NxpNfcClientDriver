#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <phNciNfc_Core.h>
#include <phNciNfcTypes.h>
#include <phNciNfc_CoreIf.h>
#ifdef __cplusplus
}
#endif

// Recovered NxpCoreLib types from the exact PDB, not the much larger public
// NfcCoreLib upper stack. C declarations shared by C originals and C++ clients.
typedef struct phNciNfc_RfConfigCtx {
    uint8_t bReqParamNum;
    uint8_t bReqParamLen;
    uint8_t* pReqParamList;
} phNciNfc_RfConfigCtx_t;
typedef struct phNciNfc_SendPayload {
    void* pBuff;
    uint16_t wPayloadSize;
} phNciNfc_SendPayload_t;
typedef struct phNciNfc_RfDiscConfigPropParams phNciNfc_RfDiscConfigPropParams_t;
typedef struct phNciNfc_SetConfigPropOptInfo {
    phNciNfc_RfDiscConfigPropParams_t* pSetConfParams;
    phNciNfc_RfDiscConfigPropParams_t* pGetCfgParams;
} phNciNfc_SetConfigPropOptInfo_t;
typedef enum phNciNfc_Prbs_Technology {
    phNciNfc_e_Type_A=0, phNciNfc_e_Type_B=1, phNciNfc_e_Type_F=2
} phNciNfc_Prbs_Technology_t;
typedef enum phNciNfc_Prbs_BitRates {
    phNciNfc_e_106_AB_212_F=0, phNciNfc_e_212_AB_424_F=1,
    phNciNfc_e_424_AB=2, phNciNfc_e_848_AB=3
} phNciNfc_Prbs_BitRates_t;
typedef struct phNciNfc_Prbs_Param {
    phNciNfc_Prbs_Technology_t tTechnology;
    phNciNfc_Prbs_BitRates_t tBitrate;
    uint16_t wSeriesLen;
} phNciNfc_Prbs_Param_t;
typedef struct phNciNfc_SelfTestInfo {
    uint8_t bSwpConn;
    phNciNfc_Prbs_Param_t tPrbsParam;
} phNciNfc_SelfTestInfo_t;
typedef struct phNciNfc_SequenceParams {
    NFCSTATUS (*SequnceInitiate)(void*); // Original PDB spelling.
    NFCSTATUS (*SequenceProcess)(void*,NFCSTATUS);
} phNciNfc_SequenceParams_t;
typedef struct phNciNfc_Context {
    phNciNfc_sInitRspParams_t InitRspParams;
    phNciNfc_RfConfigCtx_t tRfConfContext;
    phNciNfc_SendPayload_t tSendPayload;
    phNciNfc_CoreContext_t NciCoreContext;
    void (*IfNtf)(void*,NFCSTATUS,void*);
    void* IfNtfCtx;
    phNciNfc_SequenceParams_t* pSeqHandler;
    phNciNfc_Buff_t RspBuffInfo;
    uint8_t SeqNext;
    uint8_t SeqMax;
    void* pUpperLayerInfo;
    uint32_t dwNtfTimerId;
    phNciNfc_SetConfigPropOptInfo_t tSetConfPropInfo;
    phNciNfc_SelfTestInfo_t tTestInfo;
} phNciNfc_Context_t;

#ifdef __cplusplus
extern "C" {
#endif
NFCSTATUS phNciNfc_Initialise(void*,uint8_t,void**);
NFCSTATUS phNciNfc_ProcessInitRsp(void*,phNciNfc_Data_t*);
NFCSTATUS phNciNfc_ProcessResetNtf(void*,phNciNfc_Data_t*);
#ifdef __cplusplus
}
#endif
