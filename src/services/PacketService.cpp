#include "PacketService.h"

Packet<uint8_t>* PacketService::createEmptyPacket(size_t packetSize) {
    size_t maxPacketSize = PacketFactory::getMaxPacketSize();
    if (packetSize > maxPacketSize) {
        ESP_LOGI(LM_TAG, "Trying to create a packet greater than %d bytes", maxPacketSize);
        packetSize = maxPacketSize;
    }

    Packet<uint8_t>* p = static_cast<Packet<uint8_t>*>(pvPortMalloc(packetSize));

    ESP_LOGI(LM_TAG, "Packet created with %d bytes", packetSize);

    return p;

}

AppPacket<uint8_t>* PacketService::convertPacket(DataPacket* p) {
    uint32_t payloadSize = p->packetSize - sizeof(DataPacket);

    AppPacket<uint8_t>* uPacket = createAppPacket(p->dst, p->src, p->payload, payloadSize);
    return uPacket;
}

AppPacket<uint8_t>* PacketService::createAppPacket(uint16_t dst, uint16_t src, uint8_t* payload, uint32_t payloadSize) {
    int packetLength = sizeof(AppPacket<uint8_t>) + payloadSize;

    AppPacket<uint8_t>* p = static_cast<AppPacket<uint8_t>*>(pvPortMalloc(packetLength));

    if (p) {
        //Copy the payload into the packet
        memcpy(p->payload, payload, payloadSize);
    }
    else {
        ESP_LOGW(LM_TAG, "User Packet not allocated");
        return nullptr;
    }

    p->dst = dst;
    p->src = src;
    p->payloadSize = payloadSize;

    return p;
}

bool PacketService::isDataPacket(uint8_t type) {
    return (type & DATA_P) == DATA_P;
}

bool PacketService::isOnlyDataPacket(uint8_t type) {
    return type == DATA_P;
}

bool PacketService::isControlPacket(uint8_t type) {
    return !(isHelloPacket(type) || isOnlyDataPacket(type));
}

bool PacketService::isHelloPacket(uint8_t type) {
    return (type & HELLO_P) == HELLO_P;
}

bool PacketService::isNeedAckPacket(uint8_t type) {
    return (type & NEED_ACK_P) == NEED_ACK_P;
}

bool PacketService::isAckPacket(uint8_t type) {
    return (type & ACK_P) == ACK_P;
}

bool PacketService::isLostPacket(uint8_t type) {
    return (type & LOST_P) == LOST_P;
}

bool PacketService::isSyncPacket(uint8_t type) {
    return (type & SYNC_P) == SYNC_P;
}

bool PacketService::isXLPacket(uint8_t type) {
    return (type & XL_DATA_P) == XL_DATA_P;
}

bool PacketService::isDataControlPacket(uint8_t type) {
    return (isHelloPacket(type) || isAckPacket(type) || isLostPacket(type) || isLostPacket(type));
}

uint8_t PacketService::getHeaderLength(uint8_t type) {
    if (isControlPacket(type))
        return sizeof(ControlPacket);

    if (isDataPacket(type))
        return sizeof(DataPacket);

    return 0;
}

RoutePacket* PacketService::createRoutingPacket(uint16_t localAddress, NetworkNode* nodes, size_t numOfNodes, uint8_t nodeRole) {
    size_t routingSizeInBytes = numOfNodes * sizeof(NetworkNode);

    RoutePacket* routePacket = PacketFactory::createPacket<RoutePacket>(reinterpret_cast<uint8_t*>(nodes), routingSizeInBytes);
    routePacket->dst = BROADCAST_ADDR;
    routePacket->src = localAddress;
    routePacket->fwd = 0;
    routePacket->type = HELLO_P;
    routePacket->id = 0;
    routePacket->packetSize = routingSizeInBytes + sizeof(RoutePacket);
    routePacket->nodeRole = nodeRole;

    return routePacket;
}

DataPacket* PacketService::dataPacket(Packet<uint8_t>* p) {
    return reinterpret_cast<DataPacket*>(p);
}

ControlPacket* PacketService::controlPacket(Packet<uint8_t>* p) {
    return reinterpret_cast<ControlPacket*>(p);
}

ControlPacket* PacketService::createControlPacket(uint16_t dst, uint16_t src, uint8_t type, uint8_t* payload, uint8_t payloadSize) {
    ControlPacket* packet = PacketFactory::createPacket<ControlPacket>(payload, payloadSize);
    packet->dst = dst;
    packet->src = src;
    packet->fwd = 0;
    packet->type = type;
    packet->id = 0;
    packet->packetSize = payloadSize + sizeof(ControlPacket);

    return packet;
}

ControlPacket* PacketService::createEmptyControlPacket(uint16_t dst, uint16_t src, uint8_t type, uint8_t seq_id, uint16_t num_packets) {
    ControlPacket* packet = PacketFactory::createPacket<ControlPacket>(0, 0);
    packet->dst = dst;
    packet->src = src;
    packet->fwd = 0;
    packet->type = type;
    packet->id = 0;
    packet->seq_id = seq_id;
    packet->number = num_packets;
    packet->packetSize = sizeof(ControlPacket);

    return packet;
}

DataPacket* PacketService::createDataPacket(uint16_t dst, uint16_t src, uint8_t type, uint8_t* payload, uint8_t payloadSize, uint8_t maxHops) {
    static uint32_t lastPacketId = 0;

    DataPacket* packet = PacketFactory::createPacket<DataPacket>(payload, payloadSize);
    packet->dst = dst;
    packet->src = src;
    packet->fwd = 0;
    packet->type = type;
    packet->hops = maxHops;
    packet->id = ++lastPacketId;
    packet->packetSize = payloadSize + sizeof(DataPacket);

    ESP_LOGV(LM_TAG, "Creating data packet with ID %d Hops %d", packet->id, packet->hops);

    return packet;
}

size_t PacketService::getPacketPayloadLength(Packet<uint8_t>* p) {
    return p->packetSize - getHeaderLength(p);
}

size_t PacketService::getHeaderLength(Packet<uint8_t>* p) {
    return getHeaderLength(p->type);
}

size_t PacketService::getControlLength(Packet<uint8_t>* p) {
    if (isDataControlPacket(p->type))
        return p->packetSize;

    return getHeaderLength(p);
}

uint8_t PacketService::getMaximumPayloadLength(uint8_t type) {
    return PacketFactory::getMaxPacketSize() - getHeaderLength(type);
}

size_t PacketService::getPacketPayloadLengthWithoutControl(Packet<uint8_t>* p) {
    if (isDataControlPacket(p->type))
        return 0;
    return getPacketPayloadLength(p);
}

ControlPacket* PacketService::getPacketHeader(Packet<uint8_t>* p) {
    ControlPacket* ctrlPacket = new ControlPacket();

    if (isControlPacket(p->type)) {
        ControlPacket* srcCtrPacket = (ControlPacket*) p;
        memcpy(reinterpret_cast<void*>(ctrlPacket), reinterpret_cast<void*>(srcCtrPacket), sizeof(ControlPacket));
        return ctrlPacket;
    }
    if (isDataPacket(p->type)) {
        DataPacket* srcDataPacket = (DataPacket*) p;
        memcpy(reinterpret_cast<void*>(ctrlPacket), reinterpret_cast<void*>(srcDataPacket), sizeof(DataPacket));
        return ctrlPacket;
    }

    memcpy(reinterpret_cast<void*>(ctrlPacket), reinterpret_cast<void*>(p), sizeof(PacketHeader));
    return ctrlPacket;
}
