#ifndef _LORAMESHER_PACKET_HISTORY_H
#define _LORAMESHER_PACKET_HISTORY_H

#include "BuildOptions.h"
#include "entities/packets/DataPacket.h"
#include <unordered_set>

#define FLOOD_EXPIRE_TIME 600000 // 10 minutes in milliseconds

struct PacketHistoryRecord {
    uint16_t sender;
    uint32_t id;
    uint32_t expiration;              // Time in ms when the record expires

    bool operator==(const PacketHistoryRecord &p) const { return sender == p.sender && id == p.id; }
};

class PacketHistoryRecordHashFunction {
public:
    size_t operator()(const PacketHistoryRecord &p) const {
        return (std::hash<uint32_t>()(p.sender)) ^ (std::hash<uint32_t>()(p.id));
    }
};

class PacketHistory {
public:
    PacketHistory();
    bool wasSeen(const DataPacket *p);

private:
    std::unordered_set<PacketHistoryRecord, PacketHistoryRecordHashFunction> packets;
    void clearExpiredRecentPackets();
};

#endif // _LORAMESHER_PACKET_HISTORY_H