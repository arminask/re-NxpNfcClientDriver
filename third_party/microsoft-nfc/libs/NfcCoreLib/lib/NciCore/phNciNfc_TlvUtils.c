/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

#include "phNciNfc_CorePch.h"

#include "phNciNfc_TlvUtils.tmh"

uint8_t
phNciNfc_TlvUtilsAddTlv(phNciNfc_TlvUtilInfo_t * pttlvInfo,
                        uint8_t btag,
                        uint8_t blength,
                        uint8_t *pdataBuffer,
                        uint8_t bPropId2)
{
    uint32_t offset,required,valueLength;
    if(!pttlvInfo || !pttlvInfo->pBuffer) return NFCSTATUS_INVALID_PARAMETER;
    offset=pttlvInfo->sizeInfo.dwpacketSize;
    if(offset>pttlvInfo->dwLength ||
       (btag==PHNCINFC_TLVUTIL_NXP_PROP_ID1 && !blength)) return NFCSTATUS_INVALID_PARAMETER;
    required=(uint32_t)blength+PHNCINFC_TLVUTIL_DEFAULT_LEN;
    valueLength=blength-(btag==PHNCINFC_TLVUTIL_NXP_PROP_ID1?1u:0u);
    if(required>pttlvInfo->dwLength-offset) return NFCSTATUS_BUFFER_TOO_SMALL;
    if(valueLength && !pdataBuffer) return NFCSTATUS_INVALID_PARAMETER;
    pttlvInfo->pBuffer[offset++]=btag;
    if(btag==PHNCINFC_TLVUTIL_NXP_PROP_ID1) pttlvInfo->pBuffer[offset++]=bPropId2;
    pttlvInfo->pBuffer[offset++]=(uint8_t)valueLength;
    if(valueLength) phOsalNfc_MemCopy(pttlvInfo->pBuffer+offset,pdataBuffer,valueLength);
    pttlvInfo->sizeInfo.dwpacketSize+=required;
    return NFCSTATUS_SUCCESS;
}
