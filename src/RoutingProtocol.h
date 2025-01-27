#ifndef ROUTING_PROTOCOL_H
#define ROUTING_PROTOCOL_H

#include "services/PacketQueueService.h"

// Forward declaration of LoraMesher
class LoraMesher;

class RoutingProtocol {
    public:
        explicit RoutingProtocol(LoraMesher* mesher); // Pass LoraMesher as a constructor argument
        virtual ~RoutingProtocol() = default;

        virtual void routeDataPacket(QueuePacket<DataPacket>* pq) = 0;
        virtual bool routeBeforeSend(QueuePacket<Packet<uint8_t>>* tx) = 0;

    protected:
        LoraMesher* mesher;
};

class VectorRouting : public RoutingProtocol {
    public:
        explicit VectorRouting(LoraMesher* mesher);
        void routeDataPacket(QueuePacket<DataPacket>* pq) override;
        bool routeBeforeSend(QueuePacket<Packet<uint8_t>>* tx) override;
};

class FloodingRouting : public RoutingProtocol {
    public:
        explicit FloodingRouting(LoraMesher* mesher);
        void routeDataPacket(QueuePacket<DataPacket>* pq) override;
        bool routeBeforeSend(QueuePacket<Packet<uint8_t>>* tx) override;
};

#endif // ROUTING_PROTOCOL_H
