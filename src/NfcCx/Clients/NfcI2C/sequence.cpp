#include "device.h"
#include "nxp_trace.h"
#include "sequence.tmh"

UCHAR NfcGetSequenceLength(SEQUENCE* sequence)
{
    UCHAR length=0;
    if(sequence) while(sequence->SequenceInitiate) { ++length; ++sequence; }
    return length;
}

void NfcInternalSequence(DEVICE_CONTEXT* device,SEQUENCE* sequence,NTSTATUS status,void* first,void* second)
{
    if(device->pSeqHandler==sequence) {
        NfcPostMessage(device,1,static_cast<UINT_PTR>(static_cast<LONG_PTR>(status)),
            reinterpret_cast<UINT_PTR>(first),reinterpret_cast<UINT_PTR>(second),0);
    } else {
        TRACE_LINE(LEVEL_WARNING,"Sequence mismatch");
    }
}

NTSTATUS NfcRepeatSequence(DEVICE_CONTEXT* device,SEQUENCE* sequence,UCHAR steps)
{
    NTSTATUS status=STATUS_INVALID_PARAMETER;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    if(steps) {
        if(sequence!=device->pSeqHandler) {
            TRACE_LINE(LEVEL_ERROR,"Sequence mismatch, can not decrement the current sequence");
        } else if(steps>device->SeqNext) {
            TRACE_LINE(LEVEL_ERROR,"Value to decrement is invalid");
        } else {
            device->SeqNext=static_cast<UCHAR>(device->SeqNext-steps);
            status=STATUS_SUCCESS;
        }
    }
    TRACE_FUNCTION_EXIT(LEVEL_VERBOSE);
    return status;
}

NTSTATUS NfcSkipSequence(DEVICE_CONTEXT* device,SEQUENCE* sequence,UCHAR steps)
{
    NTSTATUS status=STATUS_INVALID_PARAMETER;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    if(steps) {
        if(sequence!=device->pSeqHandler) {
            TRACE_LINE(LEVEL_ERROR,"Sequence mismatch, can not decrement the current sequence");
        } else if(static_cast<unsigned>(steps)+device->SeqNext>device->SeqMax) {
            TRACE_LINE(LEVEL_ERROR,"Value to increment is invalid");
        } else {
            device->SeqNext=static_cast<UCHAR>(device->SeqNext+steps);
            status=STATUS_SUCCESS;
        }
    }
    TRACE_FUNCTION_EXIT(LEVEL_VERBOSE);
    return status;
}

NTSTATUS NfcSequenceHandler(DEVICE_CONTEXT* device,NTSTATUS status,void* first,void* second)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    if(device->bSeqHandler) {
        TRACE_LINE(LEVEL_INFO,"Sequence handling in progress");
        NfcPostMessage(device,1,static_cast<UINT_PTR>(static_cast<LONG_PTR>(status)),
            reinterpret_cast<UINT_PTR>(first),reinterpret_cast<UINT_PTR>(second),0);
        TRACE_FUNCTION_EXIT(LEVEL_VERBOSE);
        return STATUS_PENDING;
    }
    device->bSeqHandler=TRUE;
    if(status==STATUS_SUCCESS && device->pSeqHandler) {
        if(device->SeqNext<=device->SeqMax) {
            if(device->SeqNext==0) {
                if(auto initiate=device->pSeqHandler[0].SequenceInitiate)
                    status=initiate(device,STATUS_SUCCESS,first,second);
                // Initial synchronous execution advances BEFORE processing the
                // previous entry. Callbacks may replace the sequence or cursor.
                while(status==STATUS_SUCCESS && device->SeqNext<device->SeqMax) {
                    ++device->SeqNext;
                    if(auto process=device->pSeqHandler[device->SeqNext-1].SequenceProcess)
                        status=process(device,status,first,second);
                    if(status==STATUS_SUCCESS) {
                        if(auto initiate=device->pSeqHandler[device->SeqNext].SequenceInitiate)
                            status=initiate(device,STATUS_SUCCESS,first,second);
                    }
                }
            } else {
                // The resume path tests the terminal cursor before processing;
                // retain that asymmetry from the reference's 0x11b44 branch.
                while(status==STATUS_SUCCESS && device->SeqNext<device->SeqMax) {
                    if(auto process=device->pSeqHandler[device->SeqNext-1].SequenceProcess)
                        status=process(device,status,first,second);
                    if(status==STATUS_SUCCESS) {
                        if(auto initiate=device->pSeqHandler[device->SeqNext].SequenceInitiate)
                            status=initiate(device,STATUS_SUCCESS,first,second);
                        if(status==STATUS_SUCCESS) ++device->SeqNext;
                    }
                }
            }
        }
    } else {
        device->SeqNext=device->SeqMax;
    }
    if(status!=STATUS_SUCCESS && status!=STATUS_PENDING) device->SeqNext=device->SeqMax;
    if(device->SeqNext==device->SeqMax) {
        if(device->pSeqHandler) {
            if(auto complete=device->pSeqHandler[device->SeqNext].SequenceProcess)
                complete(device,status,first,second); // Completion return is ignored.
        }
    } else {
        ++device->SeqNext;
    }
    device->bSeqHandler=FALSE;
    TRACE_FUNCTION_EXIT(LEVEL_VERBOSE);
    return status;
}
