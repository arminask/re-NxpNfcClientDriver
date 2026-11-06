#pragma once
#include "phNciNfc.h"
#include "phNciNfc_Common.h"
#include "phNciNfc_Sequence.h"
typedef enum phNciNfc_PowerMode {
    phNciNfc_e_DisableStandby=0,phNciNfc_e_EnableStandby=1
} phNciNfc_PowerMode_t;
#ifdef __cplusplus
extern "C" {
#endif
NFCSTATUS phNciNfc_EnableExtnCmd(void*);
NFCSTATUS phNciNfc_EnableExtnRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteEnableExtnSeq(void*,NFCSTATUS);
NFCSTATUS phNciNfc_EnablePropExtn(void*,void (*)(void*,NFCSTATUS,void*),void*);
NFCSTATUS phNciNfc_SendStandbyCmd(void*);
NFCSTATUS phNciNfc_ProcessStandbyRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteStandbySequence(void*,NFCSTATUS);
NFCSTATUS phNciNfc_SetPowerMode(void*,phNciNfc_PowerMode_t,void (*)(void*,NFCSTATUS,void*),void*);
NFCSTATUS phNciNfc_ChkPresExtnCmd(void*);
NFCSTATUS phNciNfc_ChkPresExtnRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteChkPresExtnSeq(void*,NFCSTATUS);
NFCSTATUS phNciNfc_ChkPresExtnNtfCb(void*,void*,NFCSTATUS);
void phNciNfc_ChkPresExtnNtfTimeoutHandler(uint32_t,void*);
NFCSTATUS phNciNfc_ConfigListenFilterCmd(void*);
NFCSTATUS phNciNfc_ConfigListenFilterRsp(void*,NFCSTATUS);
NFCSTATUS phNciNfc_CompleteConfigListenFilterSeq(void*,NFCSTATUS);
#ifdef __cplusplus
}
#endif
