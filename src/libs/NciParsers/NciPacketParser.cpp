#include "nci_packet.h"
#include "nxp_trace.h"
#include "NciPacketParser.tmh"

// RVA 0x14528. This build has UINT length and no upper-length check. A valid
// data packet writes ConnId but preserves the union's second byte; callers must
// not consume that byte as a data field. Failure clears all five output bytes.
extern "C" BOOLEAN NciPacketHeaderGetFromBuffer(PUCHAR buffer, UINT length, PNCI_PACKET_HEADER header)
{
    BOOLEAN valid = FALSE;
    TRACE_FUNCTION_ENTRY(LEVEL_VERBOSE);
    if (length < 3) {
        TRACE_LINE(LEVEL_ERROR, "Buffer too small");
    } else if (buffer == nullptr) {
        TRACE_LINE(LEVEL_ERROR, "Buffer is NULL");
    } else {
        header->MessageType = buffer[0] >> 5;
        header->PBF = (buffer[0] >> 4) & 1;
        if (header->MessageType <= 3) {
            if (header->MessageType == 0) header->Header.DataPacket.ConnId = buffer[0] & 15;
            else {
                header->Header.ControlPacket.Gid = buffer[0] & 15;
                header->Header.ControlPacket.Oid = buffer[1] & 63;
            }
            header->PayloadLength = buffer[2];
            valid = TRUE;
        } else {
            TRACE_LINE(LEVEL_ERROR, "Unsupported MessageType, %d", header->MessageType);
        }
    }
    if (!valid) ZeroMemory(header, sizeof(*header));
    TRACE_FUNCTION_EXIT_DWORD(LEVEL_VERBOSE, valid);
    return valid;
}
