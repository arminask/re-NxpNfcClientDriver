#include "phDnldNfc_Internal.h"
#include <stdlib.h>
#include <phNfcCompId.h>
#include <string.h>
#include "nxp_buffer_validation.h"
#include <phTmlNfc.h>
#include <phOsalNfc_Timer.h>
#include "nxp_trace.h"
#include "phDnldNfc.tmh"

phDnldNfc_DlContext_t* gpphDnldContext;

// Initialize download ownership only for valid hardware and an idle context.
NFCSTATUS phDnldNfc_SetHwDevHandle(void* hardware)
{
    if(!hardware) return NFCSTATUS_INVALID_PARAMETER;
    if(gpphDnldContext && gpphDnldContext->tDnldInProgress!=phDnldNfc_TransitionIdle) return NFCSTATUS_BUSY;
    if(!gpphDnldContext) {
        phDnldNfc_DlContext_t* context=malloc(sizeof(*context));
        if(context) {
            memset(context,0,sizeof(*context));
            gpphDnldContext=context;
        }
    }
    if(hardware && gpphDnldContext) {
        if(gpphDnldContext->TimerInfo.dwRspTimerId) phOsalNfc_Timer_Delete(gpphDnldContext->TimerInfo.dwRspTimerId);
        memset(gpphDnldContext,0,sizeof(*gpphDnldContext));
        gpphDnldContext->pHwRef=hardware;
    }
    return gpphDnldContext?NFCSTATUS_SUCCESS:NFCSTATUS_INSUFFICIENT_RESOURCES;
}

// Retire protocol callbacks before releasing download state.
void phDnldNfc_FreeDnldContext(void)
{
    if(gpphDnldContext) {
        if(gpphDnldContext->TimerInfo.dwRspTimerId)
            phOsalNfc_Timer_Delete(gpphDnldContext->TimerInfo.dwRspTimerId);
        (void)phTmlNfc_ReadAbort(gpphDnldContext->pHwRef);
        (void)phTmlNfc_WriteAbort(gpphDnldContext->pHwRef);
        free(gpphDnldContext);
        gpphDnldContext=NULL;
    }
}

#define DOWNLOAD_INVALID_PARAMETER PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER)
#define DOWNLOAD_BUSY PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_BUSY)

// Establish callback/capacity ownership before a potentially reentrant dispatch.
static NFCSTATUS SubmitDownload(pphDnldNfc_DlContext_t context,phDnldNfc_Event_t event,
    pphDnldNfc_RspCb_t callback,void* caller)
{
    NFCSTATUS status;
    context->wRspCapacity=context->tRspBuffInfo.wLen;
    context->UserCb=callback;context->UserCtxt=caller;
    status=phDnldNfc_CmdHandler(context,event);
    return status;
}

NFCSTATUS phDnldNfc_Reset(pphDnldNfc_RspCb_t callback,void* caller)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    if(!callback || !caller) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    context->tRspBuffInfo.pBuff=NULL;context->tRspBuffInfo.wLen=0;
    context->tUserData.pBuff=NULL;context->tUserData.wLen=0;
    context->tCmdId=PH_DL_CMD_RESET;context->FrameInp.Type=phDnldNfc_FTNone;
    return SubmitDownload(context,phDnldNfc_EventReset,callback,caller);
}

static NFCSTATUS GetDownloadInfo(pphDnldNfc_Buff_t response,pphDnldNfc_RspCb_t callback,
    void* caller,phDnldNfc_CmdId_t command,phDnldNfc_Event_t event)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    if(!response || !callback || !caller) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    if(!response->pBuff || !response->wLen) return DOWNLOAD_INVALID_PARAMETER;
    context->tRspBuffInfo.pBuff=response->pBuff;context->tRspBuffInfo.wLen=response->wLen;
    context->tUserData.pBuff=NULL;context->tUserData.wLen=0;
    context->tCmdId=command;context->FrameInp.Type=phDnldNfc_FTNone;
    return SubmitDownload(context,event,callback,caller);
}
NFCSTATUS phDnldNfc_GetVersion(pphDnldNfc_Buff_t response,pphDnldNfc_RspCb_t callback,void* caller)
{
    return GetDownloadInfo(response,callback,caller,PH_DL_CMD_GETVERSION,phDnldNfc_EventGetVer);
}
NFCSTATUS phDnldNfc_GetSessionState(pphDnldNfc_Buff_t response,pphDnldNfc_RspCb_t callback,void* caller)
{
    return GetDownloadInfo(response,callback,caller,PH_DL_CMD_GETSESSIONSTATE,phDnldNfc_EventGetSesnSt);
}
NFCSTATUS phDnldNfc_CheckIntegrity(phDnldNfc_NFCC_VERSION_t rom,pphDnldNfc_Buff_t response,
    pphDnldNfc_RspCb_t callback,void* caller)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    if(!callback || !caller) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    // This write precedes buffer validation, including null response.
    context->FrameInp.Type=(rom==phDnldNfc_NFCC_ROM_Version_PN547 ||
        ((uint32_t)rom-phDnldNfc_NFCC_ROM_Version_PN548)<=2)?phDnldNfc_ChkIntg:phDnldNfc_FTNone;
    if(!response || !response->pBuff || !response->wLen) return DOWNLOAD_INVALID_PARAMETER;
    context->tRspBuffInfo.pBuff=response->pBuff;context->tRspBuffInfo.wLen=response->wLen;
    context->tUserData.pBuff=NULL;context->tUserData.wLen=0;context->tCmdId=PH_DL_CMD_CHECKINTEGRITY;
    return SubmitDownload(context,phDnldNfc_EventIntegChk,callback,caller);
}
NFCSTATUS phDnldNfc_Force(pphDnldNfc_Buff_t inputs,pphDnldNfc_RspCb_t callback,void* caller)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    // Original touches context before any input validation and while busy.
    context->bPldVal[0]=0x11;context->bPldVal[1]=0;context->bPldVal[2]=0;
    if(!callback || !caller || !inputs) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    context->tRspBuffInfo.pBuff=NULL;context->tRspBuffInfo.wLen=0;
    context->tCmdId=PH_DL_CMD_FORCE;context->FrameInp.Type=phDnldNfc_FTForce;
    if(inputs->wLen && (!inputs->pBuff || inputs->wLen<2)) return DOWNLOAD_INVALID_PARAMETER;
    if(inputs->wLen>=2) {
        // No pointer guard: the caller must provide two readable bytes here.
        uint8_t source=inputs->pBuff[0],frequency=inputs->pBuff[1];
        if(source<1 || source>3) source=2;
        if(frequency>5) frequency=1;
        context->bPldVal[0]=(uint8_t)((source<<3)|frequency);
    }
    context->tUserData.pBuff=context->bPldVal;context->tUserData.wLen=3;
    memset(&context->tRWInfo,0,sizeof(context->tRWInfo));
    return SubmitDownload(context,phDnldNfc_EventForce,callback,caller);
}
NFCSTATUS phDnldNfc_Log(pphDnldNfc_Buff_t data,pphDnldNfc_RspCb_t callback,void* caller)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    if(!data || !callback || !caller) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    if(!data->pBuff || data->wLen==0 || data->wLen>32) return DOWNLOAD_INVALID_PARAMETER;
    context->tRspBuffInfo.pBuff=NULL;context->tRspBuffInfo.wLen=0;
    context->tCmdId=PH_DL_CMD_LOG;context->FrameInp.Type=phDnldNfc_FTLog;
    context->tUserData.pBuff=data->pBuff;context->tUserData.wLen=data->wLen;
    memset(&context->tRWInfo,0,sizeof(context->tRWInfo));
    return SubmitDownload(context,phDnldNfc_EventLog,callback,caller);
}
NFCSTATUS phDnldNfc_ReadLog(pphDnldNfc_Buff_t response,pphDnldNfc_RspCb_t callback,void* caller)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    if(!response || !callback || !caller) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    if(!response->pBuff || !response->wLen) return DOWNLOAD_INVALID_PARAMETER;
    context->tCmdId=PH_DL_CMD_READ;context->FrameInp.Type=phDnldNfc_FTRead;context->FrameInp.dwAddr=0x201f80;
    context->tRspBuffInfo.pBuff=response->pBuff;context->tRspBuffInfo.wLen=response->wLen;
    context->tUserData.pBuff=NULL;context->tUserData.wLen=0;
    memset(&context->tRWInfo,0,sizeof(context->tRWInfo));
    return SubmitDownload(context,phDnldNfc_EventRead,callback,caller);
}
NFCSTATUS phDnldNfc_Write(uint8_t recover,phNfc_sData_t* data,pphDnldNfc_RspCb_t callback,void* caller)
{
    pphDnldNfc_DlContext_t context=gpphDnldContext;
    if(!context) return NFCSTATUS_NOT_INITIALISED;
    (void)recover; // Ignored in this private build, unlike the public Android API.
    if(!callback || !caller || !data) return DOWNLOAD_INVALID_PARAMETER;
    if(context->tDnldInProgress!=phDnldNfc_TransitionIdle) return DOWNLOAD_BUSY;
    if(!NxpValidateFirmwareImage(data->buffer,data->length)) return NFCSTATUS_INVALID_PARAMETER;
    context->tRspBuffInfo.pBuff=NULL;context->tRspBuffInfo.wLen=0;
    context->tCmdId=PH_DL_CMD_WRITE;context->FrameInp.Type=phDnldNfc_FTWrite;
    context->tUserData.pBuff=data->buffer;context->tUserData.wLen=(uint16_t)data->length;
    context->bResendLastFrame=0;
    memset(&context->tRWInfo,0,sizeof(context->tRWInfo));context->tRWInfo.bFirstWrReq=1;
    return SubmitDownload(context,phDnldNfc_EventWrite,callback,caller);
}
