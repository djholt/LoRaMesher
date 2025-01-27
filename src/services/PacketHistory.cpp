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
    r.time = millis();

    auto result = packets.find(r);
    bool found = result != packets.end();
    if (found) {
        packets.erase(result);
    }
    packets.insert(r);

    if (packets.size() > MAX_HISTORY_NODES * 0.9) {
        ESP_LOGW(LM_TAG, "WARNING: packet history storage is almost full!");
    }

    return found;
}