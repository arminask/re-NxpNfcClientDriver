#pragma once
#include "phDnldNfc.h"

typedef enum phDnldNfc_Event {
    phDnldNfc_EventInvalid = 0,
    phDnldNfc_EventReset = 1,
    phDnldNfc_EventGetVer = 2,
    phDnldNfc_EventWrite = 3,
    phDnldNfc_EventRead = 4,
    phDnldNfc_EventIntegChk = 5,
    phDnldNfc_EventGetSesnSt = 6,
    phDnldNfc_EventLog = 7,
    phDnldNfc_EventForce = 8,
    phDnldNfc_EventRaw = 9,
    phDnldNfc_EVENT_INT_MAX = 10
} phDnldNfc_Event_t;

typedef enum phDnldNfc_Transition {
    phDnldNfc_TransitionIdle = 0,
    phDnldNfc_TransitionBusy = 1,
    phDnldNfc_TransitionInvalid = 2
} phDnldNfc_Transition_t;

typedef enum phDnldNfc_State {
    phDnldNfc_StateInit = 0,
    phDnldNfc_StateSend = 1,
    phDnldNfc_StateRecv = 2,
    phDnldNfc_StateTimer = 3,
    phDnldNfc_StateResponse = 4,
    phDnldNfc_StatePipelined = 5,
    phDnldNfc_StateInvalid = 6
} phDnldNfc_State_t;

typedef enum phDnldNfc_CmdId {
    PH_DL_CMD_NONE = 0,
    PH_DL_CMD_RESET = 240,
    PH_DL_CMD_GETVERSION = 241,
    PH_DL_CMD_CHECKINTEGRITY = 224,
    PH_DL_CMD_WRITE = 192,
    PH_DL_CMD_READ = 162,
    PH_DL_CMD_LOG = 167,
    PH_DL_CMD_FORCE = 208,
    PH_DL_CMD_GETSESSIONSTATE = 242
} phDnldNfc_CmdId_t;

typedef enum {
    phDnldNfc_FTNone = 0,
    phDnldNfc_ChkIntg = 1,
    phDnldNfc_FTWrite = 2,
    phDnldNfc_FTLog = 3,
    phDnldNfc_FTForce = 4,
    phDnldNfc_FTRead = 5,
    phDnldNfc_FTRaw = 6
} phDnldNfc_FrameInputType_t;

typedef enum phDnldNfc_Status {
    PH_DL_STATUS_PLL_ERROR = 13,
    PH_DL_STATUS_LC_WRONG = 19,
    PH_DL_STATUS_LC_TERMINATION_NOT_SUPPORTED = 20,
    PH_DL_STATUS_LC_CREATION_NOT_SUPPORTED = 21,
    PH_DL_STATUS_LC_UNKNOWN = 22,
    PH_DL_STATUS_AUTHENTICATION_ERROR = 25,
    PH_DL_STATUS_NOT_AUTHENTICATED = 26,
    PH_DL_STATUS_AUTHENTICATION_LOST = 27,
    PH_DL_STATUS_WRITE_PROTECTED = 28,
    PH_DL_STATUS_READ_PROTECTED = 29,
    PH_DL_STATUS_ADDR_RANGE_OFL_ERROR = 30,
    PH_DL_STATUS_BUFFER_OFL_ERROR = 31,
    PH_DL_STATUS_MEM_BSY = 32,
    PH_DL_STATUS_SIGNATURE_ERROR = 33,
    PH_DL_STATUS_SESSION_WAS_OPEN = 34,
    PH_DL_STATUS_SESSION_WAS_CLOSED = 35,
    PH_DL_STATUS_FIRMWARE_VERSION_ERROR = 36,
    PH_DL_STATUS_LOOPBACK_DATA_MISSMATCH_ERROR = 37,
    PH_DL_STATUS_HOST_PAYLOAD_UFL_ERROR = 38,
    PH_DL_STATUS_HOST_PAYLOAD_OFL_ERROR = 39,
    PH_DL_STATUS_PROTOCOL_ERROR = 40,
    PH_DL_STATUS_INVALID_ADDR = 1,
    PH_DL_STATUS_GENERIC_ERROR = 255,
    PH_DL_STATUS_ABORTED_CMD = 150,
    PH_DL_STATUS_FLASH_WRITE_PROTECTED = 28,
    PH_DL_STATUS_FLASH_READ_PROTECTED = 29,
    PH_DL_STATUS_USERDATA_WRITE_PROTECTED = 28,
    PH_DL_STATUS_USERDATA_READ_PROTECTED = 29,
    PH_DL_STATUS_OK = 0
} phDnldNfc_Status_t;

typedef struct phDnldNfc_FrameInfo {
    uint16_t dwSendlength;
    uint8_t aFrameBuff[256];
} phDnldNfc_FrameInfo_t, *pphDnldNfc_FrameInfo_t;

typedef struct phDnldNfc_RWInfo {
    uint32_t dwAddr;
    uint16_t wOffset;
    uint16_t wRemBytes;
    uint16_t wRemChunkBytes;
    uint16_t wRWPldSize;
    uint16_t wBytesToSendRecv;
    uint16_t wBytesRead;
    uint8_t bFramesSegmented;
    uint8_t bFirstWrReq;
    uint8_t bFirstChunkResp;
} phDnldNfc_RWInfo_t, *pphDnldNfc_RWInfo_t;

typedef struct phDnldNfc_FrameInput {
    phDnldNfc_FrameInputType_t Type;
    uint32_t dwAddr;
} phDnldNfc_FrameInput_t, *pphDnldNfc_FrameInput_t;

typedef struct phDnldNfc_RspTimerInfo {
    uint32_t dwRspTimerId;
    uint8_t TimerStatus;
    uint16_t wTimerExpStatus;
} phDnldNfc_RspTimerInfo_t, *pphDnldNfc_RspTimerInfo_t;

typedef struct phDnldNfc_DlContext {
    uint8_t bResendLastFrame;
    phDnldNfc_Transition_t tDnldInProgress;
    phDnldNfc_Event_t tCurrEvent;
    phDnldNfc_State_t tCurrState;
    pphDnldNfc_RspCb_t UserCb;
    void* UserCtxt;
    phDnldNfc_Buff_t tUserData;
    phDnldNfc_Buff_t tRspBuffInfo;
    phDnldNfc_FrameInfo_t tCmdRspFrameInfo;
    phDnldNfc_FrameInfo_t tPipeLineWrFrameInfo;
    uint16_t wCmdSendStatus;
    phDnldNfc_CmdId_t tCmdId;
    phDnldNfc_FrameInput_t FrameInp;
    phDnldNfc_RspTimerInfo_t TimerInfo;
    void* pHwRef;
    phDnldNfc_Buff_t tTKey;
    phDnldNfc_RWInfo_t tRWInfo;
    phDnldNfc_Status_t tLastStatus;
    uint8_t bPldVal[3];
    uint16_t wRspCapacity; // Allocation extent, never overwritten by response length.
} phDnldNfc_DlContext_t, *pphDnldNfc_DlContext_t;

#ifdef __cplusplus
extern "C" {
#endif
extern phDnldNfc_DlContext_t* gpphDnldContext;
uint16_t phDnldNfc_CalcCrc16(uint8_t*,uint16_t);
NFCSTATUS phDnldNfc_CmdHandler(void*,phDnldNfc_Event_t);
#ifdef __cplusplus
}
#endif
