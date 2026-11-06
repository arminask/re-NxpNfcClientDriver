#pragma once

// Provider GUID from the supplied MOF. TraceCommon is pinned published Microsoft
// infrastructure; generated TMH message GUIDs/IDs may differ from the reference.
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(NfcI2CTraceGuid, (1b208304,26f2,433d,8781,0512d91b101c), \
        WPP_DEFINE_BIT(DUMMY))
#include "tracecommon.h"
