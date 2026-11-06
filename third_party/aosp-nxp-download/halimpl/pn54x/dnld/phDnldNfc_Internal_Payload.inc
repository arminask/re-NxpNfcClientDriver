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
/* Audited payload/response subset; private build constants. */

#define PHDNLDNFC_FRAME_RDDATA_OFFSET (6)
#define PHDNLDNFC_PLD_OFFSET (3)
#define PHDNLDNFC_FRAMESTATUS_OFFSET (2)
#define PHDNLDNFC_FIRST_FRAGFRAME_RESP (0x2DU)
#define PHDNLDNFC_NEXT_FRAGFRAME_RESP (0x2EU)
#define PHDNLDNFC_USERDATA_EEPROM_LENSIZE (2U)
#define PHDNLDNFC_USERDATA_EEPROM_OFFSIZE (2U)
/* Upstream PN547C2 option values, selected solely from original instructions. */
#define PHDNLDNFC_USERDATA_EEPROM_OFFSET (0x023CU)
#define PHDNLDNFC_USERDATA_EEPROM_LEN (0x0C80U)
static NFCSTATUS phDnldNfc_CreateFramePld(pphDnldNfc_DlContext_t pDlContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint16_t wBuffIdx = 0;
    uint16_t wChkIntgVal = 0;
    uint16_t wFrameLen = 0;

    if(NULL == pDlContext)
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameter!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        memset((pDlContext->tCmdRspFrameInfo.aFrameBuff),0,PHDNLDNFC_CMDRESP_MAX_BUFF_SIZE);
        (pDlContext->tCmdRspFrameInfo.dwSendlength) = 0;

        if(phDnldNfc_FTNone == (pDlContext->FrameInp.Type))
        {
            (pDlContext->tCmdRspFrameInfo.dwSendlength) += PHDNLDNFC_MIN_PLD_LEN;
        }
        else if(phDnldNfc_ChkIntg == (pDlContext->FrameInp.Type))
        {
            (pDlContext->tCmdRspFrameInfo.dwSendlength) += PHDNLDNFC_MIN_PLD_LEN;

            wChkIntgVal = PHDNLDNFC_USERDATA_EEPROM_OFFSET;
            memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAME_RDDATA_OFFSET]),
                        &wChkIntgVal,sizeof(wChkIntgVal));

            wChkIntgVal = PHDNLDNFC_USERDATA_EEPROM_LEN;
            memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAME_RDDATA_OFFSET +
                PHDNLDNFC_USERDATA_EEPROM_OFFSIZE]),&wChkIntgVal,sizeof(wChkIntgVal));

            (pDlContext->tCmdRspFrameInfo.dwSendlength) += PHDNLDNFC_USERDATA_EEPROM_LENSIZE;
            (pDlContext->tCmdRspFrameInfo.dwSendlength) += PHDNLDNFC_USERDATA_EEPROM_OFFSIZE;
        }
        else if(phDnldNfc_FTWrite == (pDlContext->FrameInp.Type))
        {
            wBuffIdx = (pDlContext->tRWInfo.wOffset);
            if(wBuffIdx>pDlContext->tUserData.wLen ||
               (!pDlContext->tRWInfo.bFramesSegmented && pDlContext->tUserData.wLen-wBuffIdx<2)) return NFCSTATUS_INVALID_PARAMETER;

            if(FALSE == (pDlContext->tRWInfo.bFramesSegmented))
            {
                wFrameLen = (pDlContext->tUserData.pBuff[wBuffIdx]);
                wFrameLen <<= 8;
                wFrameLen |= (pDlContext->tUserData.pBuff[wBuffIdx + 1]);

                if(!wFrameLen || (uint32_t)wFrameLen+2>(uint32_t)(pDlContext->tUserData.wLen-wBuffIdx))
                    return NFCSTATUS_INVALID_PARAMETER;
                (pDlContext->tRWInfo.wRWPldSize) = wFrameLen;
            }

            if((pDlContext->tRWInfo.wRWPldSize) > PHDNLDNFC_CMDRESP_MAX_PLD_SIZE)
            {
                if(FALSE == (pDlContext->tRWInfo.bFirstChunkResp))
                {
                    (pDlContext->tRWInfo.wRemChunkBytes) = wFrameLen;
                    (pDlContext->tRWInfo.wOffset) += PHDNLDNFC_FRAME_HDR_LEN;
                    wBuffIdx = (pDlContext->tRWInfo.wOffset);
                }

                if(PHDNLDNFC_CMDRESP_MAX_PLD_SIZE < (pDlContext->tRWInfo.wRemChunkBytes))
                {
                    (pDlContext->tRWInfo.wBytesToSendRecv) = PHDNLDNFC_CMDRESP_MAX_PLD_SIZE;
                    (pDlContext->tRWInfo.bFramesSegmented) = TRUE;
                }
                else
                {
                    (pDlContext->tRWInfo.wBytesToSendRecv) = (pDlContext->tRWInfo.wRemChunkBytes);
                    (pDlContext->tRWInfo.bFramesSegmented) = FALSE;
                }

                if(pDlContext->tRWInfo.wBytesToSendRecv>pDlContext->tUserData.wLen-wBuffIdx)
                    return NFCSTATUS_INVALID_PARAMETER;
                memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[PHDNLDNFC_FRAMEID_OFFSET]),
                        &(pDlContext->tUserData.pBuff[wBuffIdx]),(pDlContext->tRWInfo.wBytesToSendRecv));
            }
            else
            {
                (pDlContext->tRWInfo.wRWPldSize) = 0;
                (pDlContext->tRWInfo.wBytesToSendRecv) = (wFrameLen + PHDNLDNFC_FRAME_HDR_LEN);

                memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[0]),
                    &(pDlContext->tUserData.pBuff[wBuffIdx]),(pDlContext->tRWInfo.wBytesToSendRecv));
            }
            (pDlContext->tCmdRspFrameInfo.dwSendlength) += (pDlContext->tRWInfo.wBytesToSendRecv);
        }
        else if(phDnldNfc_FTRead == (pDlContext->FrameInp.Type))
        {
            (pDlContext->tRWInfo.wBytesToSendRecv) = ((pDlContext->tRWInfo.wRemBytes) >
                    (pDlContext->tRWInfo.wRWPldSize)) ? (pDlContext->tRWInfo.wRWPldSize) :
                    (pDlContext->tRWInfo.wRemBytes);

            wBuffIdx = (PHDNLDNFC_PLD_OFFSET + ((sizeof(pDlContext->tRWInfo.wBytesToSendRecv))
                        % PHDNLDNFC_MIN_PLD_LEN) - 1);

            memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[wBuffIdx]),
                &(pDlContext->tRWInfo.wBytesToSendRecv),(sizeof(pDlContext->tRWInfo.wBytesToSendRecv)));

            wBuffIdx += sizeof(pDlContext->tRWInfo.wBytesToSendRecv);

            memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[wBuffIdx]),
                &(pDlContext->tRWInfo.dwAddr),sizeof(pDlContext->tRWInfo.dwAddr));

            (pDlContext->tCmdRspFrameInfo.dwSendlength) += (PHDNLDNFC_MIN_PLD_LEN +
                (sizeof(pDlContext->tRWInfo.dwAddr)));
        }
        else if(phDnldNfc_FTLog == (pDlContext->FrameInp.Type))
        {
            (pDlContext->tCmdRspFrameInfo.dwSendlength) += PHDNLDNFC_MIN_PLD_LEN;

            wBuffIdx = (PHDNLDNFC_MIN_PLD_LEN + PHDNLDNFC_FRAME_HDR_LEN);

            memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[wBuffIdx]),
                (pDlContext->tUserData.pBuff),(pDlContext->tUserData.wLen));

            (pDlContext->tCmdRspFrameInfo.dwSendlength) += (pDlContext->tUserData.wLen);
        }
        else if(phDnldNfc_FTForce == (pDlContext->FrameInp.Type))
        {
            (pDlContext->tCmdRspFrameInfo.dwSendlength) += PHDNLDNFC_MIN_PLD_LEN;

            wBuffIdx = PHDNLDNFC_PLD_OFFSET;

            memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[wBuffIdx]),
                (pDlContext->tUserData.pBuff),(pDlContext->tUserData.wLen));
        }
        else if(phDnldNfc_FTRaw == (pDlContext->FrameInp.Type))
        {
            if((0 == (pDlContext->tUserData.wLen)) || (NULL == (pDlContext->tUserData.pBuff)))
            {
                NXPLOG_FWDNLD_E("Invalid Input Parameter(s) for Raw Request!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
            }
            else
            {
                memcpy(&(pDlContext->tCmdRspFrameInfo.aFrameBuff[wBuffIdx]),
                    (pDlContext->tUserData.pBuff),(pDlContext->tUserData.wLen));

                (pDlContext->tCmdRspFrameInfo.dwSendlength) += (pDlContext->tUserData.wLen);
            }
        }
        else
        {
        }
    }

    return wStatus;
}

static NFCSTATUS phDnldNfc_UpdateRsp(pphDnldNfc_DlContext_t   pDlContext, phTmlNfc_TransactInfo_t  *pInfo, uint16_t wPldLen)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint16_t wReadLen = 0;

    if((NULL == pDlContext) || (NULL == pInfo) || !pInfo->pBuff ||
       pInfo->wLength<5 || wPldLen>pInfo->wLength-5)
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD,NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(PH_DL_CMD_WRITE == (pDlContext->tCmdId))
        {
            if(PH_DL_STATUS_OK == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                /* first write frame response received case */
                if(TRUE == (pDlContext->tRWInfo.bFirstWrReq))
                {
                    NXPLOG_FWDNLD_D("First Write Frame Success Status received!!");
                    (pDlContext->tRWInfo.bFirstWrReq) = FALSE;
                }

                if(TRUE == (pDlContext->tRWInfo.bFirstChunkResp))
                {
                    if(FALSE == (pDlContext->tRWInfo.bFramesSegmented))
                    {
                        NXPLOG_FWDNLD_D("Chunked Write Frame Success Status received!!");
                        (pDlContext->tRWInfo.wRemChunkBytes) -= (pDlContext->tRWInfo.wBytesToSendRecv);
                        (pDlContext->tRWInfo.bFirstChunkResp) = FALSE;
                    }
                    else
                    {
                        NXPLOG_FWDNLD_E("UnExpected Status received!!");
                        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_FAILED);
                    }
                }

                if(NFCSTATUS_SUCCESS == wStatus)
                {
                    (pDlContext->tRWInfo.wRemBytes) -= (pDlContext->tRWInfo.wBytesToSendRecv);
                    (pDlContext->tRWInfo.wOffset) += (pDlContext->tRWInfo.wBytesToSendRecv);
                }
            }
            else if((FALSE == (pDlContext->tRWInfo.bFirstChunkResp)) &&
                (TRUE == (pDlContext->tRWInfo.bFramesSegmented)) &&
                (PHDNLDNFC_FIRST_FRAGFRAME_RESP == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET])))
            {
                (pDlContext->tRWInfo.bFirstChunkResp) = TRUE;
                (pDlContext->tRWInfo.wRemChunkBytes) -= (pDlContext->tRWInfo.wBytesToSendRecv);
                (pDlContext->tRWInfo.wRemBytes) -= ((pDlContext->tRWInfo.wBytesToSendRecv) + PHDNLDNFC_FRAME_HDR_LEN);
                (pDlContext->tRWInfo.wOffset) += (pDlContext->tRWInfo.wBytesToSendRecv);

                /* first write frame response received case */
                if(TRUE == (pDlContext->tRWInfo.bFirstWrReq))
                {
                    NXPLOG_FWDNLD_D("First Write Frame Success Status received!!");
                    (pDlContext->tRWInfo.bFirstWrReq) = FALSE;
                }
            }
            else if((TRUE == (pDlContext->tRWInfo.bFirstChunkResp)) &&
                (TRUE == (pDlContext->tRWInfo.bFramesSegmented)) &&
                (PHDNLDNFC_NEXT_FRAGFRAME_RESP == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET])))
            {
                (pDlContext->tRWInfo.wRemChunkBytes) -= (pDlContext->tRWInfo.wBytesToSendRecv);
                (pDlContext->tRWInfo.wRemBytes) -= (pDlContext->tRWInfo.wBytesToSendRecv);
                (pDlContext->tRWInfo.wOffset) += (pDlContext->tRWInfo.wBytesToSendRecv);
            }
            else if(PH_DL_STATUS_FIRMWARE_VERSION_ERROR == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                NXPLOG_FWDNLD_E("Firmware Already Up To Date!!");
                (pDlContext->tRWInfo.bFirstWrReq) = FALSE;
                /* resetting wRemBytes to 0 to avoid any further write frames send */
                (pDlContext->tRWInfo.wRemBytes) = 0;
                (pDlContext->tRWInfo.wOffset) = 0;
                wStatus = NFCSTATUS_FAILED;
            }
            else if(PH_DL_STATUS_PLL_ERROR == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                NXPLOG_FWDNLD_E("PLL Error Status received!!");
                (pDlContext->tLastStatus) = PH_DL_STATUS_PLL_ERROR;
                wStatus = NFCSTATUS_WRITE_FAILED;
            }
            else if(PH_DL_STATUS_SIGNATURE_ERROR == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                NXPLOG_FWDNLD_E("Signature Mismatch Error received!!");
                /* save the status for use in loading the relevant recovery image (either signature or platform) */
                (pDlContext->tLastStatus) = PH_DL_STATUS_SIGNATURE_ERROR;
                wStatus = NFCSTATUS_REJECTED;
            }
            else if(PH_DL_STATUS_MEM_BSY == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                NXPLOG_FWDNLD_E("Mem Busy Status received!!");
                (pDlContext->tLastStatus) = PH_DL_STATUS_MEM_BSY;
                wStatus = NFCSTATUS_BUSY;
            }
            else
            {
                NXPLOG_FWDNLD_E("Unsuccessful Status received!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_FAILED);
            }
        }
        else if(PH_DL_CMD_READ == (pDlContext->tCmdId))
        {
            if(PH_DL_STATUS_OK == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                if(pInfo->wLength<8) return NFCSTATUS_INVALID_RECEIVE_LENGTH;
                wReadLen = (((uint16_t)(pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET + 3]) << 8U) |
                           (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET + 2]));

                if(wReadLen != (pDlContext->tRWInfo.wBytesToSendRecv) ||
                   wReadLen>pInfo->wLength-8 ||
                   pDlContext->tRWInfo.wOffset>pDlContext->wRspCapacity ||
                   wReadLen>pDlContext->wRspCapacity-pDlContext->tRWInfo.wOffset ||
                   !pDlContext->tRspBuffInfo.pBuff)
                {
                    NXPLOG_FWDNLD_E("Desired Length bytes not received!!");
                    wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_FAILED);
                }
                else
                {
                    memcpy(&(pDlContext->tRspBuffInfo.pBuff[(pDlContext->tRWInfo.wOffset)]),
                        &(pInfo->pBuff[PHDNLDNFC_FRAME_RDDATA_OFFSET]),
                        wReadLen);

                    (pDlContext->tRWInfo.wBytesRead) += wReadLen;

                    (pDlContext->tRspBuffInfo.wLen) = (pDlContext->tRWInfo.wBytesRead);

                    (pDlContext->tRWInfo.wRemBytes) -= (pDlContext->tRWInfo.wBytesToSendRecv);
                    (pDlContext->tRWInfo.dwAddr) += (pDlContext->tRWInfo.wBytesToSendRecv);
                    (pDlContext->tRWInfo.wOffset) += (pDlContext->tRWInfo.wBytesToSendRecv);
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Unsuccessful Status received!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_FAILED);
            }
        }
        else
        {
            if(PH_DL_STATUS_OK == (pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET]))
            {
                if((0 != (pDlContext->tRspBuffInfo.wLen)) &&
                    (NULL != (pDlContext->tRspBuffInfo.pBuff)))
                {
                    if(wPldLen>pDlContext->wRspCapacity) return NFCSTATUS_BUFFER_TOO_SMALL;
                    memcpy((pDlContext->tRspBuffInfo.pBuff),&(pInfo->pBuff[PHDNLDNFC_FRAMESTATUS_OFFSET + 1]),
                        wPldLen);

                    (pDlContext->tRspBuffInfo.wLen) = wPldLen;
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Unsuccessful Status received!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_FAILED);
            }
        }
    }

    return wStatus;
}
