#pragma once
#include "nxp_nci_context.h"
typedef struct phNciNfc_NfccCapabilities {
    uint8_t ManufacturerId;
    uint8_t HwVersion;
    uint8_t RomVersion;
    uint16_t FwVersion;
} phNciNfc_NfccCapabilities_t;
#ifdef __cplusplus
extern "C" {
#endif
extern phNciNfc_Context_t* volatile gpphNciNfc_Context;
NFCSTATUS phNciNfc_Deinitialise(void*);
NFCSTATUS phNciNfc_GetNfccCapabilities(void*,phNciNfc_NfccCapabilities_t*);
void phNciNfc_NciCtxInitialize(phNciNfc_Context_t*);
#ifdef __cplusplus
}
#endif
