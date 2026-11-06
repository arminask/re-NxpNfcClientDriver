#include "device.h"
#include <stdlib.h>
#include "nxp_buffer_validation.h"
#include "nxp_trace.h"
#include "NfcEEProm.tmh"

// Validate atomically: rejected blobs never leave a partial parameter count.
UCHAR EEProm_Parse_ConfigBlob(DEVICE_CONTEXT* device,UCHAR* blob,ULONG length)
{
    device->DriverStore_NoEEPromParams=0;
    uint8_t count=0;
    if(!NxpValidateEepromBlob(blob,length,&count)) return FALSE;
    device->DriverStore_NoEEPromParams=count;
    return TRUE;
}

UCHAR EEPROM_Process_CustomEEPromConfigBlob(DEVICE_CONTEXT* device)
{
    NTSTATUS status=Process_EEPromUserData(device);
    device->GetEEPromUserDataSize=NT_SUCCESS(status)?device->DriverStore_EEPromParamsDataSize:0;
    return NT_SUCCESS(status);
}
