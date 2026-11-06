#pragma once
#include <windows.h>
#include "nxp_nci_context.h"
#ifdef __cplusplus
extern "C" {
#endif
void phNciNfc_FreeSendPayloadBuff(void*);
NFCSTATUS phNciNfc_GenericSequence(void*,void*,NFCSTATUS);
void phNciNfc_Notify(void*,NFCSTATUS,void*);
NFCSTATUS phNciNfc_SeqHandler(void*,NFCSTATUS);
#ifdef __cplusplus
}
#endif
