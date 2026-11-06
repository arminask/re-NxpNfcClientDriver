#include "phNciNfc_Common.h"
#include <phNciNfc_CoreIf.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "nxp_trace.h"
#include "phNciNfc_Common.tmh"

void phNciNfc_FreeSendPayloadBuff(void* context)
{
    phNciNfc_Context_t* nci=context;
    if(nci->tSendPayload.pBuff) {
        free(nci->tSendPayload.pBuff);
        nci->tSendPayload.pBuff=NULL;nci->tSendPayload.wPayloadSize=0;
    }
}

NFCSTATUS phNciNfc_GenericSequence(void* context,void* information,NFCSTATUS status)
{
    phNciNfc_Context_t* nci=context;
    phNciNfc_TransactInfo_t* transaction=information;
    if(nci && transaction) {
        nci->RspBuffInfo.pBuff=transaction->pbuffer;
        nci->RspBuffInfo.wLen=transaction->wLength;
    }
    return phNciNfc_SeqHandler(context,status);
}

void phNciNfc_Notify(void* context,NFCSTATUS status,void* information)
{
    phNciNfc_Context_t* nci=context;
    if(nci->IfNtf) {
        void (*callback)(void*,NFCSTATUS,void*)=nci->IfNtf;
        void* callbackContext=nci->IfNtfCtx;
        nci->IfNtf=NULL;nci->IfNtfCtx=NULL;
        callback(callbackContext,status,information);
    }
}

/* The encoded manufacturer length is one byte. Reserve that complete range
 * once: reset/init updates can change Length, while capability readers do not
 * share the receive worker's lock. Keep this owned address stable until the
 * existing context teardown frees it; do not move/free it during an update. */
enum { PHNCINFC_MANUFACTURER_INFO_CAPACITY = UINT8_MAX };
static NFCSTATUS phNciNfc_EnsureManufacturerBuffer(phNciNfc_Context_t* nci)
{
    if(!nci->InitRspParams.ManufacturerInfo.Buffer)
        nci->InitRspParams.ManufacturerInfo.Buffer=malloc(PHNCINFC_MANUFACTURER_INFO_CAPACITY);
    return nci->InitRspParams.ManufacturerInfo.Buffer?NFCSTATUS_SUCCESS:NFCSTATUS_INSUFFICIENT_RESOURCES;
}

NFCSTATUS phNciNfc_ProcessInitRsp(void* context,phNciNfc_Data_t* response)
{
    phNciNfc_Context_t* nci=context;
    if(!nci || !response || !response->pBuff) return NFCSTATUS_INVALID_PARAMETER;
    nci->InitRspParams.ManufacturerInfo.Length=0;
    if(response->wLen>=19 && response->pBuff[0]==0) {
        phNciNfc_sInitRspParams_t* init=&nci->InitRspParams;
        const uint8_t* data=response->pBuff;
        uint8_t count=data[5];
        if(count<=8) {
            if(response->wLen<(uint16_t)(13u+count)) return NFCSTATUS_FAILED;
            if(data[12u+count] && response->wLen<(uint16_t)(17u+count)) return NFCSTATUS_FAILED;
        }
        init->NfccFeatures.DiscConfSuprt=data[1];init->NfccFeatures.RoutingType=data[2];
        init->NfccFeatures.PwrOffState=data[3];init->NfccFeatures.Byte3=data[4];init->NoOfRfIfSuprt=count;
        if(count<=8) {
            uint8_t offset=(uint8_t)(count+6);
            memcpy(init->RfInterfaces,data+6,count);
            init->MaxLogicalCon=data[offset++];
            init->RoutingTableSize=data[offset];
            init->RoutingTableSize|=(uint16_t)data[(uint8_t)(offset+1)]<<8;offset=(uint8_t)(offset+2);
            init->CntrlPktPayloadLen=data[offset++];
            init->MaxSizeLarge=data[offset];
            init->MaxSizeLarge|=(uint16_t)data[(uint8_t)(offset+1)]<<8;offset=(uint8_t)(offset+2);
            init->ManufacturerId=data[offset++];
            if(!init->ManufacturerId) return NFCSTATUS_FAILED;
            if(init->ManufacturerId) {
                if(phNciNfc_EnsureManufacturerBuffer(nci)!=NFCSTATUS_SUCCESS) return NFCSTATUS_INSUFFICIENT_RESOURCES;
                if(init->ManufacturerInfo.Buffer) {
                    memcpy(init->ManufacturerInfo.Buffer,data+offset,4);
                    init->ManufacturerInfo.Length=4;
                }
            }
            // Manufacturer bytes have been initialized before publishing capabilities.
            return phNciNfc_CoreIfSetMaxCtrlPacketSize(&nci->NciCoreContext,init->CntrlPktPayloadLen);
        }
    }
    return NFCSTATUS_FAILED;
}

NFCSTATUS phNciNfc_ProcessResetNtf(void* context,phNciNfc_Data_t* response)
{
    phNciNfc_Context_t* nci=context;
    if(!nci || !response || !response->pBuff) return NFCSTATUS_INVALID_PARAMETER;
    nci->InitRspParams.ManufacturerInfo.Length=0;
    if(response->wLen<9) return NFCSTATUS_FAILED;
    if(!response->pBuff[3] || response->pBuff[4]<4 || response->pBuff[4]!=response->wLen-5)
        return NFCSTATUS_FAILED;
    nci->InitRspParams.ManufacturerId=response->pBuff[3];
    if(nci->InitRspParams.ManufacturerId) {
        const uint8_t size=response->pBuff[4];
        if(phNciNfc_EnsureManufacturerBuffer(nci)!=NFCSTATUS_SUCCESS) return NFCSTATUS_INSUFFICIENT_RESOURCES;
        if(nci->InitRspParams.ManufacturerInfo.Buffer)
            memcpy(nci->InitRspParams.ManufacturerInfo.Buffer,response->pBuff+5,size);
        /* Publish the extent only after the allocation and copy succeed. */
        nci->InitRspParams.ManufacturerInfo.Length=size;
    }
    return NFCSTATUS_SUCCESS;
}
