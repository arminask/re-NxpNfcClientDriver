#pragma once
#include "device.h"

// PDB: 16-byte polymorphic base, initialized BOOLEAN at offset eight. Original
// vtable at RVA 0x2e930: deleting destructor, IsInitialized, WriteFile. Additional
// factory/static declarations are deferred until their definitions are recovered.
class INfcProximityBus {
public:
    virtual ~INfcProximityBus()=default;
    virtual bool IsInitialized() { return m_bInitialized; }
    virtual NTSTATUS WriteFile(WDFDEVICE,UCHAR*,USHORT,PFN_WDF_REQUEST_COMPLETION_ROUTINE,void*)=0;
    virtual void CancelWrite(void*)=0;
protected:
    bool m_bInitialized;
    static INfcProximityBus* m_BusBaseInstance;
    static INfcProximityBus* m_BusBaseInstance2;
};
