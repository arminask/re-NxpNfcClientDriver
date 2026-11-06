#include "acpi.h"
#include "nxp_trace.h"
#include "acpi.tmh"

// Original constant at RVA 0x2e6d0, independently read through allowed MCP.
static const GUID ACPI_DSM_GUID_NFC={0xa2e7f6c4,0x9638,0x4485,{0x9f,0x12,0x6b,0x4e,0x20,0xb6,0x0d,0x63}};

void AcpiPrepareInputParametersForDsm(ACPI_EVAL_INPUT_BUFFER_COMPLEX* input,ULONG size,
    ULONG functionIndex,ULONG flags,ULONG argument3)
{
    memset(input,0,size);
    input->Signature=ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
    input->MethodNameAsUlong=0x4d53445f; // _DSM, little endian
    input->Size=size;input->ArgumentCount=4;
    PACPI_METHOD_ARGUMENT argument=input->Argument;
    ACPI_METHOD_SET_ARGUMENT_BUFFER(argument,&ACPI_DSM_GUID_NFC,sizeof(GUID));
    argument=ACPI_METHOD_NEXT_ARGUMENT(argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(argument,1);
    argument=ACPI_METHOD_NEXT_ARGUMENT(argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(argument,functionIndex);
    argument=ACPI_METHOD_NEXT_ARGUMENT(argument);
    argument->Type=(flags&1)?ACPI_METHOD_ARGUMENT_INTEGER:ACPI_METHOD_ARGUMENT_PACKAGE;
    argument->DataLength=sizeof(ULONG);argument->Argument=(flags&1)?argument3:0;
}

NTSTATUS AcpiFreeEEPROMConfig(DEVICE_CONTEXT*,UCHAR* buffer)
{
    if(buffer) (void)HeapFree(GetProcessHeap(),0,buffer);
    return STATUS_SUCCESS;
}

NTSTATUS AcpiGetEEPROMConfig(DEVICE_CONTEXT* device,ULONG* size,UCHAR** buffer)
{
    if(!device || !size || !buffer) return STATUS_INVALID_PARAMETER;
    *buffer=nullptr;*size=0;
    ACPI_EVAL_INPUT_BUFFER_COMPLEX* input=static_cast<ACPI_EVAL_INPUT_BUFFER_COMPLEX*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,60));
    if(!input) return STATUS_INSUFFICIENT_RESOURCES;
    AcpiPrepareInputParametersForDsm(input,60,3,0,0);
    ULONG capacity=sizeof(ACPI_EVAL_OUTPUT_BUFFER);
    auto output=static_cast<ACPI_EVAL_OUTPUT_BUFFER*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,capacity));
    NTSTATUS status=STATUS_INSUFFICIENT_RESOURCES;
    if(output) {
        WDF_MEMORY_DESCRIPTOR inDescriptor,outDescriptor;
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inDescriptor,input,60);
        WDF_REQUEST_SEND_OPTIONS options;WDF_REQUEST_SEND_OPTIONS_INIT(&options,0);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_SEC(2));
        ULONG_PTR received=0;
        for(unsigned attempt=0;attempt<2;++attempt) {
            WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outDescriptor,output,capacity);
            status=WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(device->FxDevice),nullptr,
                IOCTL_ACPI_EVAL_METHOD,&inDescriptor,&outDescriptor,&options,&received);
            if(status!=STATUS_BUFFER_OVERFLOW || attempt) break;
            // ACPI Length is the complete output extent, including its header.
            if(received<FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument) ||
               output->Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE || output->Length<=capacity || output->Length>65536) {
                status=STATUS_INVALID_BUFFER_SIZE;break;
            }
            capacity=output->Length;
            HeapFree(GetProcessHeap(),0,output);
            output=static_cast<ACPI_EVAL_OUTPUT_BUFFER*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,capacity));
            if(!output) { status=STATUS_INSUFFICIENT_RESOURCES;break; }
            received=0;
        }
        if(NT_SUCCESS(status)) {
            const size_t offset=FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+FIELD_OFFSET(ACPI_METHOD_ARGUMENT,Data);
            if(!output || received>capacity || received<offset || output->Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE ||
               output->Length>received || output->Length<offset || output->Count!=1 ||
               output->Argument[0].Type!=ACPI_METHOD_ARGUMENT_BUFFER || !output->Argument[0].DataLength ||
               output->Argument[0].DataLength>output->Length-offset) status=STATUS_INVALID_PARAMETER;
            else {
                ULONG length=output->Argument[0].DataLength;
                auto copy=static_cast<UCHAR*>(HeapAlloc(GetProcessHeap(),0,length));
                if(!copy) status=STATUS_INSUFFICIENT_RESOURCES;
                else { memcpy(copy,output->Argument[0].Data,length);*buffer=copy;*size=length; }
            }
        }
    }
    HeapFree(GetProcessHeap(),0,input);
    if(output) HeapFree(GetProcessHeap(),0,output);
    return status;
}

void NfcIsACPIAvailable(DEVICE_CONTEXT* device)
{
    UCHAR* buffer=nullptr;ULONG length=0;
    // Successful validation of the OEM function actually consumed by this
    // driver establishes availability, including small non-overflow replies.
    device->bIsACPIAvailableFlag=NT_SUCCESS(AcpiGetEEPROMConfig(device,&length,&buffer));
    AcpiFreeEEPROMConfig(device,buffer);
}
