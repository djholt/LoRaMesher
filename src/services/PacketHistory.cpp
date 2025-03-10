#include "PacketHistory.h"

PacketHistory::PacketHistory() {
    packets.reserve(MAX_HISTORY_NODES);
}

bool PacketHistory::wasSeen(const DataPacket *p) {
    if (p->id == 0) {
        return false;
    }

    PacketHistoryRecord r;
    r.id = p->id;
    r.sender = p->src;
    r.expiration = millis() + FLOOD_EXPIRE_TIME;

    auto result = packets.find(r);
    bool found = result != packets.end();

    if (found && result->expiration < millis()) {
        packets.erase(result); // Erase and pretend packet has not been seen recently
        result = packets.end();
        found = false;
    }
    if (found) {
        ESP_LOGV(LM_TAG, "Found existing packet record from %d with ID %d", result.sender, result.id);
        packets.erase(result);
    }
    
    packets.insert(r);

    if (packets.size() > MAX_HISTORY_NODES * 0.9) {
        clearExpiredRecentPackets();
    }

    return found;
}

void PacketHistory::clearExpiredRecentPackets() {
    ESP_LOGI(LM_TAG, "Removing expired records from packet history");
    for (auto record = packets.begin(); record != packets.end();) {
        if (record->expiration >= millis()) {
            packets.erase(record);
        } else {
            ++record;
        }
    }
}