#include "phNciNfc.h"
#include "nxp_trace.h"
#include "phNciNfc_Context.tmh"

void phNciNfc_NciCtxInitialize(phNciNfc_Context_t* context)
{
    if(context) {
        context->tRfConfContext.pReqParamList=NULL;
        context->IfNtf=NULL;context->IfNtfCtx=NULL;context->pSeqHandler=NULL;
        context->RspBuffInfo.pBuff=NULL;context->tSendPayload.pBuff=NULL;
        context->pUpperLayerInfo=NULL;
    }
}
