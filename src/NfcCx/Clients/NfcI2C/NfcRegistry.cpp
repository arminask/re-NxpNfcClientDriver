#include "device.h"
#include <stdlib.h>
#include <stdint.h>
#include <strsafe.h>
#include "nxp_utils.h"
#include "nxp_trace.h"
#include "NfcRegistry.tmh"

// Device-specific configuration and persistence. Custom settings use the
// device hardware key; validated runtime state uses the WDF driver subkey.
UCHAR CustomRegKey_Get_CustomEEPROMConfigBlob(DEVICE_CONTEXT* device,UCHAR* buffer,ULONG* length)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    WDFKEY key=nullptr;
    UNICODE_STRING name;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(status==STATUS_SUCCESS) {
        RtlInitUnicodeString(&name,L"CustomEEPROMConfigBlob");
        ULONG type=0;
        status=WdfRegistryQueryValue(key,&name,*length,buffer,length,&type);
        if(NT_SUCCESS(status) && type!=REG_BINARY) status=STATUS_OBJECT_TYPE_MISMATCH;
    }
    if(key) WdfRegistryClose(key);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status==STATUS_SUCCESS;
}

UCHAR CustomRegKey_Get_CustomEEPROMConfigBlobLength(DEVICE_CONTEXT* device,ULONG* length)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    WDFKEY key=nullptr;
    UNICODE_STRING name;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    UCHAR result=FALSE;
    *length=0;
    if(status==STATUS_SUCCESS) {
        RtlInitUnicodeString(&name,L"CustomEEPROMConfigBlob");
        status=WdfRegistryQueryValue(key,&name,0,nullptr,length,nullptr);
        result=NT_SUCCESS(status) || status==STATUS_BUFFER_TOO_SMALL || status==STATUS_BUFFER_OVERFLOW;
    }
    if(key) WdfRegistryClose(key);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return result;
}

UCHAR CustomRegKey_IsExist_CustomEEPROMConfigBlob(DEVICE_CONTEXT* device)
{
    WDFKEY key=nullptr;
    ULONG value=0,length;
    UNICODE_STRING name;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(status==STATUS_SUCCESS) {
        RtlInitUnicodeString(&name,L"CustomEEPROMConfigBlob");
        status=WdfRegistryQueryValue(key,&name,sizeof(value),&value,&length,nullptr);
        if(key) WdfRegistryClose(key);
        return status==STATUS_SUCCESS || status==STATUS_BUFFER_OVERFLOW || status==STATUS_BUFFER_TOO_SMALL;
    }
    if(key) WdfRegistryClose(key);
    return FALSE;
}

UCHAR CustomRegKey_Get_I2CAddress(DEVICE_CONTEXT* device)
{
    WDFKEY key=nullptr;
    ULONG value=0,length;
    UNICODE_STRING name;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(status==STATUS_SUCCESS) {
        RtlInitUnicodeString(&name,L"I2cAddress");
        status=WdfRegistryQueryValue(key,&name,sizeof(value),&value,&length,nullptr);
        if(status==STATUS_SUCCESS) device->I2CAddress=static_cast<UCHAR>(value);
    }
    if(key) WdfRegistryClose(key);
    return status==STATUS_SUCCESS;
}

UCHAR CustomRegKey_Set_CustomKeepRfData(DEVICE_CONTEXT* device,ULONG value)
{
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    WDFKEY key=nullptr;
    UNICODE_STRING name;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ|KEY_SET_VALUE,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(status==STATUS_SUCCESS) {
        RtlInitUnicodeString(&name,L"CustomKeepRfData");
        status=WdfRegistryAssignValue(key,&name,REG_DWORD,sizeof(value),&value);
    }
    if(key) WdfRegistryClose(key);
    TRACE_FUNCTION_EXIT_NTSTATUS(LEVEL_VERBOSE,status);
    return status==STATUS_SUCCESS;
}

UCHAR CustomRegKey_Get_CustomKeepRfData(DEVICE_CONTEXT* device,ULONG* value)
{
    if(!value) return FALSE;
    *value=0;WDFKEY key=nullptr;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        UNICODE_STRING name;RtlInitUnicodeString(&name,L"CustomKeepRfData");
        status=WdfRegistryQueryULong(key,&name,value);
        if(status==STATUS_OBJECT_NAME_NOT_FOUND) status=STATUS_SUCCESS; // absent optional setting
    }
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status);
}

UCHAR DriverStoreRegKey_Get_KeepRfData(DEVICE_CONTEXT* device,ULONG* value)
{
    WDFKEY key=nullptr;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,
        PLUGPLAY_REGKEY_DEVICE|WDF_REGKEY_DRIVER_SUBKEY,GENERIC_ALL,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        UNICODE_STRING name;
        // The original checked string helper cannot fail for this fixed literal.
        RtlInitUnicodeString(&name,L"KeepRfData");
        status=WdfRegistryQueryULong(key,&name,value);
    }
    if(!NT_SUCCESS(status)) *value=0;
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status);
}

UCHAR DriverStoreRegKey_Set_KeepRfData(DEVICE_CONTEXT* device,ULONG value)
{
    WDFKEY key=nullptr;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,
        PLUGPLAY_REGKEY_DEVICE|WDF_REGKEY_DRIVER_SUBKEY,GENERIC_ALL,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        UNICODE_STRING name;
        RtlInitUnicodeString(&name,L"KeepRfData");
        status=WdfRegistryAssignULong(key,&name,value);
    }
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status);
}

UCHAR DriverStoreRegKey_Set_EEPROMConfigBlob(DEVICE_CONTEXT* device,UCHAR* buffer,ULONG length)
{
    if(length && !buffer) return FALSE;
    WDFKEY key=nullptr;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,
        PLUGPLAY_REGKEY_DEVICE|WDF_REGKEY_DRIVER_SUBKEY,KEY_SET_VALUE,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        UNICODE_STRING name;RtlInitUnicodeString(&name,L"EEPROMConfigBlob");
        status=WdfRegistryAssignValue(key,&name,REG_BINARY,length,buffer);
    }
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status);
}

NTSTATUS Process_EEPromUserData(DEVICE_CONTEXT* device)
{
    ULONG keep=0,length=0;
    if(device->DriverStore_EEPromParams) free(device->DriverStore_EEPromParams);
    device->DriverStore_EEPromParams=nullptr;
    device->DriverStore_EEPromParamsDataSize=0;device->DriverStore_NoEEPromParams=0;
    if(!CustomRegKey_Get_CustomKeepRfData(device,&keep)) return STATUS_UNSUCCESSFUL;
    if(keep || !CustomRegKey_IsExist_CustomEEPROMConfigBlob(device)) return STATUS_SUCCESS;
    if(!CustomRegKey_Get_CustomEEPROMConfigBlobLength(device,&length)) return STATUS_INVALID_PARAMETER;
    if(!length) return STATUS_SUCCESS;
    if(length>UINT16_MAX) return STATUS_INVALID_BUFFER_SIZE;
    auto buffer=static_cast<UCHAR*>(malloc(length));
    if(!buffer) return STATUS_INSUFFICIENT_RESOURCES;
    const ULONG capacity=length;
    if(!CustomRegKey_Get_CustomEEPROMConfigBlob(device,buffer,&length) || length>capacity ||
       !EEProm_Parse_ConfigBlob(device,buffer,length)) {
        free(buffer);return STATUS_INVALID_PARAMETER;
    }
    // Only validated data becomes device-owned. Persist after serialization.
    device->DriverStore_EEPromParams=buffer;
    device->DriverStore_EEPromParamsDataSize=length;
    return STATUS_SUCCESS;
}

NTSTATUS RegistryGetDeviceInitSettings(PWDFDEVICE_INIT deviceInit,NFC_CX_CLIENT_CONFIG* config)
{
    WDFKEY key=nullptr;
    NTSTATUS status=WdfFdoInitOpenRegistryKey(deviceInit,PLUGPLAY_REGKEY_DEVICE,KEY_READ,
        WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        // Every name is a fixed short literal; the original checked Unicode
        // initializer has no failing input on these paths.
        auto query=[key](PCWSTR literal,ULONG* value) {
            UNICODE_STRING name;RtlInitUnicodeString(&name,literal);
            return WdfRegistryQueryULong(key,&name,value);
        };
        ULONG value=0;
        if(NT_SUCCESS(query(L"DriverFlags",&value))) config->DriverFlags=value;
        if(NT_SUCCESS(query(L"IdleTimeoutType",&value)) && value<=2)
            config->PowerIdleType=static_cast<WDF_POWER_POLICY_IDLE_TIMEOUT_TYPE>(value);
        if(NT_SUCCESS(query(L"IdleTimeout",&value))) config->PowerIdleTimeout=value;
        if(NT_SUCCESS(query(L"DTAEnabled",&value)) && value!=0) config->DeviceMode=NFC_CX_DEVICE_MODE_DTA;
        status=query(L"VendorTestEnabled",&value);
        if(NT_SUCCESS(status) && value!=0) config->DeviceMode=NFC_CX_DEVICE_MODE_RAW;
    }
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status)?status:STATUS_SUCCESS;
}

NTSTATUS RegistryGetDeviceSettings(DEVICE_CONTEXT* device)
{
    WDFKEY key=nullptr;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,KEY_READ,
        WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        auto query=[key](PCWSTR literal,ULONG* value) {
            UNICODE_STRING name;RtlInitUnicodeString(&name,literal);
            return WdfRegistryQueryULong(key,&name,value);
        };
        ULONG value=0;
        if(NT_SUCCESS(query(L"SkipFirmwareDownload",&value))) device->FWSkipDownload=value!=0;
        if(NT_SUCCESS(query(L"ForceFirmwareDownload",&value))) device->FWForceDownload=value!=0;
        if(device->FWSkipDownload && device->FWForceDownload) device->FWForceDownload=FALSE;
        if(NT_SUCCESS(query(L"PollConfigInfo",&value)) && value<=0xff) device->PollConfig=value;
        if(NT_SUCCESS(query(L"NfcIP_Mode",&value)) && value<=0xff) device->NfcIPMode=static_cast<UCHAR>(value);
        if(NT_SUCCESS(query(L"NfcIP_Tgt_Mode",&value)) && value<=0xff) device->NfcIPTgtMode=static_cast<UCHAR>(value);
        if(NT_SUCCESS(query(L"NfcCE_Mode",&value)) && value<=0xff) device->NfcCEMode=static_cast<UCHAR>(value);
        if(NT_SUCCESS(query(L"Duration",&value)) && value/1000<=0xffff)
            device->TotalDuration=static_cast<USHORT>(value/1000);
        UNICODE_STRING name;RtlInitUnicodeString(&name,L"LLCPConfigParams");
        UCHAR llcp[4];ULONG length=0;
        status=WdfRegistryQueryValue(key,&name,sizeof(llcp),llcp,&length,nullptr);
        if(NT_SUCCESS(status)) {
            if(length==2 || length==3 || length==4) memcpy(&device->Miu,llcp,2);
            if(length==3 || length==4) device->LinkTimeout=llcp[2];
            if(length==4) device->RecvWindowSize=llcp[3];
        }
    }
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status)?status:STATUS_SUCCESS;
}

NTSTATUS RegistryReadPersistedDeviceParameters(DEVICE_CONTEXT* device)
{
    WDFKEY key=nullptr;
    NTSTATUS status=WdfDeviceOpenRegistryKey(device->FxDevice,
        PLUGPLAY_REGKEY_DEVICE|WDF_REGKEY_DRIVER_SUBKEY,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        WCHAR buffer[40];
        UNICODE_STRING value;
        value.Length=0;value.MaximumLength=78;value.Buffer=buffer;
        UNICODE_STRING name;RtlInitUnicodeString(&name,L"EEPROMWrittenSignature");
        if(NT_SUCCESS(WdfRegistryQueryUnicodeString(key,&name,&value.MaximumLength,&value)))
            (void)NfcUtilsGuidFromUnicodeString(&value,&device->EEPromWrittenSignature);
        ULONG attempts=0;
        RtlInitUnicodeString(&name,L"HWFailedAttempts");
        status=WdfRegistryQueryULong(key,&name,&attempts);
        if(NT_SUCCESS(status)) device->HWFailedAttempts=attempts;
    }
    if(key) WdfRegistryClose(key);
    return NT_SUCCESS(status)?status:STATUS_SUCCESS;
}

NTSTATUS RegistryWritePersistedDeviceParameters(DEVICE_CONTEXT* device)
{
    WDFKEY key=nullptr;
    NTSTATUS status=STATUS_SUCCESS;
    status=WdfDeviceOpenRegistryKey(device->FxDevice,
        PLUGPLAY_REGKEY_DEVICE|WDF_REGKEY_DRIVER_SUBKEY,GENERIC_ALL,WDF_NO_OBJECT_ATTRIBUTES,&key);
    if(NT_SUCCESS(status)) {
        WCHAR buffer[40];
        const GUID& guid=device->EEPromWrittenSignature;
        const HRESULT formatted=StringCchPrintfW(buffer,39,
            L"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
            guid.Data1,guid.Data2,guid.Data3,guid.Data4[0],guid.Data4[1],guid.Data4[2],
            guid.Data4[3],guid.Data4[4],guid.Data4[5],guid.Data4[6],guid.Data4[7]);
        if(FAILED(formatted)) {
            status=STATUS_INVALID_PARAMETER;
        } else {
            UNICODE_STRING name,value;
            RtlInitUnicodeString(&name,L"EEPROMWrittenSignature");
            RtlInitUnicodeString(&value,buffer);
            status=WdfRegistryAssignUnicodeString(key,&name,&value);
            if(NT_SUCCESS(status)) {
                RtlInitUnicodeString(&name,L"HWFailedAttempts");
                status=WdfRegistryAssignULong(key,&name,device->HWFailedAttempts);
            }
        }
    }
    // This close is present in the instructions at RVA 0x1129c even though the
    // decompiler incorrectly marked the block unreachable after indirect calls.
    if(key) WdfRegistryClose(key);
    return status;
}
