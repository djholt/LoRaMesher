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

    if (found) {
        if (result->expiration < millis()) {
            packets.erase(result);
            found = false;
        } else {
            int old_id = result->id;
            int old_sender = result->sender;
            packets.erase(result);
            ESP_LOGV(LM_TAG, "Found existing packet record from %d with ID %d", old_sender, old_id);
        }
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
        if (record->expiration < millis()) {
            record = packets.erase(record); 
        } else {
            ++record;
        }
    }
    ESP_LOGI(LM_TAG, "Packet history size after clearing: %d", packets.size());
}