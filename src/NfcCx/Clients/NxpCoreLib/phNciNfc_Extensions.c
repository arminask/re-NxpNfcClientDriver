#include "phNciNfc_Extensions.h"
#include <stdlib.h>
#include <phOsalNfc_Timer.h>
#include <phNciNfc_CoreRecvMgr.h>
#include "nxp_trace.h"
#include "phNciNfc_Extensions.tmh"

// Exact PDB/data RVAs 0x33410 and 0x33430, two 16-byte entries each.
static phNciNfc_SequenceParams_t gphNciNfc_EnableExtnSequence[]={
    {phNciNfc_EnableExtnCmd,phNciNfc_EnableExtnRsp},{NULL,phNciNfc_CompleteEnableExtnSeq}
};
static phNciNfc_SequenceParams_t gphNciNfc_NfccStandbySequence[]={
    {phNciNfc_SendStandbyCmd,phNciNfc_ProcessStandbyRsp},{NULL,phNciNfc_CompleteStandbySequence}
};

NFCSTATUS phNciNfc_EnableExtnCmd(void* context)
{
    phNciNfc_Context_t* nci=context;
    phNciNfc_CoreTxInfo_t command={0};
    if(!nci) return NFCSTATUS_INVALID_PARAMETER;
    command.tHeaderInfo.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlCmd;
    command.tHeaderInfo.Group_ID=phNciNfc_e_CorePropGid;command.tHeaderInfo.Opcode_ID.Val=2;
    return phNciNfc_CoreIfTxRx(&nci->NciCoreContext,&command,&nci->RspBuffInfo,2000,phNciNfc_GenericSequence,nci);
}

NFCSTATUS phNciNfc_EnableExtnRsp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return NFCSTATUS_INVALID_PARAMETER;
    if(status!=NFCSTATUS_SUCCESS) return status;
    if(nci->RspBuffInfo.wLen==5) {
        if(nci->RspBuffInfo.pBuff[0]==0) return NFCSTATUS_SUCCESS;
        if(nci->RspBuffInfo.pBuff[0]==1 || nci->RspBuffInfo.pBuff[0]==6) return 0x93;
    } else if(nci->RspBuffInfo.wLen==1 && (nci->RspBuffInfo.pBuff[0]==1 || nci->RspBuffInfo.pBuff[0]==6)) return 0x93;
    return NFCSTATUS_FAILED;
}

NFCSTATUS phNciNfc_CompleteEnableExtnSeq(void* context,NFCSTATUS status)
{
    if(context) phNciNfc_Notify(context,status,NULL);
    return status;
}

NFCSTATUS phNciNfc_EnablePropExtn(void* context,void (*callback)(void*,NFCSTATUS,void*),void* callbackContext)
{
    phNciNfc_Context_t* nci=context;
    if(!nci || context!=gpphNciNfc_Context) return NFCSTATUS_NOT_INITIALISED;
    if(!callback) return NFCSTATUS_INVALID_PARAMETER;
    nci->IfNtf=callback;nci->IfNtfCtx=callbackContext;nci->pSeqHandler=gphNciNfc_EnableExtnSequence;
    nci->SeqNext=0;nci->SeqMax=phNciNfc_getSequenceLength(nci->pSeqHandler);
    return phNciNfc_GenericSequence(nci,NULL,NFCSTATUS_SUCCESS);
}

NFCSTATUS phNciNfc_SendStandbyCmd(void* context)
{
    phNciNfc_Context_t* nci=context;phNciNfc_CoreTxInfo_t command={0};
    if(!nci) return NFCSTATUS_FAILED;
    command.tHeaderInfo.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlCmd;
    command.tHeaderInfo.Group_ID=phNciNfc_e_CorePropGid;
    command.Buff=nci->tSendPayload.pBuff;command.wLen=nci->tSendPayload.wPayloadSize;
    return phNciNfc_CoreIfTxRx(&nci->NciCoreContext,&command,&nci->RspBuffInfo,2000,phNciNfc_GenericSequence,nci);
}

NFCSTATUS phNciNfc_ProcessStandbyRsp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(status==NFCSTATUS_SUCCESS && nci && (!nci->RspBuffInfo.pBuff || nci->RspBuffInfo.pBuff[0]!=0 || nci->RspBuffInfo.wLen!=1)) {
        nci->RspBuffInfo.wLen=0;return NFCSTATUS_FAILED;
    }
    return status;
}

NFCSTATUS phNciNfc_CompleteStandbySequence(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return NFCSTATUS_FAILED;
    if(nci->tSendPayload.pBuff) {
        free(nci->tSendPayload.pBuff);nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0;
    }
    phNciNfc_Notify(nci,status,NULL);return status;
}

NFCSTATUS phNciNfc_SetPowerMode(void* context,phNciNfc_PowerMode_t mode,void (*callback)(void*,NFCSTATUS,void*),void* callbackContext)
{
    phNciNfc_Context_t* nci=context;uint8_t* payload;NFCSTATUS status;
    if(!nci) return NFCSTATUS_NOT_INITIALISED;
    if(!callback) return NFCSTATUS_INVALID_PARAMETER;
    payload=malloc(1);
    if(!payload) return NFCSTATUS_INSUFFICIENT_RESOURCES;
    *payload=(uint8_t)mode;
    nci->pSeqHandler=gphNciNfc_NfccStandbySequence;nci->tSendPayload.pBuff=payload;nci->tSendPayload.wPayloadSize=1;
    nci->IfNtf=callback;nci->IfNtfCtx=callbackContext;nci->SeqNext=0;
    nci->SeqMax=phNciNfc_getSequenceLength(nci->pSeqHandler);
    status=phNciNfc_GenericSequence(nci,NULL,NFCSTATUS_SUCCESS);
    if(status!=NFCSTATUS_PENDING) { free(payload);nci->tSendPayload.pBuff=NULL; }
    return status;
}

NFCSTATUS phNciNfc_ChkPresExtnCmd(void* context)
{
    phNciNfc_Context_t* nci=context;phNciNfc_CoreTxInfo_t command={0};
    if(!nci) return NFCSTATUS_INVALID_PARAMETER;
    command.tHeaderInfo.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlCmd;
    command.tHeaderInfo.Group_ID=phNciNfc_e_CorePropGid;command.tHeaderInfo.Opcode_ID.Val=0x11;
    return phNciNfc_CoreIfTxRx(&nci->NciCoreContext,&command,&nci->RspBuffInfo,2000,phNciNfc_GenericSequence,nci);
}

NFCSTATUS phNciNfc_ChkPresExtnRsp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return NFCSTATUS_INVALID_PARAMETER;
    if(status!=NFCSTATUS_SUCCESS) return status;
    if(nci->RspBuffInfo.wLen==1) {
        if(nci->RspBuffInfo.pBuff[0]==0) return NFCSTATUS_SUCCESS;
        if(nci->RspBuffInfo.pBuff[0]==1) return 0x93;
    }
    return NFCSTATUS_FAILED;
}

NFCSTATUS phNciNfc_CompleteChkPresExtnSeq(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return status;
    if(status==NFCSTATUS_SUCCESS) {
        phNciNfc_sCoreHeaderInfo_t header={0};
        header.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlNtf;
        header.Group_ID=phNciNfc_e_CorePropGid;header.Opcode_ID.Val=0x11;
        status=phNciNfc_CoreIfRegRspNtf(&nci->NciCoreContext,&header,phNciNfc_ChkPresExtnNtfCb,nci);
        if(status!=NFCSTATUS_SUCCESS) status=NFCSTATUS_FAILED;
        else {
            if(!nci->dwNtfTimerId) return NFCSTATUS_SUCCESS;
            status=phOsalNfc_Timer_Start(nci->dwNtfTimerId,750,phNciNfc_ChkPresExtnNtfTimeoutHandler,nci);
            if(status==NFCSTATUS_SUCCESS) return status;
        }
    }
    phNciNfc_Notify(nci,status,NULL);return status;
}

NFCSTATUS phNciNfc_ChkPresExtnNtfCb(void* context,void* information,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;phNciNfc_TransactInfo_t* transaction=information;
    phNciNfc_CoreRegInfo_t registration={0};
    if(!nci || !transaction) return status;
    if(status==NFCSTATUS_SUCCESS) {
        (void)phOsalNfc_Timer_Stop(nci->dwNtfTimerId);
        if(transaction->wLength!=1) status=NFCSTATUS_FAILED;
        else if(transaction->pbuffer[0]==0) status=0x92;
        else if(transaction->pbuffer[0]!=1) status=NFCSTATUS_FAILED;
    } else status=NFCSTATUS_FAILED;
    registration.bGid=15;registration.bOid=0x11;
    registration.pNotifyCb=phNciNfc_ChkPresExtnNtfCb;registration.pContext=nci;
    (void)phNciNfc_CoreRecvMgrDeRegisterCb(&nci->NciCoreContext,&registration,phNciNfc_e_NciCoreMsgTypeCntrlNtf);
    phNciNfc_Notify(nci,status,NULL);return status;
}

void phNciNfc_ChkPresExtnNtfTimeoutHandler(uint32_t timer,void* context)
{
    phNciNfc_Context_t* nci=context;phNciNfc_TransactInfo_t transaction={0};
    (void)timer;
    if(nci) (void)phOsalNfc_Timer_Stop(nci->dwNtfTimerId);
    (void)phNciNfc_ChkPresExtnNtfCb(nci,&transaction,NFCSTATUS_FAILED);
}

NFCSTATUS phNciNfc_ConfigListenFilterCmd(void* context)
{
    phNciNfc_Context_t* nci=context;phNciNfc_CoreTxInfo_t command={0};
    if(!nci) return NFCSTATUS_INVALID_PARAMETER;
    command.tHeaderInfo.eMsgType=phNciNfc_e_NciCoreMsgTypeCntrlCmd;
    command.tHeaderInfo.Group_ID=phNciNfc_e_CorePropGid;command.tHeaderInfo.Opcode_ID.Val=0x15;
    command.Buff=nci->tSendPayload.pBuff;command.wLen=nci->tSendPayload.wPayloadSize;
    return phNciNfc_CoreIfTxRx(&nci->NciCoreContext,&command,&nci->RspBuffInfo,2000,phNciNfc_GenericSequence,nci);
}

NFCSTATUS phNciNfc_ConfigListenFilterRsp(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(!nci) return NFCSTATUS_FAILED;
    if(status!=NFCSTATUS_SUCCESS) return status;
    return nci->RspBuffInfo.wLen==1 && nci->RspBuffInfo.pBuff[0]==0?NFCSTATUS_SUCCESS:NFCSTATUS_FAILED;
}

NFCSTATUS phNciNfc_CompleteConfigListenFilterSeq(void* context,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    if(nci) {
        if(nci->tSendPayload.pBuff) { free(nci->tSendPayload.pBuff);nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0; }
        phNciNfc_Notify(nci,status,NULL);
    }
    return status;
}
