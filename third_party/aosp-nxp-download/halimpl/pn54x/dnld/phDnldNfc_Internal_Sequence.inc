/*
 * Copyright (C) 2010-2014 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Modified for this repository; notice added 2026-10-30.
 * Extracted/adapted from the pinned AOSP source for the Windows driver.
 * Local changes are listed in ../../../UPSTREAM.json and
 * licenses/README.md at the repository root. Original notices are retained.
 */

/* Selected normal sequence and receive validation; private TML handle bridge. */
#define PHDNLDNFC_RSP_TIMEOUT (2500)
#define PHDNLDNFC_CMDRESP_MAX_BUFF_SIZE (256)
static NFCSTATUS phDnldNfc_ProcessRecvInfo(void *pContext, phTmlNfc_TransactInfo_t *pInfo)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;

    if(NULL != pContext)
    {
        if (NULL == pInfo)
        {
            NXPLOG_FWDNLD_E("Invalid pInfo received from TML!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
        }
        else
        {
            wStatus = PHNFCSTATUS(pInfo->wStatus);

            if(NFCSTATUS_SUCCESS == wStatus)
            {
                NXPLOG_FWDNLD_D("Send Success");
            }else
            {
                NXPLOG_FWDNLD_E("Tml Write error!!");
                 wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
            }
        }
    }
    else
    {
        NXPLOG_FWDNLD_E("Invalid context received from TML!!");
         wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
    }

    return wStatus;
}

static void phDnldNfc_ProcessSeqState(void *pContext, phTmlNfc_TransactInfo_t *pInfo)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    NFCSTATUS wIntStatus;
    uint32_t TimerId;
    pphDnldNfc_DlContext_t pDlCtxt = (pphDnldNfc_DlContext_t)pContext;

    if(NULL == pDlCtxt)
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameter!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        switch(pDlCtxt->tCurrState)
        {
            case phDnldNfc_StateInit:
            {
                NXPLOG_FWDNLD_D("Initializing Sequence..");

                if(0 == (pDlCtxt->TimerInfo.dwRspTimerId))
                {
                    TimerId = phOsalNfc_Timer_Create();

                    if (0 == TimerId)
                    {
                        NXPLOG_FWDNLD_W("Response Timer Create failed!!");
                        wStatus = NFCSTATUS_INSUFFICIENT_RESOURCES;
                        pDlCtxt->wCmdSendStatus = wStatus;
                        break;
                    }
                    else
                    {
                        NXPLOG_FWDNLD_D("Response Timer Created Successfully");
                        (pDlCtxt->TimerInfo.dwRspTimerId) = TimerId;
                        (pDlCtxt->TimerInfo.TimerStatus) = 0;
                        (pDlCtxt->TimerInfo.wTimerExpStatus) = 0;
                    }
                }
                pDlCtxt->tCurrState = phDnldNfc_StateSend;
            }
            case phDnldNfc_StateSend:
            {
                wStatus = phDnldNfc_BuildFramePkt(pDlCtxt);

                if(NFCSTATUS_SUCCESS == wStatus)
                {
                    pDlCtxt->tCurrState = phDnldNfc_StateRecv;

                    wStatus = phTmlNfc_Write(pDlCtxt->pHwRef, (pDlCtxt->tCmdRspFrameInfo.aFrameBuff),
                        (uint16_t)(pDlCtxt->tCmdRspFrameInfo.dwSendlength),
                                    (pphTmlNfc_TransactCompletionCb_t)&phDnldNfc_ProcessSeqState,
                                    pDlCtxt);
                }
                pDlCtxt->wCmdSendStatus = wStatus;
                break;
            }
            case phDnldNfc_StateRecv:
            {
                wStatus = phDnldNfc_ProcessRecvInfo(pContext,pInfo);

                if(NFCSTATUS_SUCCESS == wStatus)
                {
                    wStatus = phOsalNfc_Timer_Start((pDlCtxt->TimerInfo.dwRspTimerId),
                                                PHDNLDNFC_RSP_TIMEOUT,
                                                &phDnldNfc_RspTimeOutCb,
                                                pDlCtxt);

                    if (NFCSTATUS_SUCCESS == wStatus)
                    {
                        NXPLOG_FWDNLD_D("Response timer started");
                        pDlCtxt->TimerInfo.TimerStatus = 1;
                        pDlCtxt->tCurrState = phDnldNfc_StateTimer;
                    }
                    else
                    {
                         NXPLOG_FWDNLD_W("Response timer not started");
                        pDlCtxt->tCurrState = phDnldNfc_StateResponse;
                    }
                    /* Call TML_Read function and register the call back function */
                    wStatus = phTmlNfc_Read(pDlCtxt->pHwRef,
                        pDlCtxt->tCmdRspFrameInfo.aFrameBuff,
                        (uint16_t )PHDNLDNFC_CMDRESP_MAX_BUFF_SIZE,
                        (pphTmlNfc_TransactCompletionCb_t)&phDnldNfc_ProcessSeqState,
                        (void *)pDlCtxt);

                    break;
                }
                else
                {
                    /* Setting TimerExpStatus below to avoid frame processing in response state */
                    (pDlCtxt->TimerInfo.wTimerExpStatus) = NFCSTATUS_RF_TIMEOUT;
                    pDlCtxt->tCurrState = phDnldNfc_StateResponse;
                }
            }
            case phDnldNfc_StateTimer:
            {
                if (1 == (pDlCtxt->TimerInfo.TimerStatus)) /*Is Timer Running*/
                {
                    /*Stop Timer*/
                    (void)phOsalNfc_Timer_Stop(pDlCtxt->TimerInfo.dwRspTimerId);
                    (pDlCtxt->TimerInfo.TimerStatus) = 0; /*timer stopped*/
                }
                pDlCtxt->tCurrState = phDnldNfc_StateResponse;
            }
            case phDnldNfc_StateResponse:
            {
                if(NFCSTATUS_RF_TIMEOUT != (pDlCtxt->TimerInfo.wTimerExpStatus))
                {
                    /* Process response */
                    wStatus = phDnldNfc_ProcessFrame(pContext,pInfo);
                }
                else
                {
                    if(phDnldNfc_EventReset != pDlCtxt->tCurrEvent)
                    {
                        wStatus = (pDlCtxt->TimerInfo.wTimerExpStatus);
                    }
                    else
                    {
                        wStatus = NFCSTATUS_SUCCESS;
                    }
                    (pDlCtxt->TimerInfo.wTimerExpStatus) = 0;
                }

                /* Abort TML read operation which is always kept open */
                wIntStatus  = phTmlNfc_ReadAbort(pDlCtxt->pHwRef);

                if(NFCSTATUS_SUCCESS != wIntStatus)
                {
                    /* TODO:-Action to take in this case:-Tml read abort failed!? */
                    NXPLOG_FWDNLD_W("Tml Read Abort failed!!");
                }

                pDlCtxt->tCurrEvent = phDnldNfc_EventInvalid;
                pDlCtxt->tDnldInProgress = phDnldNfc_TransitionIdle;
                pDlCtxt->tCurrState = phDnldNfc_StateInit;

                /* Delete the timer & reset timer primitives in context */
                (void)phOsalNfc_Timer_Delete(pDlCtxt->TimerInfo.dwRspTimerId);
                (pDlCtxt->TimerInfo.dwRspTimerId) = 0;
                (pDlCtxt->TimerInfo.TimerStatus) = 0;
                (pDlCtxt->TimerInfo.wTimerExpStatus) = 0;

                if((NULL != (pDlCtxt->UserCb)) && (NULL != (pDlCtxt->UserCtxt)))
                {
                    pDlCtxt->UserCb((pDlCtxt->UserCtxt),wStatus,&(pDlCtxt->tRspBuffInfo));
                }
                break;
            }
            default:
            {
                pDlCtxt->tCurrEvent = phDnldNfc_EventInvalid;
                pDlCtxt->tDnldInProgress = phDnldNfc_TransitionIdle;
                break;
            }
        }
    }

    return;
}
