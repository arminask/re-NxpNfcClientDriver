#include "device.h"
extern "C" {
#include <phOsalNfc.h>
}
#include "nxp_trace.h"
#include "msgqueue.tmh"

void NfcMessageHandler(void* context,UINT32 message,UINT_PTR first,UINT_PTR second,UINT_PTR third,UINT_PTR fourth)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(device->Stopping && message<2) {
        if(message==0 && second) reinterpret_cast<PFN_NFC_CX_SEQUENCE_COMPLETION_ROUTINE>(second)(device->FxDevice,STATUS_CANCELLED,0,reinterpret_cast<void*>(third));
        return;
    }
    switch(message) {
    case 0:
        NfcExecuteSequence(device,static_cast<NFC_CX_SEQUENCE>(first),
            reinterpret_cast<PFN_NFC_CX_SEQUENCE_COMPLETION_ROUTINE>(second),reinterpret_cast<void*>(third));
        break;
    case 1:
        NfcSequenceHandler(device,static_cast<NTSTATUS>(first),reinterpret_cast<void*>(second),reinterpret_cast<void*>(third));
        break;
    case 2: {
        using MESSAGE_CALLBACK=void (*)(void*,UINT_PTR,UINT_PTR,UINT_PTR);
        reinterpret_cast<MESSAGE_CALLBACK>(first)(context,second,third,fourth);
        break;
    }
    default: break;
    }
    TRACE_FUNCTION_EXIT(LEVEL_VERBOSE);
}

void NfcPostMessage(void*,UINT32 message,UINT_PTR first,UINT_PTR second,UINT_PTR third,UINT_PTR fourth)
{
    phOsalNfc_PostMsg(message,first,second,third,fourth);
}
