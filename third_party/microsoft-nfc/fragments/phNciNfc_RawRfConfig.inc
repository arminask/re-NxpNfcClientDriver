/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

/* Selected published C bodies; see evidence/RF-RAW-CONFIG.md. */
NFCSTATUS phNciNfc_GetConfig(void *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_INVALID_PARAMETER;
    phNciNfc_CoreTxInfo_t tTxInfo;
    pphNciNfc_Context_t pNciCtx = (pphNciNfc_Context_t) pContext;
    if(NULL != pNciCtx)
    {
        /* Clear the Tx information structure */
        phOsalNfc_SetMemory(&tTxInfo, 0x00, sizeof(phNciNfc_CoreTxInfo_t));

        tTxInfo.tHeaderInfo.eMsgType = phNciNfc_e_NciCoreMsgTypeCntrlCmd;
        tTxInfo.tHeaderInfo.Group_ID = phNciNfc_e_CoreNciCoreGid;
        tTxInfo.tHeaderInfo.Opcode_ID.OidType.NciCoreCmdOid = phNciNfc_e_NciCoreGetConfigCmdOid;

        tTxInfo.Buff = pNciCtx->tSendPayload.pBuff;
        tTxInfo.wLen = pNciCtx->tSendPayload.wPayloadSize;

        /* Sending command */
        wStatus = phNciNfc_CoreIfTxRx(&(pNciCtx->NciCoreContext), &tTxInfo,
                        &(pNciCtx->RspBuffInfo), 2000,
                        (pphNciNfc_CoreIfNtf_t)&phNciNfc_GenericSequence,
                        (void *)pNciCtx);
    }
    else
    {
        wStatus = NFCSTATUS_FAILED;
    }
    return wStatus;
}

NFCSTATUS phNciNfc_GetConfigRawRsp(void *pContext, NFCSTATUS wStatus)
{
    NFCSTATUS wGetStatus = wStatus;
    pphNciNfc_Context_t pNciContext = pContext;
    if( (NULL != pNciContext) && (NULL != pNciContext->RspBuffInfo.pBuff) &&\
                    (NFCSTATUS_SUCCESS == wStatus) )
    {
        if(pNciContext->RspBuffInfo.wLen<2 || pNciContext->RspBuffInfo.pBuff[0]!=0)
            wGetStatus=NFCSTATUS_FAILED;
    }
    return wGetStatus;
}

NFCSTATUS phNciNfc_CompleteGetConfigRaw(void *pContext,NFCSTATUS wStatus)
{
    NFCSTATUS wGetStatus = wStatus;
    pphNciNfc_Context_t pNciContext = pContext;
    uint8_t aGetConfig[255] = {0};
    if(NULL != pNciContext)
    {
        if(NULL != pNciContext->tSendPayload.pBuff)
        {
            phOsalNfc_FreeMemory(pNciContext->tSendPayload.pBuff);
            pNciContext->tSendPayload.pBuff = NULL;
            pNciContext->tSendPayload.wPayloadSize = 0;
        }
        if(NFCSTATUS_SUCCESS == wStatus)
        {
            if(!pNciContext->RspBuffInfo.pBuff || pNciContext->RspBuffInfo.wLen<2 ||
               pNciContext->RspBuffInfo.wLen > (ARRAYSIZE(aGetConfig) - 1))
            {
                wGetStatus = NFCSTATUS_BUFFER_TOO_SMALL;
                /* Local hardening: terminal size failure must release the pending notification. */
                phNciNfc_Notify(pNciContext, wGetStatus, (void *)aGetConfig);
                goto Done;
            }

            aGetConfig[0] = (uint8_t)pNciContext->RspBuffInfo.wLen;
            if(aGetConfig[0] > 0)
            {
                phOsalNfc_MemCopy(&aGetConfig[1],pNciContext->RspBuffInfo.pBuff,\
                    pNciContext->RspBuffInfo.wLen);
            }
        }
        phNciNfc_Notify(pNciContext, wStatus, (void *)aGetConfig);
    }

Done:
    return wGetStatus;
}

