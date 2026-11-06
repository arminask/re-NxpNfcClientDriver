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

/* Selected audited subset; original private C TU supplies declarations. */
#define PHDNLDNFC_RETRY_FRAME_WRITE (50)
#define PH_LIBNFC_VEN_RESET_ON_DOWNLOAD_TIMEOUT (0)
NFCSTATUS phDnldNfc_CmdHandler(void *pContext, phDnldNfc_Event_t TrigEvent)
{
    NFCSTATUS  status = NFCSTATUS_SUCCESS;
    pphDnldNfc_DlContext_t  pDlCtxt = (pphDnldNfc_DlContext_t)pContext;

    if(NULL == pDlCtxt)
    {
      NXPLOG_FWDNLD_E("Invalid Input Parameter!!");
      status = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        switch(TrigEvent)
        {
            case phDnldNfc_EventReset:
            case phDnldNfc_EventGetVer:
            case phDnldNfc_EventIntegChk:
            case phDnldNfc_EventGetSesnSt:
            case phDnldNfc_EventRaw:
            {
                if(phDnldNfc_EventInvalid == (pDlCtxt->tCurrEvent))
                {
                    NXPLOG_FWDNLD_D("Processing Normal Sequence..");
                    pDlCtxt->tCurrEvent = TrigEvent;
                    pDlCtxt->tDnldInProgress = phDnldNfc_TransitionBusy;

                    phDnldNfc_ProcessSeqState(pDlCtxt,NULL);

                    status = pDlCtxt->wCmdSendStatus;
                }
                else
                {
                    NXPLOG_FWDNLD_E("Prev Norml Sequence not completed/restored!!");
                    status = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
                }
                break;
            }
            case phDnldNfc_EventWrite:
            case phDnldNfc_EventRead:
            case phDnldNfc_EventLog:
            case phDnldNfc_EventForce:
            {
                if(phDnldNfc_EventInvalid == (pDlCtxt->tCurrEvent))
                {
                    NXPLOG_FWDNLD_D("Processing R/W Sequence..");
                    pDlCtxt->tCurrEvent = TrigEvent;
                    pDlCtxt->tDnldInProgress = phDnldNfc_TransitionBusy;

                    phDnldNfc_ProcessRWSeqState(pDlCtxt,NULL);

                    status = pDlCtxt->wCmdSendStatus;
                }
                else
                {
                    NXPLOG_FWDNLD_E("Prev R/W Sequence not completed/restored!!");
                    status = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
                }
                break;
            }
            default:
            {
                /* Unknown Event */
                NXPLOG_FWDNLD_E("Unknown Event Parameter!!");
                status = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
                break;
            }
        }
    }

    return status;
}

static void phDnldNfc_RspTimeOutCb(uint32_t TimerId, void *pContext)
{
    pphDnldNfc_DlContext_t pDlCtxt = (pphDnldNfc_DlContext_t)pContext;

    if (NULL != pDlCtxt)
    {
        UNUSED(TimerId);

        if(1 == pDlCtxt->TimerInfo.TimerStatus)
        {
            /* No response received and the timer expired */
            pDlCtxt->TimerInfo.TimerStatus = 0;    /* Reset timer status flag */

            NXPLOG_FWDNLD_D("%x",pDlCtxt->tLastStatus);

#if PH_LIBNFC_VEN_RESET_ON_DOWNLOAD_TIMEOUT
            if ( PH_DL_STATUS_SIGNATURE_ERROR  == pDlCtxt->tLastStatus ) {
                /* Do a VEN Reset of the chip. */
                NXPLOG_FWDNLD_E("Performing a VEN Reset");
                phTmlNfc_IoCtl(phTmlNfc_e_EnableNormalMode);
                phTmlNfc_IoCtl(phTmlNfc_e_EnableDownloadMode);
                NXPLOG_FWDNLD_E("VEN Reset Done");
            }
#endif


            (pDlCtxt->TimerInfo.wTimerExpStatus) = NFCSTATUS_RF_TIMEOUT;

            if((phDnldNfc_EventRead == pDlCtxt->tCurrEvent) || (phDnldNfc_EventWrite == pDlCtxt->tCurrEvent))
            {
                phDnldNfc_ProcessRWSeqState(pDlCtxt,NULL);
            }
            else
            {
                phDnldNfc_ProcessSeqState(pDlCtxt,NULL);
            }
        }
    }

    return;
}

static void phDnldNfc_ResendTimeOutCb(uint32_t TimerId, void *pContext)
{
    pphDnldNfc_DlContext_t pDlCtxt = (pphDnldNfc_DlContext_t)pContext;

    if (NULL != pDlCtxt)
    {
        UNUSED(TimerId);

        if(1 == pDlCtxt->TimerInfo.TimerStatus)
        {
            /* No response received and the timer expired */
            pDlCtxt->TimerInfo.TimerStatus = 0;    /* Reset timer status flag */

            (pDlCtxt->TimerInfo.wTimerExpStatus) = 0;

             pDlCtxt->tCurrState = phDnldNfc_StateSend;

             /* set the flag to trigger last frame re-transmission */
             pDlCtxt->bResendLastFrame = TRUE;

             phDnldNfc_ProcessRWSeqState(pDlCtxt,NULL);
        }
    }

    return;
}

static NFCSTATUS phDnldNfc_SetupResendTimer(pphDnldNfc_DlContext_t pDlContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;

    wStatus = phOsalNfc_Timer_Start((pDlContext->TimerInfo.dwRspTimerId),
                                                PHDNLDNFC_RETRY_FRAME_WRITE,
                                                &phDnldNfc_ResendTimeOutCb,
                                                pDlContext);

    if(NFCSTATUS_SUCCESS == wStatus)
    {
        NXPLOG_FWDNLD_D("Frame Resend wait timer started");
        (pDlContext->TimerInfo.TimerStatus) = 1;
        pDlContext->tCurrState = phDnldNfc_StateTimer;
    }
    else
    {
         NXPLOG_FWDNLD_W("Frame Resend wait timer not started");
         (pDlContext->TimerInfo.TimerStatus) = 0;/*timer stopped*/
         pDlContext->tCurrState = phDnldNfc_StateResponse;
        /* Todo:- diagnostic in this case */
    }

    return wStatus;
}
