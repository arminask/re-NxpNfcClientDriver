#pragma once
#include <phNfcTypes.h>
#include <phNfcStatus.h>
#include <stddef.h>

// Private C ABI recovered from the exact PDB, not the Android download context.
#ifndef NXP_DOWNLOAD_ROM_VERSION_DEFINED
#define NXP_DOWNLOAD_ROM_VERSION_DEFINED
typedef enum _phDnldNfc_NFCC_ROM_VERSION {
    phDnldNfc_NFCC_ROM_Version_PN547 = 8,
    phDnldNfc_NFCC_ROM_Version_PN548 = 16,
    phDnldNfc_NFCC_ROM_Version_PN553 = 17,
    phDnldNfc_NFCC_ROM_Version_PN557 = 18
} phDnldNfc_NFCC_VERSION_t;
#else
typedef enum _phDnldNfc_NFCC_ROM_VERSION phDnldNfc_NFCC_VERSION_t;
#endif

typedef enum phDnldNfc_LC {
    phDnldNfc_LCCreat = 11,
    phDnldNfc_LCInit = 13,
    phDnldNfc_LCOper = 17,
    phDnldNfc_LCTerm = 19
} phDnldNfc_LC_t;

typedef enum phDnldNfc_ClkSrc {
    phDnldNfc_ClkSrcXtal = 1,
    phDnldNfc_ClkSrcPLL = 2,
    phDnldNfc_ClkSrcPad = 3
} phDnldNfc_ClkSrc_t;

typedef enum phDnldNfc_ClkFreq {
    phDnldNfc_ClkFreq_13Mhz = 0,
    phDnldNfc_ClkFreq_19_2Mhz = 1,
    phDnldNfc_ClkFreq_24Mhz = 2,
    phDnldNfc_ClkFreq_26Mhz = 3,
    phDnldNfc_ClkFreq_38_4Mhz = 4,
    phDnldNfc_ClkFreq_52Mhz = 5
} phDnldNfc_ClkFreq_t;

typedef struct phDnldNfc_Buff {
    uint8_t* pBuff;
    uint16_t wLen;
} phDnldNfc_Buff_t, *pphDnldNfc_Buff_t;

typedef void (*pphDnldNfc_RspCb_t)(void*,NFCSTATUS,void*);

#ifdef __cplusplus
extern "C" {
#endif
NFCSTATUS phDnldNfc_SetHwDevHandle(void*);
void phDnldNfc_FreeDnldContext(void);
NFCSTATUS phDnldNfc_Reset(pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_GetVersion(pphDnldNfc_Buff_t,pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_GetSessionState(pphDnldNfc_Buff_t,pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_CheckIntegrity(phDnldNfc_NFCC_VERSION_t,pphDnldNfc_Buff_t,pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_Force(pphDnldNfc_Buff_t,pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_Log(pphDnldNfc_Buff_t,pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_ReadLog(pphDnldNfc_Buff_t,pphDnldNfc_RspCb_t,void*);
NFCSTATUS phDnldNfc_Write(uint8_t,phNfc_sData_t*,pphDnldNfc_RspCb_t,void*);
#ifdef __cplusplus
}
#endif
