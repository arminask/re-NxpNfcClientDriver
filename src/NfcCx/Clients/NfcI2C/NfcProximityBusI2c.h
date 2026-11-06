#pragma once
#include "nfcproximitybusinterface.h"
struct _READ_REQUEST_CONTEXT {
    LIST_ENTRY ListEntry;
    WDFMEMORY Memory;
    WDFREQUEST Request;
    _FIRMWARE_MODE Mode;
};
class CNfcProximityBusI2C;
struct _WRITE_REQUEST_CONTEXT {
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine;
    void* CompletionContext;
    WDFMEMORY Memory;
    size_t ExpectedLength;
    LIST_ENTRY Link;
    CNfcProximityBusI2C* Bus;
    WDFREQUEST Request;
    volatile LONG Owners;
    bool Sent,Aborted,Completed;
};
using READ_REQUEST_CONTEXT=_READ_REQUEST_CONTEXT;
using WRITE_REQUEST_CONTEXT=_WRITE_REQUEST_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(READ_REQUEST_CONTEXT,GetReadRequestContext);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WRITE_REQUEST_CONTEXT,GetWriteRequestContext);
class CNfcProximityBusI2C:public INfcProximityBus {
public:
    explicit CNfcProximityBusI2C(WDFDEVICE);
    ~CNfcProximityBusI2C() override;
    static INfcProximityBus* GetInstance(WDFDEVICE);
    void CancelWrite(void*) override;
    NTSTATUS WriteFile(WDFDEVICE,UCHAR*,USHORT,PFN_WDF_REQUEST_COMPLETION_ROUTINE,void*) override;
    void SpbDestroy(DEVICE_CONTEXT*);
    NTSTATUS SpbGpioWrite(WDFDEVICE,UCHAR,WDFIOTARGET);
    NTSTATUS SpbWriteRequest(WDFDEVICE,WDFREQUEST);
    NTSTATUS SpbAllocateResourceForRead(WDFDEVICE,WDFIOTARGET,WDFREQUEST*);
    NTSTATUS SpbRead(WDFDEVICE,WDFREQUEST*);
    NTSTATUS SpbOpenIoTarget(WDFDEVICE,WDFCMRESLIST,ULONG,ULONG,WDFIOTARGET*);
    NTSTATUS SpbInitialize(DEVICE_CONTEXT*,WDFCMRESLIST,WDFCMRESLIST);
    static EVT_WDF_INTERRUPT_ISR OnInterruptIsr;
    static EVT_WDF_INTERRUPT_WORKITEM OnInterruptWorkItem;
    static EVT_WDF_REQUEST_COMPLETION_ROUTINE SpbWriteToDeviceRequestCompletion;
    SRWLOCK RawWriteLock;
    LIST_ENTRY RawWrites;
    WDFWAITLOCK InterruptWaitLock;
    WDFIOTARGET SpbIoTarget;
    WDFIOTARGET DownloadPinIoTarget;
    WDFIOTARGET VenPinIoTarget;
};
UCHAR NxpProprietaryNciPacketFilter(DEVICE_CONTEXT*,WDFMEMORY);
NTSTATUS NfcVendorTestReadNotification(DEVICE_CONTEXT*,WDFMEMORY);
