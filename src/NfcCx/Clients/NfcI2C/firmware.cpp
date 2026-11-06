#include "device.h"
#include "NfcProximityBusI2c.h"
#include "../NxpCoreLib/phDnldNfc.h"
#include "../NxpCoreLib/phNciNfc.h"
#include <string.h>
#include <stdlib.h>
#include "nxp_buffer_validation.h"
// UMDF already supplies NT/UNICODE_STRING types through WudfWdm.h.
// Use the installed SDK's safe-string inlines without reimporting kernel types.
#pragma push_macro("_NTDEF_")
#define _NTDEF_
#include <ntstrsafe.h>
#pragma pop_macro("_NTDEF_")
extern "C" {
#include <phOsalNfc.h>
}

// RVA 0x5298. Clear image state before releasing the shared download context.
void NfcFWFreeFullFWImage(DEVICE_CONTEXT* device)
{
    if(device->FirmwareImage.buffer) {
        free(device->FirmwareImage.buffer);
        device->FirmwareImage.buffer=nullptr;
    }
    device->FirmwareImage.length=0;
    device->FWVersion=0;
    phDnldNfc_FreeDnldContext();
}

// RVA 0x5ef8. Serialize read-queue disposal and state transitions with the ISR.
NTSTATUS NfcFWSetMode(DEVICE_CONTEXT* device,UCHAR mode)
{
    if(mode>1) return STATUS_INVALID_PARAMETER;
    if(device->TransportPlatfrom!=TRANSPORT_SPBI2C || !device->m_pNfcProximityBus || !device->ReadInterrupt) return STATUS_NOT_SUPPORTED;
    auto bus=static_cast<CNfcProximityBusI2C*>(device->m_pNfcProximityBus);
    WdfInterruptAcquireLock(device->ReadInterrupt);
    auto head=&device->ReadList;
    while(head->Flink!=head) {
        auto read=reinterpret_cast<READ_REQUEST_CONTEXT*>(head->Flink);
        head->Flink=read->ListEntry.Flink;
        read->ListEntry.Flink->Blink=head;
        if(read->Request) WdfObjectDelete(read->Request);
    }
    device->DeviceState=DEVICE_STATE_UPDATING_MODE;
    WdfInterruptReleaseLock(device->ReadInterrupt);

    NTSTATUS status=STATUS_SUCCESS;
    if(mode==0 || mode==1) {
        status=bus->SpbGpioWrite(device->FxDevice,0,bus->VenPinIoTarget);
        if(NT_SUCCESS(status)) {
            status=bus->SpbGpioWrite(device->FxDevice,mode,bus->DownloadPinIoTarget);
            if(NT_SUCCESS(status)) status=bus->SpbGpioWrite(device->FxDevice,1,bus->VenPinIoTarget);
            if(NT_SUCCESS(status)) device->DeviceMode=mode==1?DEVICE_MODE_FIRMWARE:DEVICE_MODE_NCI;
            else (void)bus->SpbGpioWrite(device->FxDevice,0,bus->VenPinIoTarget);
        }
    }
    WdfInterruptAcquireLock(device->ReadInterrupt);
    device->DeviceState=NT_SUCCESS(status)?DEVICE_STATE_WORKING:DEVICE_STATE_UPDATING_MODE;
    WdfInterruptReleaseLock(device->ReadInterrupt);
    return status;
}

// Publish only a completely read and structurally valid image. The download
// engine has a 16-bit image cursor; reject files that cannot be represented.
NTSTATUS NfcFWReadFullFWImage(DEVICE_CONTEXT* device)
{
    if(!device->FirmwareFileName) return STATUS_INVALID_DEVICE_STATE;
    UNICODE_STRING path={};WdfStringGetUnicodeString(device->FirmwareFileName,&path);
    HANDLE file=CreateFileW(path.Buffer,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return STATUS_OBJECT_NAME_NOT_FOUND;
    NTSTATUS status=STATUS_INVALID_IMAGE_FORMAT;
    LARGE_INTEGER size={};UCHAR* image=nullptr;
    if(GetFileSizeEx(file,&size) && size.QuadPart>6 && size.QuadPart<=UINT16_MAX) {
        image=static_cast<UCHAR*>(malloc(static_cast<size_t>(size.QuadPart)));
        if(!image) status=STATUS_INSUFFICIENT_RESOURCES;
        else {
            DWORD received=0;
            if(!ReadFile(file,image,static_cast<DWORD>(size.QuadPart),&received,nullptr) || received!=size.QuadPart)
                status=STATUS_IO_DEVICE_ERROR;
            else if(NxpValidateFirmwareImage(image,received)) {
                free(device->FirmwareImage.buffer);
                device->FirmwareImage.buffer=image;device->FirmwareImage.length=received;
                image=nullptr;status=STATUS_SUCCESS;
            }
        }
    }
    free(image);CloseHandle(file);
    return status;
}

// RVA 0x4d60. Input status/sequence arguments are unused by this step.
NTSTATUS NfcFWDnldSetup(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    memset(&device->FWDnldContext,0,sizeof(device->FWDnldContext));
    device->FirmwareRecoveryCycles=0;device->FirmwareLogPending=FALSE;
    NTSTATUS status=NfcFWReadFullFWImage(device);
    if(NT_SUCCESS(status)) {
        status=NfcFWSetMode(device,1);
        if(NT_SUCCESS(status)) {
            AcquireSRWLockExclusive(&device->ControllerInfoLock);
            if(device->pNciHandle) {
                (void)phNciNfc_Deinitialise(device->pNciHandle);
                device->pNciHandle=nullptr;
            }
            ReleaseSRWLockExclusive(&device->ControllerInfoLock);
            status=phOsalNfc_NtStatusFromNfcStatus(phDnldNfc_SetHwDevHandle(device));
        }
    }
    return status;
}

// RVA 0x52d8. Resolve ROM-specific registry value, inspect its six-byte header.
NTSTATUS NfcFWGetVersion(DEVICE_CONTEXT* device,ULONG romVersion)
{
    NTSTATUS status=STATUS_SUCCESS;
    WDFKEY deviceKey=nullptr,mapKey=nullptr;
    WCHAR* expanded=nullptr;
    UCHAR* header=nullptr;
    HANDLE file=INVALID_HANDLE_VALUE;
    WDFSTRING mappedPath=nullptr;
    do {
        status=WdfDeviceOpenRegistryKey(device->FxDevice,PLUGPLAY_REGKEY_DEVICE,
            KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&deviceKey);
        if(!NT_SUCCESS(status)) break;
        UNICODE_STRING mapName;
        status=RtlUnicodeStringInit(&mapName,L"FirmwareMap");
        if(!NT_SUCCESS(status)) break;
        status=WdfRegistryOpenKey(deviceKey,&mapName,KEY_READ,WDF_NO_OBJECT_ATTRIBUTES,&mapKey);
        if(!NT_SUCCESS(status)) break;
        if(romVersion==0xffffffffUL) {
            WCHAR soleName[MAX_PATH]={};DWORD nameCharacters=MAX_PATH,valueCount=0;
            status=STATUS_UNSUCCESSFUL;
            if(RegQueryInfoKeyW(static_cast<HKEY>(WdfRegistryWdmGetHandle(mapKey)),nullptr,nullptr,nullptr,nullptr,
                nullptr,nullptr,&valueCount,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS || valueCount!=1) break;
            if(RegEnumValueW(static_cast<HKEY>(WdfRegistryWdmGetHandle(mapKey)),0,soleName,&nameCharacters,nullptr,
                nullptr,nullptr,nullptr)!=ERROR_SUCCESS) break;
            WCHAR* end=nullptr;
            romVersion=wcstoul(soleName,&end,16);
            if(end==soleName || *end || romVersion>255) break;
        }
        WCHAR numberBuffer[9]={};
        UNICODE_STRING number={0,sizeof(numberBuffer),numberBuffer};
        status=RtlIntegerToUnicodeString(romVersion,16,&number);
        if(!NT_SUCCESS(status)) break;
        WDF_OBJECT_ATTRIBUTES attributes;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject=device->FxDevice;

        status=WdfStringCreate(nullptr,&attributes,&mappedPath);
        if(!NT_SUCCESS(status)) break;
        status=WdfRegistryQueryString(mapKey,&number,mappedPath);
        if(!NT_SUCCESS(status)) break;
        UNICODE_STRING path;
        WdfStringGetUnicodeString(mappedPath,&path);
        status=STATUS_UNSUCCESSFUL;
        DWORD required=ExpandEnvironmentStringsW(path.Buffer,nullptr,0);
        if(required==0 || required>=MAX_PATH) break;
        expanded=static_cast<WCHAR*>(malloc(static_cast<size_t>(required)*sizeof(WCHAR)));
        if(!expanded) break;
        if(!ExpandEnvironmentStringsW(path.Buffer,expanded,required)) break;
        file=CreateFileW(expanded,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) break;
        LARGE_INTEGER size;
        if(!GetFileSizeEx(file,&size) || size.QuadPart<=6 || size.QuadPart>UINT16_MAX) break;
        header=static_cast<UCHAR*>(malloc(6));
        if(!header) break;
        DWORD received=0;
        if(!ReadFile(file,header,6,&received,nullptr) || received!=6) break;
        device->FWVersion=static_cast<USHORT>(header[4]|(static_cast<USHORT>(header[5])<<8));
        UNICODE_STRING expandedPath;
        status=RtlUnicodeStringInit(&expandedPath,expanded);
        if(!NT_SUCCESS(status)) break;
        WDFSTRING resolved=nullptr;
        status=WdfStringCreate(&expandedPath,&attributes,&resolved);
        if(NT_SUCCESS(status)) { if(device->FirmwareFileName) WdfObjectDelete(device->FirmwareFileName);device->FirmwareFileName=resolved; }
        if(NT_SUCCESS(status)) status=STATUS_SUCCESS;
    } while(false);
    if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
    if(expanded) free(expanded);
    if(header) free(header);
    if(mappedPath) WdfObjectDelete(mappedPath);
    if(mapKey) WdfRegistryClose(mapKey);
    if(deviceKey) WdfRegistryClose(deviceKey);
    return status;
}

// RVA 0x3be8. Commit the one-shot check only after dependencies succeed.
UCHAR NfcFWCheckFWDownload(DEVICE_CONTEXT* device)
{
    if(device->FWCheckComplete) return FALSE;

    phNciNfc_NfccCapabilities_t capabilities={};
    if(!NT_SUCCESS(NfcGetControllerCapabilities(device,&capabilities))) return FALSE;
    device->HWVersion=capabilities.HwVersion;
    if(!NT_SUCCESS(NfcFWGetVersion(device,device->ROMVersion))) return FALSE;
    device->FWCheckComplete=TRUE;
    return device->FirmwareFileName && !device->FWSkipDownload &&
        (device->FWForceDownload || device->FWVersion!=capabilities.FwVersion)?TRUE:FALSE;
}

// RVA 0x4330. Successful notifications require a readable Buff structure.
static void NfcFWDnldGetSessionStateCB(void* context,NFCSTATUS status,void* info)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(status==NFCSTATUS_SUCCESS) {
        auto response=static_cast<phDnldNfc_Buff_t*>(info);
        if(!response || response->wLen!=3 || !response->pBuff || response->pBuff[2]!=0x11) {
            status=NFCSTATUS_FAILED;
        } else if(response->pBuff[0]==1) {
            device->FWDnldContext.PrevSessionOpen=1;
            if(device->FWDnldContext.DnldInitiated) {
                status=NFCSTATUS_FAILED;
                device->FWDnldContext.RetryDnld=device->FWDnldContext.DnldAttempts<3?1:0;
            }
        } else {
            device->FWDnldContext.PrevSessionOpen=0;
        }
    }
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldGetSessionState(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    // PDB local static result: array3, data RVA0x338f8. Shared across requests.
    static UCHAR result[3];
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(!device->FWDnldContext.SkipSequence) {
        phDnldNfc_Buff_t response={result,sizeof(result)};
        status=phDnldNfc_GetSessionState(&response,NfcFWDnldGetSessionStateCB,device);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}

// RVA 0x4630. Require the complete nine-byte command response for known ROMs;
// allocation capacity alone does not validate the response schema.
static void NfcFWDnldGetVersionCB(void* context,NFCSTATUS status,void* info)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(status==NFCSTATUS_SUCCESS) {
        auto response=static_cast<phDnldNfc_Buff_t*>(info);
        if(!response || response->wLen!=9 || !response->pBuff) {
            status=NFCSTATUS_FAILED;
        } else {
            const unsigned rom=response->pBuff[1];
            device->FWDnldContext.NFCCROMVersion=static_cast<_phDnldNfc_NFCC_ROM_VERSION>(rom);
            if(rom!=8 && (rom<16 || rom>18)) {
                status=NFCSTATUS_FAILED;
            } else if(device->FWDnldContext.DnldInitiated) {
                if(!NxpValidateFirmwareImage(device->FirmwareImage.buffer,device->FirmwareImage.length)) {
                    NfcInternalSequence(device,device->pSeqHandler,STATUS_INVALID_IMAGE_FORMAT,nullptr,nullptr);return;
                }
                device->FWDnldContext.EEPromUserData.MwVersion=0x0300;
                const auto image=device->FirmwareImage.buffer;
                device->FWDnldContext.EEPromUserData.FwVersion=static_cast<USHORT>(image[4]|(static_cast<USHORT>(image[5])<<8));
            }
        }
    }
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldGetVersion(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    // PDB local static result: array11, data RVA0x33900.
    static UCHAR result[11];
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(!device->FWDnldContext.SkipSequence && !device->FWDnldContext.PrevSessionOpen) {
        phDnldNfc_Buff_t response={result,sizeof(result)};
        status=phDnldNfc_GetVersion(&response,NfcFWDnldGetVersionCB,device);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
#ifdef NXP_FIRMWARE_NATIVE_TEST
void NxpFirmwareTestSessionCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldGetSessionStateCB(context,status,info); }
void NxpFirmwareTestVersionCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldGetVersionCB(context,status,info); }
#endif

// RVA 0x4cb0. Response data is unused; status conversion masks component bits.
static void NfcFWDnldResetCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldReset(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(device->FWDnldContext.SkipSequence==1 || device->FWDnldContext.SkipReset==1) {
        device->FWDnldContext.SkipReset=0;
    } else {
        status=phDnldNfc_Reset(NfcFWDnldResetCB,device);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}

// RVA 0x4160. Propagate force failures so bounded recovery owns the outcome.
static void NfcFWDnldForceCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(status==NFCSTATUS_SUCCESS) {
        device->FWDnldContext.DnldRecovery=0;
        device->FWDnldContext.RetryDnld=1;
        device->FWDnldContext.SkipReset=1;
    }
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldForce(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    phDnldNfc_Buff_t inputs={};
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(!device->FWDnldContext.SkipForce) {
        status=phDnldNfc_Force(&inputs,NfcFWDnldForceCB,device);
        if(status!=NFCSTATUS_PENDING) {
            device->FWDnldContext.SkipForce=0;
            device->FWDnldContext.RetryDnld=0;
        }
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
#ifdef NXP_FIRMWARE_NATIVE_TEST
void NxpFirmwareTestResetCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldResetCB(context,status,info); }
void NxpFirmwareTestForceCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldForceCB(context,status,info); }
#endif

// RVA 0x4950. C ReadLog has already copied directly into the device metadata.
static void NfcFWDnldReadEEPromUserDataCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldReadEEPromUserData(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    auto& download=device->FWDnldContext;
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    const bool skip=((download.SkipSequence==1 || download.ForceDnld==1) && download.PrevSessionOpen==0) ||
        (download.PrevSessionOpen==1 && download.RetryDnld==1);
    if(!skip) {
        phDnldNfc_Buff_t response={reinterpret_cast<UCHAR*>(&download.EEPromUserData),sizeof(download.EEPromUserData)};
        status=phDnldNfc_ReadLog(&response,NfcFWDnldReadEEPromUserDataCB,device);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}

static void NfcFWDnldUpdateEEPromUserDataCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    const NTSTATUS result=phOsalNfc_NtStatusFromNfcStatus(status);
    // Logging failure must terminate this attempt rather than recursively log.
    device->FWDnldContext.DnldInitiated=0;
    if(!NT_SUCCESS(result)) { device->FWDnldContext.RetryDnld=0;device->FWDnldContext.DnldRecovery=0;device->FWDnldContext.DnldAttempts=3; }
    NfcInternalSequence(device,device->pSeqHandler,result,nullptr,nullptr);
}
NTSTATUS NfcFWDnldUpdateEEPromUserData(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    auto& download=device->FWDnldContext;
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if((download.SkipSequence!=1 && download.ForceDnld!=1) || download.DnldInitiated) {
        phDnldNfc_Buff_t data={reinterpret_cast<UCHAR*>(&download.EEPromUserData),sizeof(download.EEPromUserData)};
        status=phDnldNfc_Log(&data,NfcFWDnldUpdateEEPromUserDataCB,device);
        if(status!=NFCSTATUS_PENDING) download.DnldInitiated=0;
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
#ifdef NXP_FIRMWARE_NATIVE_TEST
void NxpFirmwareTestReadEepromCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldReadEEPromUserDataCB(context,status,info); }
void NxpFirmwareTestUpdateEepromCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldUpdateEEPromUserDataCB(context,status,info); }
#endif

// RVA 0x3f10. The callback uses device ROMVersion, whereas the command uses
// FWDnldContext.NFCCROMVersion. Only the first response byte is inspected.
static void NfcFWDnldCheckIntegrityCB(void* context,NFCSTATUS status,void* info)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    if(status==NFCSTATUS_SUCCESS) {
        auto response=static_cast<phDnldNfc_Buff_t*>(info);
        if(!response || !response->wLen || !response->pBuff) {
            status=NFCSTATUS_FAILED;
        } else {
            const bool newerRom=static_cast<ULONG>(device->ROMVersion)-16<=2;
            const UCHAR required=newerRom?0x7e:0x7f;
            if((response->pBuff[0]&required)!=required) status=NFCSTATUS_FAILED;
        }
    }
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldCheckIntegrity(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    // PDB static array31, data RVA0x33910. Shared, as in the original.
    static UCHAR result[31];
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(!device->FWDnldContext.SkipSequence && !device->FWDnldContext.PrevSessionOpen) {
        phDnldNfc_Buff_t response={result,sizeof(result)};
        status=phDnldNfc_CheckIntegrity(device->FWDnldContext.NFCCROMVersion,&response,NfcFWDnldCheckIntegrityCB,device);
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}

// RVA 0x5120. A nonzero NFC status, including tagged success, becomes FAILED.
static void NfcFWDnldWriteCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    auto& download=device->FWDnldContext;
    auto& metadata=download.EEPromUserData;
    if(status==NFCSTATUS_SUCCESS) {
        if(download.DnldInitiated) {
            ++metadata.NumOfDnldSuccess;
            metadata.DnldFailCount=0;
            if(!metadata.DnldFlag) metadata.DnldFlag=1;
        }
        download.DnldAttemptFailed=0;
    } else {
        if(download.DnldInitiated) {
            ++metadata.NumOfDnldFailed;
            ++metadata.DnldFailCount;
            metadata.DnldFlag=0;
        }
        if(status==0x15) {
            download.SkipSequence=1;
            download.DnldRecovery=1;
        }
        status=NFCSTATUS_FAILED;
    }
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldWrite(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    auto& download=device->FWDnldContext;
    auto& metadata=download.EEPromUserData;
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    download.RetryDnld=0;
    if(download.SkipSequence!=1 || download.PrevSessionOpen) {
        if(!download.ForceDnld) {
            ++download.DnldAttempts;
            ++metadata.NumOfDnldAttempted;
            download.DnldInitiated=1;
        }
        status=phDnldNfc_Write(0,&device->FirmwareImage,NfcFWDnldWriteCB,device);
        if(!download.ForceDnld && status!=NFCSTATUS_PENDING) {
            ++metadata.NumOfDnldFailed;
            ++metadata.DnldFailCount;
            metadata.DnldFlag=0;
        }
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}

static void NfcFWDnldRecoveryCB(void* context,NFCSTATUS status,void*)
{
    auto device=static_cast<DEVICE_CONTEXT*>(context);
    auto& download=device->FWDnldContext;
    if(status==NFCSTATUS_SUCCESS && !download.SkipForce) download.RetryDnld=1;
    download.DnldRecovery=0;
    download.SkipForce=0;
    NfcInternalSequence(device,device->pSeqHandler,phOsalNfc_NtStatusFromNfcStatus(status),nullptr,nullptr);
}
NTSTATUS NfcFWDnldRecovery(DEVICE_CONTEXT* device,NTSTATUS,void*,void*)
{
    auto& download=device->FWDnldContext;
    NFCSTATUS status=NFCSTATUS_SUCCESS;
    if(download.DnldRecovery && download.DnldAttempts>=3) return STATUS_IO_DEVICE_ERROR;
    if(download.DnldRecovery) {
        download.DnldRecovery=0;
        ++download.DnldAttempts;
        status=phDnldNfc_Write(1,&device->FirmwareImage,NfcFWDnldRecoveryCB,device);
        if(status!=NFCSTATUS_PENDING) {
            download.SkipForce=0;
            download.RetryDnld=0;
        }
    }
    return phOsalNfc_NtStatusFromNfcStatus(status);
}
#ifdef NXP_FIRMWARE_NATIVE_TEST
void NxpFirmwareTestIntegrityCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldCheckIntegrityCB(context,status,info); }
void NxpFirmwareTestWriteCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldWriteCB(context,status,info); }
void NxpFirmwareTestRecoveryCB(void* context,NFCSTATUS status,void* info)
{ NfcFWDnldRecoveryCB(context,status,info); }
#endif
