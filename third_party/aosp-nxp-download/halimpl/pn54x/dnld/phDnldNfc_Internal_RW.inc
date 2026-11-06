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

/* Selected read/write sequence; private TML handle bridge. */
static void phDnldNfc_ProcessRWSeqState(void *pContext, phTmlNfc_TransactInfo_t *pInfo)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    NFCSTATUS wIntStatus = wStatus;
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
                if(0 == (pDlCtxt->TimerInfo.dwRspTimerId))
                {
                    TimerId = phOsalNfc_Timer_Create();

                    if (0 == TimerId)
                    {
                        NXPLOG_FWDNLD_E("Response Timer Create failed!!");
                        wStatus = NFCSTATUS_INSUFFICIENT_RESOURCES;
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
                if(FALSE == pDlCtxt->bResendLastFrame)
                {
                    wStatus = phDnldNfc_BuildFramePkt(pDlCtxt);
                }
                else
                {
                    pDlCtxt->bResendLastFrame = FALSE;
                }

                if(NFCSTATUS_SUCCESS == wStatus)
                {
                    pDlCtxt->tCurrState = phDnldNfc_StateRecv;

                    wStatus = phTmlNfc_Write(pDlCtxt->pHwRef, (pDlCtxt->tCmdRspFrameInfo.aFrameBuff),
                        (uint16_t)(pDlCtxt->tCmdRspFrameInfo.dwSendlength),
                                    (pphTmlNfc_TransactCompletionCb_t)&phDnldNfc_ProcessRWSeqState,
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
                    /* processing For Pipelined write before calling timer below */
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
                        /* Todo:- diagnostic in this case */
                    }
                    /* Call TML_Read function and register the call back function */
                    wStatus = phTmlNfc_Read(pDlCtxt->pHwRef,
                        pDlCtxt->tCmdRspFrameInfo.aFrameBuff,
                        (uint16_t )PHDNLDNFC_CMDRESP_MAX_BUFF_SIZE,
                        (pphTmlNfc_TransactCompletionCb_t)&phDnldNfc_ProcessRWSeqState,
                        (void *)pDlCtxt);

                    break;
                }
                else
                {
                    /* Setting TimerExpStatus below to avoid frame processing in reponse state */
                    (pDlCtxt->TimerInfo.wTimerExpStatus) = NFCSTATUS_RF_TIMEOUT;
                    pDlCtxt->tCurrState = phDnldNfc_StateResponse;
                }
            }
            case phDnldNfc_StateTimer:
            {
                if (1 == (pDlCtxt->TimerInfo.TimerStatus)) /*Is Timer Running*/
                {
                    /* Stop Timer */
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

                    if(NFCSTATUS_BUSY == wStatus)
                    {
                        /* store the status for use in subsequent processing */
                        wIntStatus = wStatus;

                        /* setup the resend wait timer */
                        wStatus = phDnldNfc_SetupResendTimer(pDlCtxt);

                        if(NFCSTATUS_SUCCESS == wStatus)
                        {
                            /* restore the last mem_bsy status to avoid re-building frame below */
                            wStatus = wIntStatus;
                        }
                    }
                }
                else
                {
                    wStatus = (pDlCtxt->TimerInfo.wTimerExpStatus);
                    (pDlCtxt->TimerInfo.wTimerExpStatus) = 0;
                }

                if((0 != (pDlCtxt->tRWInfo.wRemBytes)) && (NFCSTATUS_SUCCESS == wStatus))
                {
                    /* Abort TML read operation which is always kept open */
                    wIntStatus  = phTmlNfc_ReadAbort(pDlCtxt->pHwRef);

                    if(NFCSTATUS_SUCCESS != wIntStatus)
                    {
                         NXPLOG_FWDNLD_W("Tml read abort failed!");
                    }

                    wStatus = phDnldNfc_BuildFramePkt(pDlCtxt);

                    if(NFCSTATUS_SUCCESS == wStatus)
                    {
                        pDlCtxt->tCurrState = phDnldNfc_StateRecv;
                        wStatus = phTmlNfc_Write(pDlCtxt->pHwRef, (pDlCtxt->tCmdRspFrameInfo.aFrameBuff),
                            (uint16_t)(pDlCtxt->tCmdRspFrameInfo.dwSendlength),
                                        (pphTmlNfc_TransactCompletionCb_t)&phDnldNfc_ProcessRWSeqState,
                                        pDlCtxt);

                        /* TODO:- Verify here if TML_Write returned NFC_PENDING status & take appropriate
                              action otherwise ?? */
                    }
                }
                else if(NFCSTATUS_BUSY == wStatus)
                {
                    /* No processing to be done,since resend wait timer should have already been started */
                }
                else
                {
                    (pDlCtxt->tRWInfo.bFramesSegmented) = FALSE;
                    /* Abort TML read operation which is always kept open */
                    wIntStatus  = phTmlNfc_ReadAbort(pDlCtxt->pHwRef);

                    if(NFCSTATUS_SUCCESS != wIntStatus)
                    {
                         NXPLOG_FWDNLD_W("Tml read abort failed!");
                    }

                    pDlCtxt->tCurrEvent = phDnldNfc_EventInvalid;
                    pDlCtxt->tDnldInProgress = phDnldNfc_TransitionIdle;
                    pDlCtxt->tCurrState = phDnldNfc_StateInit;
                    pDlCtxt->bResendLastFrame = FALSE;

                    /* Delete the timer & reset timer primitives in context */
                    (void)phOsalNfc_Timer_Delete(pDlCtxt->TimerInfo.dwRspTimerId);
                    (pDlCtxt->TimerInfo.dwRspTimerId) = 0;
                    (pDlCtxt->TimerInfo.TimerStatus) = 0;
                    (pDlCtxt->TimerInfo.wTimerExpStatus) = 0;

                    if((NULL != (pDlCtxt->UserCb)) && (NULL != (pDlCtxt->UserCtxt)))
                    {
                        pDlCtxt->UserCb((pDlCtxt->UserCtxt),wStatus,&(pDlCtxt->tRspBuffInfo));
                    }
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
