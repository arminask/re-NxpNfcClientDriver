#pragma once
#include <windows.h>
#include "../src/NfcCx/Clients/NxpCoreLib/phNciNfc_Sequence.h"
#include <NfcCoreLibTracing.h>
// Published algorithms need only the original table/next/max fields. The
// client's PDB context is 1424 bytes, unlike the public upper NFC stack.
typedef phNciNfc_Context_t* pphNciNfc_Context_t;
typedef phNciNfc_SequenceParams_t phNciNfc_SequenceP_t;
