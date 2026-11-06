#include <windows.h>
#include "phDnldNfc_Internal.h"

// The original private C TU owns all twelve routines, recovered from audited
// published subsets and bridged to its exact PDB context. See DOWNLOAD-*.md.
#include "../../../../third_party/aosp-nxp-download/halimpl/pn54x/dnld/phDnldNfc_Utils.c"

#include <phNfcCompId.h>
#include <phTmlNfc.h>
#include <phOsalNfc_Timer.h>
#ifndef UNUSED
#define UNUSED(value) ((void)(value))
#endif
// Keep original private state engines static; native isolated boundaries have
// separate external fixture names.
#ifdef NXP_DOWNLOAD_NATIVE_TEST
void phDnldNfc_ProcessSeqState(void*,phTmlNfc_TransactInfo_t*);
#else
static void phDnldNfc_ProcessSeqState(void*,phTmlNfc_TransactInfo_t*);
#endif
#ifdef NXP_DOWNLOAD_NATIVE_TEST
void phDnldNfc_ProcessRWSeqState(void*,phTmlNfc_TransactInfo_t*);
#else
static void phDnldNfc_ProcessRWSeqState(void*,phTmlNfc_TransactInfo_t*);
#endif
// Timer helpers retain their upstream static linkage.
#include "../../../../third_party/aosp-nxp-download/halimpl/pn54x/dnld/phDnldNfc_Internal.c"

#ifdef NXP_DOWNLOAD_NATIVE_TEST
#undef phDnldNfc_ProcessSeqState
#endif
#ifdef NXP_DOWNLOAD_NATIVE_TEST
NFCSTATUS phDnldNfc_BuildFramePkt(pphDnldNfc_DlContext_t);
NFCSTATUS phDnldNfc_ProcessFrame(void*,phTmlNfc_TransactInfo_t*);
#else
static NFCSTATUS phDnldNfc_BuildFramePkt(pphDnldNfc_DlContext_t);
static NFCSTATUS phDnldNfc_ProcessFrame(void*,phTmlNfc_TransactInfo_t*);
#endif
#include "../../../../third_party/aosp-nxp-download/halimpl/pn54x/dnld/phDnldNfc_Internal_Sequence.inc"
#ifdef NXP_DOWNLOAD_NATIVE_TEST
#undef phDnldNfc_ProcessRWSeqState
#endif
#include "../../../../third_party/aosp-nxp-download/halimpl/pn54x/dnld/phDnldNfc_Internal_RW.inc"
#ifdef NXP_DOWNLOAD_NATIVE_TEST
#undef phDnldNfc_BuildFramePkt
#undef phDnldNfc_ProcessFrame
#endif
#ifdef NXP_DOWNLOAD_NATIVE_TEST
NFCSTATUS phDnldNfc_CreateFramePld(pphDnldNfc_DlContext_t);
NFCSTATUS phDnldNfc_UpdateRsp(pphDnldNfc_DlContext_t,phTmlNfc_TransactInfo_t*,uint16_t);
#else
static NFCSTATUS phDnldNfc_CreateFramePld(pphDnldNfc_DlContext_t);
static NFCSTATUS phDnldNfc_UpdateRsp(pphDnldNfc_DlContext_t,phTmlNfc_TransactInfo_t*,uint16_t);
#endif
#include "../../../../third_party/aosp-nxp-download/halimpl/pn54x/dnld/phDnldNfc_Internal_Frame.inc"
#ifdef NXP_DOWNLOAD_NATIVE_TEST
#undef phDnldNfc_CreateFramePld
#undef phDnldNfc_UpdateRsp
#endif
#include "../../../../third_party/aosp-nxp-download/halimpl/pn54x/dnld/phDnldNfc_Internal_Payload.inc"

#ifdef NXP_DOWNLOAD_NATIVE_TEST
void NxpDownloadTestResponseTimeout(uint32_t timer,void* context)
{ phDnldNfc_RspTimeOutCb(timer,context); }
void NxpDownloadTestResendTimeout(uint32_t timer,void* context)
{ phDnldNfc_ResendTimeOutCb(timer,context); }
NFCSTATUS NxpDownloadTestSetupTimer(pphDnldNfc_DlContext_t context)
{ return phDnldNfc_SetupResendTimer(context); }
#endif

#ifdef NXP_DOWNLOAD_NATIVE_TEST
void NxpDownloadTestNormalState(void* context,phTmlNfc_TransactInfo_t* info)
{ phDnldNfc_ProcessSeqState(context,info); }
NFCSTATUS NxpDownloadTestRecvInfo(void* context,phTmlNfc_TransactInfo_t* info)
{ return phDnldNfc_ProcessRecvInfo(context,info); }
#endif

#ifdef NXP_DOWNLOAD_NATIVE_TEST
void NxpDownloadTestRWState(void* context,phTmlNfc_TransactInfo_t* info)
{ phDnldNfc_ProcessRWSeqState(context,info); }
#endif

#ifdef NXP_DOWNLOAD_NATIVE_TEST
NFCSTATUS NxpDownloadTestBuildFrame(pphDnldNfc_DlContext_t context)
{ return phDnldNfc_BuildFramePkt(context); }
NFCSTATUS NxpDownloadTestProcessFrame(void* context,phTmlNfc_TransactInfo_t* info)
{ return phDnldNfc_ProcessFrame(context,info); }
#endif

#ifdef NXP_DOWNLOAD_NATIVE_TEST
NFCSTATUS NxpDownloadTestCreatePayload(pphDnldNfc_DlContext_t context)
{ return phDnldNfc_CreateFramePld(context); }
NFCSTATUS NxpDownloadTestUpdateResponse(pphDnldNfc_DlContext_t context,phTmlNfc_TransactInfo_t* info,uint16_t length)
{ return phDnldNfc_UpdateRsp(context,info,length); }
#endif
