/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

/* Selected published C bodies; see evidence/RF-RAW-CONFIG.md. */
NFCSTATUS phNciNfc_SetConfigRaw(
                            void*                      pNciHandle,
                            uint8_t *pBuff, uint16_t Length,
                            pphNciNfc_IfNotificationCb_t pConfigRfNotifyCb,
                            void*                      pContext)
{
    NFCSTATUS               wStatus = NFCSTATUS_INVALID_PARAMETER;
    pphNciNfc_Context_t     pNciContext = (pphNciNfc_Context_t )pNciHandle;
    if((NULL == pNciHandle) || (pNciHandle != gpphNciNfc_Context))
    {
        wStatus = NFCSTATUS_NOT_INITIALISED;
    }
    else if((NULL != pBuff) && (0 != Length))
    {
        uint8_t *payload = (uint8_t *) phOsalNfc_GetMemory((uint32_t)Length);
        /* Local hardening: preserve context and ownership on allocation failure. */
        if(NULL == payload)
        {
            return NFCSTATUS_INSUFFICIENT_RESOURCES;
        }
        pNciContext->tSendPayload.pBuff = payload;
        phOsalNfc_MemCopy(pNciContext->tSendPayload.pBuff, pBuff,Length);
        /* Input parameters are valid and payload field has been framed successfully */
        pNciContext->IfNtf = pConfigRfNotifyCb;
        pNciContext->IfNtfCtx = pContext;
        PHNCINFC_INIT_SEQUENCE(pNciContext,gphNciNfc_SetConfigSequence);
        pNciContext->tSendPayload.wPayloadSize = Length;
        wStatus = phNciNfc_GenericSequence(pNciContext, NULL, NFCSTATUS_SUCCESS);
        if(NFCSTATUS_PENDING != wStatus)
        {
        }
    }
    else
    {
    }
    return wStatus;
}

NFCSTATUS phNciNfc_GetConfigRaw(
                            void*                      pNciHandle,
                            uint8_t *pBuff, uint16_t Length,
                            pphNciNfc_IfNotificationCb_t pConfigRfNotifyCb,
                            void*                      pContext)
{

    NFCSTATUS               wStatus = NFCSTATUS_INVALID_PARAMETER;
    pphNciNfc_Context_t     pNciContext = (pphNciNfc_Context_t )pNciHandle;
    if((NULL == pNciHandle) || (pNciHandle != gpphNciNfc_Context))
    {
        wStatus = NFCSTATUS_NOT_INITIALISED;
    }
    else if((NULL != pBuff) && (0 != Length))
    {
        uint8_t *payload = (uint8_t *) phOsalNfc_GetMemory((uint32_t)Length);
        /* Local hardening: preserve context and ownership on allocation failure. */
        if(NULL == payload)
        {
            return NFCSTATUS_INSUFFICIENT_RESOURCES;
        }
        pNciContext->tSendPayload.pBuff = payload;
        phOsalNfc_MemCopy(pNciContext->tSendPayload.pBuff, pBuff,Length);
        /* Input parameters are valid and payload field has been framed successfully */
        pNciContext->IfNtf = pConfigRfNotifyCb;
        pNciContext->IfNtfCtx = pContext;
        PHNCINFC_INIT_SEQUENCE(pNciContext,gphNciNfc_GetConfigRawSequence);

        pNciContext->tSendPayload.wPayloadSize = Length;
        wStatus = phNciNfc_GenericSequence(pNciContext, NULL, NFCSTATUS_SUCCESS);
        if(NFCSTATUS_PENDING != wStatus)
        {
        }
    }
    else
    {
    }
    return wStatus;
}

