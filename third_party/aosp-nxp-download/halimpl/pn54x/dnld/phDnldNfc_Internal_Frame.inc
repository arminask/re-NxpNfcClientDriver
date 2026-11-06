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
/* Selected audited frame envelope helpers. */

#define PHDNLDNFC_MIN_PLD_LEN (4U)
#define PHDNLDNFC_CMDRESP_MAX_PLD_SIZE (252U)
#define PHDNLDNFC_FRAME_HDR_OFFSET (0)
#define PHDNLDNFC_FRAME_HDR_LEN (2U)
#define PHDNLDNFC_FRAME_CRC_LEN (2U)
#define PHDNLDNFC_FRAMEID_OFFSET (2)
#define PHDNLDNFC_SET_HDR_FRAGBIT(n) ((n) | (1<<10))
#define PHDNLDNFC_CLR_HDR_FRAGBIT(n) ((n) & ~(1U<<10))
static NFCSTATUS phDnldNfc_BuildFramePkt(pphDnldNfc_DlContext_t pDlContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint16_t wFrameLen = 0;
    uint16_t wCrcVal;
    uint8_t *pFrameByte;

    if(NULL == pDlContext)
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameter!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_FTWrite == (pDlContext->FrameInp.Type))
        {
            if((0 == (pDlContext->tUserData.wLen)) || (NULL == (pDlContext->tUserData.pBuff)))
            {
                NXPLOG_FWDNLD_E("Invalid Input Parameter(s) for Write!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
            }
            else
            {
                if(TRUE == (pDlContext->tRWInfo.bFirstWrReq))
                {
                    (pDlContext->tRWInfo.wRemBytes) = (pDlContext->tUserData.wLen);
                    (pDlContext->tRWInfo.wOffset) = 0;
                }
            }
        }
        else if(phDnldNfc_FTRead == (pDlContext->FrameInp.Type))
        {
            if((0 == (pDlContext->tRspBuffInfo.wLen)) || (NULL == (pDlContext->tRspBuffInfo.pBuff)))
            {
                NXPLOG_FWDNLD_E("Invalid Input Parameter(s) for Read!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
            }
            else
            {
                if(FALSE == (pDlContext->tRWInfo.bFramesSegmented))
                {
                    NXPLOG_FWDNLD_D("Verifying RspBuffInfo for Read Request..");
                    wFrameLen = (pDlContext->tRspBuffInfo.wLen) + PHDNLDNFC_MIN_PLD_LEN;

                    (pDlContext->tRWInfo.wRWPldSize) = (PHDNLDNFC_CMDRESP_MAX_PLD_SIZE - PHDNLDNFC_MIN_PLD_LEN);
                    (pDlContext->tRWInfo.wRemBytes) = (pDlContext->tRspBuffInfo.wLen);
                    (pDlContext->tRWInfo.dwAddr) = (pDlContext->FrameInp.dwAddr);
                    (pDlContext->tRWInfo.wOffset) = 0;
                    (pDlContext->tRWInfo.wBytesRead) = 0;

                    if(PHDNLDNFC_CMDRESP_MAX_PLD_SIZE < wFrameLen)
                    {
                        (pDlContext->tRWInfo.bFramesSegmented) = TRUE;
                    }
                }
            }
        }
        else if(phDnldNfc_FTLog == (pDlContext->FrameInp.Type))
        {
            if((0 == (pDlContext->tUserData.wLen)) || (NULL == (pDlContext->tUserData.pBuff)))
            {
                NXPLOG_FWDNLD_E("Invalid Input Parameter(s) for Log!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
            }
        }
        else
        {
        }

        if(NFCSTATUS_SUCCESS == wStatus)
        {
            wStatus = phDnldNfc_CreateFramePld(pDlContext);
        }

        if(NFCSTATUS_SUCCESS == wStatus)
        {
            wFrameLen = 0;
            wFrameLen  = (pDlContext->tCmdRspFrameInfo.dwSendlength);

            if(phDnldNfc_FTRaw != (pDlContext->FrameInp.Type))
            {
                if(phDnldNfc_FTWrite != (pDlContext->FrameInp.Type))
                {
                    pFrameByte = (uint8_t *)&wFrameLen;

                    pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAME_HDR_OFFSET] = pFrameByte[1];
                    pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAME_HDR_OFFSET + 1] = pFrameByte[0];

                    NXPLOG_FWDNLD_D("Inserting FrameId ..");
                    pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAMEID_OFFSET] =
                        (pDlContext->tCmdId);

                    wFrameLen += PHDNLDNFC_FRAME_HDR_LEN;
                }
                else
                {
                    if(0 != (pDlContext->tRWInfo.wRWPldSize))
                    {
                        if(TRUE == (pDlContext->tRWInfo.bFramesSegmented))
                        {
                            /* Turning ON the Fragmentation bit in FrameLen */
                            wFrameLen = PHDNLDNFC_SET_HDR_FRAGBIT(wFrameLen);
                        }

                        pFrameByte = (uint8_t *)&wFrameLen;

                        pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAME_HDR_OFFSET] = pFrameByte[1];
                        pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAME_HDR_OFFSET + 1] = pFrameByte[0];

                        /* To ensure we have no frag bit set for crc calculation */
                        wFrameLen = PHDNLDNFC_CLR_HDR_FRAGBIT(wFrameLen);

                        wFrameLen += PHDNLDNFC_FRAME_HDR_LEN;
                    }
                }

                /* calculate CRC16 */
                wCrcVal = phDnldNfc_CalcCrc16((pDlContext->tCmdRspFrameInfo.aFrameBuff),wFrameLen);

                pFrameByte = (uint8_t *)&wCrcVal;

                /* Insert the computed Crc value */
                pDlContext->tCmdRspFrameInfo.aFrameBuff[wFrameLen] = pFrameByte[1];
                pDlContext->tCmdRspFrameInfo.aFrameBuff[wFrameLen+ 1] = pFrameByte[0];

                wFrameLen += PHDNLDNFC_FRAME_CRC_LEN;
            }

            (pDlContext->tCmdRspFrameInfo.dwSendlength) = wFrameLen;
            NXPLOG_FWDNLD_D("Frame created successfully");
        }
        else
        {
            NXPLOG_FWDNLD_E("Frame creation failed!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
        }
    }

    return wStatus;
}

static NFCSTATUS phDnldNfc_ProcessFrame(void *pContext, phTmlNfc_TransactInfo_t *pInfo)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint16_t wCrcVal,wRecvdCrc,wRecvdLen,wPldLen;
    pphDnldNfc_DlContext_t pDlCtxt = (pphDnldNfc_DlContext_t)pContext;

    if((NULL == pDlCtxt) ||
       (NULL == pInfo)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if((PH_DL_STATUS_OK != pInfo->wStatus) ||
            (pInfo->wLength < (pDlCtxt->FrameInp.Type==phDnldNfc_FTRaw?1:5)) ||
            (NULL == pInfo->pBuff))
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request Failed!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
        }
        else
        {
            if(phDnldNfc_FTRaw == (pDlCtxt->FrameInp.Type))
            {
              if((0 != (pDlCtxt->tRspBuffInfo.wLen)) &&
                    (NULL != (pDlCtxt->tRspBuffInfo.pBuff)))
              {
                  if(pInfo->wLength>pDlCtxt->wRspCapacity) return NFCSTATUS_BUFFER_TOO_SMALL;
                  memcpy((pDlCtxt->tRspBuffInfo.pBuff),(pInfo->pBuff),(pInfo->wLength));

                  (pDlCtxt->tRspBuffInfo.wLen) = (pInfo->wLength);
              }
              else
              {
                  NXPLOG_FWDNLD_E("Cannot update Response buff with received data!!");
              }
            }
            else
            {
                /* calculate CRC16 */
                wCrcVal = phDnldNfc_CalcCrc16((pInfo->pBuff),((pInfo->wLength) - PHDNLDNFC_FRAME_CRC_LEN));

                wRecvdCrc = 0;
                wRecvdCrc = (((uint16_t)(pInfo->pBuff[(pInfo->wLength) - 2]) << 8U) |
                               (pInfo->pBuff[(pInfo->wLength) - 1]));

                if(wRecvdCrc == wCrcVal)
                {
                    wRecvdLen = (((uint16_t)(pInfo->pBuff[PHDNLDNFC_FRAME_HDR_OFFSET]) << 8U) |
                               (pInfo->pBuff[PHDNLDNFC_FRAME_HDR_OFFSET + 1]));

                    wPldLen = ((pInfo->wLength) - (PHDNLDNFC_FRAME_HDR_LEN + PHDNLDNFC_FRAME_CRC_LEN));

                    if(wRecvdLen != wPldLen)
                    {
                        NXPLOG_FWDNLD_E("Invalid frame payload length received");
                        wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
                    }
                    else
                    {
                        wStatus = phDnldNfc_UpdateRsp(pDlCtxt,pInfo,(wPldLen - 1));
                    }
                }
                else
                {
                    NXPLOG_FWDNLD_E("Invalid frame received");
                    wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_FAILED);
                }
            }
        }
    }

    return wStatus;
}
