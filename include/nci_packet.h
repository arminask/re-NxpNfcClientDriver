#pragma once
#include <windows.h>

// PDB _NCI_PACKET_HEADER: five bytes, union at offset 2, length at offset 4.
typedef struct _NCI_PACKET_HEADER {
    UCHAR MessageType;
    UCHAR PBF;
    union {
        struct { UCHAR Gid; UCHAR Oid; } ControlPacket;
        struct { UCHAR ConnId; } DataPacket;
    } Header;
    UCHAR PayloadLength;
} NCI_PACKET_HEADER, *PNCI_PACKET_HEADER;

#ifdef __cplusplus
extern "C" {
#endif
BOOLEAN NciPacketHeaderGetFromBuffer(PUCHAR buffer, UINT length, PNCI_PACKET_HEADER header);
#ifdef __cplusplus
}
static_assert(sizeof(NCI_PACKET_HEADER)==5, "PDB header layout");
#endif
