#include "RoutingProtocol.h"
#include "LoraMesher.h" // Include only in the .cpp file

RoutingProtocol::RoutingProtocol(LoraMesher* mesher) : mesher(mesher) {}

VectorRouting::VectorRouting(LoraMesher* mesher) : RoutingProtocol(mesher) {}
FloodingRouting::FloodingRouting(LoraMesher* mesher) : RoutingProtocol(mesher) {}

void VectorRouting::routeDataPacket(QueuePacket<DataPacket>* pq) {
    DataPacket* packet = pq->packet;

    if (packet->via == mesher->getLocalAddress()) {
        ESP_LOGV(LM_TAG, "Data Packet from %X for %X. Via is me. Forwarding it", packet->src, packet->dst);
        mesher->incReceivedIAmVia();
        mesher->addToSendOrderedAndNotify(reinterpret_cast<QueuePacket<Packet<uint8_t>>*>(pq));
    } else {
        ESP_LOGV(LM_TAG, "Packet not for me, deleting it");
        mesher->incReceivedNotForMe();
        PacketQueueService::deleteQueuePacketAndPacket(pq);
    }
}

bool VectorRouting::routeBeforeSend(QueuePacket<Packet<uint8_t>>* tx) {
    //If the packet has a data packet and its destination is not broadcast add the via to the packet and forward the packet
    if (PacketService::isDataPacket(tx->packet->type) && tx->packet->dst != BROADCAST_ADDR) {
        uint16_t nextHop = RoutingTableService::getNextHop(tx->packet->dst);

        //Next hop not found
        if (nextHop == 0) {
            ESP_LOGE(LM_TAG, "NextHop Not found from %X, destination %X", tx->packet->src, tx->packet->dst);
            PacketQueueService::deleteQueuePacketAndPacket(tx);
            mesher->incDestinyUnreachable();
            return false;
        }

        (reinterpret_cast<DataPacket*>(tx->packet))->via = nextHop;
    }
    return true;
}

void FloodingRouting::routeDataPacket(QueuePacket<DataPacket>* pq) {
    DataPacket* packet = pq->packet;

    if (packet->hopLimit <= 0) {
        ESP_LOGI(LM_TAG, "Packet is not for me and has reached hop limit. Dropping packet.");
        PacketQueueService::deleteQueuePacketAndPacket(pq);
    } else if (packet->via == BROADCAST_ADDR) {
        ESP_LOGV(LM_TAG, "Data Packet from %X for %X. Via is broadcast. Flooding!", packet->src, packet->dst);
        mesher->incReceivedIAmVia();
        mesher->addToSendOrderedAndNotify(reinterpret_cast<QueuePacket<Packet<uint8_t>>*>(pq));
    } else if (packet->via == mesher->getLocalAddress()) {
        ESP_LOGV(LM_TAG, "Data Packet from %X for %X. Via is me. Forwarding it", packet->src, packet->dst);
        mesher->incReceivedIAmVia();
        mesher->addToSendOrderedAndNotify(reinterpret_cast<QueuePacket<Packet<uint8_t>>*>(pq));
    } else {
        ESP_LOGV(LM_TAG, "Packet not for me, deleting it");
        mesher->incReceivedNotForMe();
        PacketQueueService::deleteQueuePacketAndPacket(pq);
    }
}

bool FloodingRouting::routeBeforeSend(QueuePacket<Packet<uint8_t>>* tx) {
    if (PacketService::isDataPacket(tx->packet->type) && tx->packet->dst != BROADCAST_ADDR) {
        if ((reinterpret_cast<DataPacket*>(tx->packet))->via == BROADCAST_ADDR) {
            (reinterpret_cast<DataPacket*>(tx->packet))->hopLimit -= 1;
        } else {
            uint16_t nextHop = RoutingTableService::getNextHop(tx->packet->dst);

            //Next hop not found
            if (nextHop == 0) {
                ESP_LOGE(LM_TAG, "NextHop Not found from %X, destination %X", tx->packet->src, tx->packet->dst);
                PacketQueueService::deleteQueuePacketAndPacket(tx);
                mesher->incDestinyUnreachable();
                return false;
            }

            (reinterpret_cast<DataPacket*>(tx->packet))->via = nextHop;
        }
    }
    return true;
}
