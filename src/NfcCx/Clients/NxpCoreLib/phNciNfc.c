#include "phNciNfc.h"
#include <stdlib.h>
#include <string.h>
#include "nxp_trace.h"
#include "phNciNfc.tmh"

phNciNfc_Context_t* volatile gpphNciNfc_Context;
pphNciNfc_CoreContext_t volatile gpphNciNfc_CoreContext;

NFCSTATUS phNciNfc_Initialise(void* hardware,uint8_t logDataMessages,void** handle)
{
    NFCSTATUS status;
    if(!hardware || !handle) return NFCSTATUS_INVALID_PARAMETER;
    *handle=NULL;
    if(gpphNciNfc_Context) return NFCSTATUS_BUSY;
    gpphNciNfc_Context=malloc(sizeof(*gpphNciNfc_Context));
    // Failure leaves an explicitly empty output handle.
    if(!gpphNciNfc_Context) return NFCSTATUS_INSUFFICIENT_RESOURCES;
    memset(gpphNciNfc_Context,0,sizeof(*gpphNciNfc_Context));
    phNciNfc_NciCtxInitialize(gpphNciNfc_Context);
    gpphNciNfc_CoreContext=&gpphNciNfc_Context->NciCoreContext;
    gpphNciNfc_CoreContext->pHwRef=hardware;
    gpphNciNfc_CoreContext->bLogDataMessages=logDataMessages;
    status=phNciNfc_CoreInitialise(gpphNciNfc_CoreContext);
    if(status!=NFCSTATUS_SUCCESS) {
        if(gpphNciNfc_Context) free(gpphNciNfc_Context);
        gpphNciNfc_Context=NULL;gpphNciNfc_CoreContext=NULL;
    }
    *handle=gpphNciNfc_Context;
    return status;
}

NFCSTATUS phNciNfc_Deinitialise(void* handle)
{
    if(gpphNciNfc_Context!=handle) return NFCSTATUS_INVALID_PARAMETER;
    if(!gpphNciNfc_Context) return NFCSTATUS_NOT_INITIALISED;
    if(gpphNciNfc_CoreContext) {
        (void)phNciNfc_CoreRelease(gpphNciNfc_CoreContext);
        gpphNciNfc_CoreContext=NULL;
    }
    if(gpphNciNfc_Context->tRfConfContext.pReqParamList!=gpphNciNfc_Context->tSendPayload.pBuff)
        free(gpphNciNfc_Context->tRfConfContext.pReqParamList);
    free(gpphNciNfc_Context->tSetConfPropInfo.pSetConfParams);
    free(gpphNciNfc_Context->tSetConfPropInfo.pGetCfgParams);
    if(gpphNciNfc_Context->tSendPayload.pBuff) {
        if(gpphNciNfc_Context->tSendPayload.pBuff) free(gpphNciNfc_Context->tSendPayload.pBuff);
        gpphNciNfc_Context->tSendPayload.pBuff=NULL;
        gpphNciNfc_Context->tSendPayload.wPayloadSize=0;
    }
    if(gpphNciNfc_Context->InitRspParams.ManufacturerInfo.Buffer) {
        if(gpphNciNfc_Context->InitRspParams.ManufacturerInfo.Buffer) free(gpphNciNfc_Context->InitRspParams.ManufacturerInfo.Buffer);
        gpphNciNfc_Context->InitRspParams.ManufacturerInfo.Buffer=NULL;
    }
    if(gpphNciNfc_Context) free(gpphNciNfc_Context);
    gpphNciNfc_Context=NULL;
    return NFCSTATUS_SUCCESS;
}

NFCSTATUS phNciNfc_GetNfccCapabilities(void* handle,phNciNfc_NfccCapabilities_t* capabilities)
{
    phNciNfc_Context_t* nci=handle;
    const uint8_t* manufacturer;
    if(!handle || handle!=gpphNciNfc_Context) return NFCSTATUS_NOT_INITIALISED;
    if(!capabilities || nci->InitRspParams.ManufacturerInfo.Length<4 ||
       !nci->InitRspParams.ManufacturerInfo.Buffer) return NFCSTATUS_INVALID_PARAMETER;
    capabilities->ManufacturerId=nci->InitRspParams.ManufacturerId;
    manufacturer=nci->InitRspParams.ManufacturerInfo.Buffer;
    capabilities->HwVersion=manufacturer[0];capabilities->RomVersion=manufacturer[1];
    capabilities->FwVersion=((uint16_t)manufacturer[2]<<8)|manufacturer[3];
    return NFCSTATUS_SUCCESS;
}
