/*
*          Modifications Copyright (c) Microsoft. All rights reserved.
*
*              Original code Copyright (c), NXP Semiconductors
*/

#include "phNciNfc_CorePch.h"

#include "phNciNfc_LoglConnMgmt.tmh"

static void phNciNfc_WaitCreditTimerCb(uint32_t TimerId, void *pContext);

static phNciNfc_LogConnMgmt_Int_t gphNciNfc_ConnMgmtInt;

NFCSTATUS
phNciNfc_LogConnMgmtInit()
{
    NFCSTATUS   status = NFCSTATUS_SUCCESS;

    PH_LOG_NCI_FUNC_ENTRY();

    phOsalNfc_SetMemory(&gphNciNfc_ConnMgmtInt, 0x00, sizeof(phNciNfc_LogConnMgmt_Int_t));

    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[CONNRFTYPE_STATIC].tConn.bConnId =  CONNRFTYPE_STATIC;
    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[CONNRFTYPE_STATIC].tConn.bMaxDpldSize = 0;
    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[CONNRFTYPE_STATIC].tConn.bNumCredits = 0;
    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[CONNRFTYPE_STATIC].bDestId = UNASSIGNED_DESTID;
    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[CONNRFTYPE_STATIC].bDestType = phNciNfc_e_UNKNOWN_DEST_TYPE;
    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[CONNRFTYPE_STATIC].bIfActive = FALSE;

    gphNciNfc_ConnMgmtInt.tConnInfo.bOpenConns = 1;

    PH_LOG_NCI_FUNC_EXIT();

    return status;
}

void
phNciNfc_LogConnMgmtDeInit()
{
    if(gphNciNfc_ConnMgmtInt.tCWTimerIf.TimerStatus == 1)
    {
        PH_LOG_NCI_INFO_STR("Stopping Credit await timer..");
        (void)phOsalNfc_Timer_Stop(gphNciNfc_ConnMgmtInt.tCWTimerIf.dwTimerId);
        (void)phOsalNfc_Timer_Delete(gphNciNfc_ConnMgmtInt.tCWTimerIf.dwTimerId);
    }

    phOsalNfc_SetMemory(&gphNciNfc_ConnMgmtInt, 0x00, sizeof(phNciNfc_LogConnMgmt_Int_t));
}

NFCSTATUS
phNciNfc_GetConnCredits(
                            uint8_t   bConnId,
                            uint8_t  *pCredits
                            )
{
    NFCSTATUS   wStatus = NFCSTATUS_FAILED;
    uint8_t     bConnListIdx;

    PH_LOG_NCI_FUNC_ENTRY();

    if(NULL == pCredits)
    {
        wStatus = PHNFCSTVAL(CID_NFC_NCI, NFCSTATUS_INVALID_PARAMETER);
        PH_LOG_NCI_INFO_STR(" Invalid Params..");
    }
    else
    {
        for(bConnListIdx = 0; bConnListIdx < (MAX_LOGICAL_CONNS+1) ; bConnListIdx++)
        {
            if(bConnId == (gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId) )
            {
                if((TRUE == (gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].bIfActive)) ||
                    (CONNRFTYPE_STATIC == (gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId)))
                {
                    *pCredits = (gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bNumCredits);
                    wStatus = NFCSTATUS_SUCCESS;
                }
                else
                {
                    PH_LOG_NCI_CRIT_STR("Interface not active for this connection");
                }
                break;
            }
        }
    }
    PH_LOG_NCI_FUNC_EXIT();

    return wStatus;
}

NFCSTATUS phNciNfc_RegForConnCredits(
                                     uint8_t bConnId,
                                     pphNciNfc_ConnCreditsNtf_t pNotify,
                                     void *pContext,
                                     uint32_t CreditTo
                                     )
{
    CreditTo = 1100; /* Original linked x64 routine ignores the supplied timeout. */
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint8_t     bConnListIdx;
    uint32_t TimerId;

    PH_LOG_NCI_FUNC_ENTRY();

    if((NULL == pNotify) || (NULL == pContext))
    {
        wStatus = PHNFCSTVAL(CID_NFC_NCI, NFCSTATUS_INVALID_PARAMETER);
        PH_LOG_NCI_INFO_STR(" Invalid Caller Layer Param(s) received ..");
    }
    else
    {
        for(bConnListIdx = 0; bConnListIdx < (MAX_LOGICAL_CONNS+1) ; bConnListIdx++)
        {
            if(bConnId == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId)
            {
                if((TRUE == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].bIfActive) ||
                    (CONNRFTYPE_STATIC == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId))
                {
                    wStatus = NFCSTATUS_SUCCESS;
                }
                else
                {
                    PH_LOG_NCI_CRIT_STR(" Interface not active for this connection");
                    wStatus = NFCSTATUS_FAILED;
                }
                break;
            }
        }

        if(NFCSTATUS_SUCCESS == wStatus)
        {
            gphNciNfc_ConnMgmtInt.bConnId = bConnId;
            gphNciNfc_ConnMgmtInt.pCrdtsNotify = pNotify;
            gphNciNfc_ConnMgmtInt.pContext = pContext;

            TimerId = phOsalNfc_Timer_Create();

            if (PH_OSALNFC_TIMER_ID_INVALID == TimerId)
            {
                PH_LOG_NCI_WARN_STR("Credit wait Timer Create failed!!");
                wStatus = NFCSTATUS_INSUFFICIENT_RESOURCES;
            }
            else
            {
                PH_LOG_NCI_INFO_STR("Credit wait Timer Created Successfully");

                gphNciNfc_ConnMgmtInt.tCWTimerIf.dwTimerId = TimerId;
                gphNciNfc_ConnMgmtInt.tCWTimerIf.TimerStatus = 0;

                PH_LOG_NCI_INFO_STR("Credit timeout set to %d, for ConnId 0x%x",CreditTo, bConnId);

                wStatus = phOsalNfc_Timer_Start(gphNciNfc_ConnMgmtInt.tCWTimerIf.dwTimerId,
                                                CreditTo,
                                                &phNciNfc_WaitCreditTimerCb,
                                                &gphNciNfc_ConnMgmtInt);

                if (NFCSTATUS_SUCCESS == wStatus)
                {
                    PH_LOG_NCI_INFO_STR("Credit wait timer started..");
                    gphNciNfc_ConnMgmtInt.tCWTimerIf.TimerStatus = 1;
                    /* Set the CreditsAwaited flag to indicate credit notif function to call the registered CB */
                    gphNciNfc_ConnMgmtInt.bCreditsAwaited = TRUE;
                }
                else
                {
                     PH_LOG_NCI_WARN_STR("Failed to start credit ntf wait timer!");
                     (void )phOsalNfc_Timer_Delete(gphNciNfc_ConnMgmtInt.tCWTimerIf.dwTimerId);
                     gphNciNfc_ConnMgmtInt.tCWTimerIf.dwTimerId = 0;
                     wStatus = NFCSTATUS_FAILED;
                }
            }
        }
    }

    PH_LOG_NCI_FUNC_EXIT();

    return wStatus;
}

NFCSTATUS
phNciNfc_GetConnMaxPldSz(
                           uint8_t   bConnId,
                           uint8_t *pMaxPldSz
                           )
{
    NFCSTATUS   wStatus = NFCSTATUS_FAILED;
    uint8_t     bConnListIdx;

    PH_LOG_NCI_FUNC_ENTRY();

    if(NULL == pMaxPldSz)
    {
        wStatus = PHNFCSTVAL(CID_NFC_NCI, NFCSTATUS_INVALID_PARAMETER);
        PH_LOG_NCI_INFO_STR(" Invalid Params..");
    }
    else
    {
        for(bConnListIdx = 0; bConnListIdx < (MAX_LOGICAL_CONNS+1) ; bConnListIdx++)
        {
            if(bConnId == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId)
            {
                if((TRUE == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].bIfActive) ||
                    (CONNRFTYPE_STATIC == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId))
                {
                    *pMaxPldSz = gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bMaxDpldSize;
                    wStatus = NFCSTATUS_SUCCESS;
                }
                else
                {
                    PH_LOG_NCI_INFO_STR(" Interface not active for this connection");
                }
                break;
            }
        }
    }
    PH_LOG_NCI_FUNC_EXIT();

    return wStatus;
}

NFCSTATUS phNciNfc_DecrConnCredit(
                                   uint8_t bConnId
                                   )
{
    NFCSTATUS   wStatus = NFCSTATUS_FAILED;
    uint8_t     bConnListIdx;

    PH_LOG_NCI_FUNC_ENTRY();

    for(bConnListIdx = 0; bConnListIdx < (MAX_LOGICAL_CONNS+1) ; bConnListIdx++)
    {
        if(bConnId == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId)
        {
            if((TRUE == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].bIfActive) ||
                (CONNRFTYPE_STATIC == gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bConnId))
            {
                if(gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bNumCredits > 0)
                {
                    gphNciNfc_ConnMgmtInt.tConnInfo.tConnList[bConnListIdx].tConn.bNumCredits -= 1;
                    wStatus = NFCSTATUS_SUCCESS;
                }
                else
                {
                    PH_LOG_NCI_WARN_STR("No Credits available for this connId!!");
                }
            }
            else
            {
                PH_LOG_NCI_CRIT_STR(" Interface not active for this connection");
            }
            break;
        }
    }
    PH_LOG_NCI_FUNC_EXIT();

    return wStatus;
}

static void phNciNfc_WaitCreditTimerCb(uint32_t TimerId, void *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_RF_TIMEOUT;
    phNciNfc_LogConnMgmt_Int_t *pConnMgmtCtxt = (phNciNfc_LogConnMgmt_Int_t *)pContext;

    PH_LOG_NCI_FUNC_ENTRY();

    if(NULL != pConnMgmtCtxt)
    {
        if(TimerId == pConnMgmtCtxt->tCWTimerIf.dwTimerId)
        {
            pConnMgmtCtxt->tCWTimerIf.TimerStatus = 0; /* Reset timer status flag */

            (void)phOsalNfc_Timer_Stop(pConnMgmtCtxt->tCWTimerIf.dwTimerId);
            (void)phOsalNfc_Timer_Delete(pConnMgmtCtxt->tCWTimerIf.dwTimerId);
            pConnMgmtCtxt->tCWTimerIf.dwTimerId = 0;
        }
        else
        {
            PH_LOG_NCI_CRIT_STR("Invalid wait credit timer ID");
            phOsalNfc_RaiseException(phOsalNfc_e_InternalErr, 1);
        }

        if(TRUE == pConnMgmtCtxt->bCreditsAwaited)
        {
            pConnMgmtCtxt->bCreditsAwaited = FALSE;
        }

        if (NULL != pConnMgmtCtxt->pCrdtsNotify)
        {
            (void)(pConnMgmtCtxt->pCrdtsNotify)(pConnMgmtCtxt->pContext,0,wStatus);
        }
    }
    PH_LOG_NCI_FUNC_EXIT();
    return;
}

