#pragma once
#include "device.h"
#include <acpiioct.h>
NTSTATUS AcpiFreeEEPROMConfig(DEVICE_CONTEXT*,UCHAR*);
NTSTATUS AcpiGetEEPROMConfig(DEVICE_CONTEXT*,ULONG*,UCHAR**);
extern "C" void AcpiPrepareInputParametersForDsm(ACPI_EVAL_INPUT_BUFFER_COMPLEX*,ULONG,ULONG,ULONG,ULONG);
