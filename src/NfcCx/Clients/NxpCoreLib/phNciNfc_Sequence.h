#pragma once
#include "nxp_nci_context.h"
#ifdef __cplusplus
extern "C" {
#endif
NFCSTATUS phNciNfc_SeqHandler(void*,NFCSTATUS);
uint8_t phNciNfc_getSequenceLength(const phNciNfc_SequenceParams_t*);
uint8_t phNciNfc_RepeatSequenceSeq(void*,phNciNfc_SequenceParams_t*,uint8_t);
uint8_t phNciNfc_SkipSequenceSeq(void*,phNciNfc_SequenceParams_t*,uint8_t);
#ifdef __cplusplus
}
#endif
