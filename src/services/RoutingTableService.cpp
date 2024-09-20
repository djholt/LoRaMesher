#include "RoutingTableService.h"

size_t RoutingTableService::routingTableSize() {
    return routingTableList->getLength();
}

RouteNode* RoutingTableService::findNode(uint16_t address, bool block_routing_table) {

    if (block_routing_table)
        routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.address == address) {
                if (block_routing_table)
                    routingTableList->releaseInUse();
                return node;
            }

        } while (routingTableList->next());
    }

    if (block_routing_table)
        routingTableList->releaseInUse();

    return nullptr;
}

RouteNode* RoutingTableService::getBestNodeByRole(uint8_t role) {
    RouteNode* bestNode = nullptr;

    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if ((node->networkNode.role & role) == role &&
                (bestNode == nullptr || node->networkNode.metric < bestNode->networkNode.metric)) {
                bestNode = node;
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
    return bestNode;
}

bool RoutingTableService::hasAddressRoutingTable(uint16_t address) {
    RouteNode* node = findNode(address);
    return node != nullptr;
}

uint16_t RoutingTableService::getNextHop(uint16_t dst) {
    RouteNode* node = findNode(dst);

    if (node == nullptr)
        return 0;

    return node->via;
}

uint8_t RoutingTableService::getNumberOfHops(uint16_t address) {
    RouteNode* node = findNode(address);

    if (node == nullptr)
        return 0;

    return node->networkNode.metric;
}

HelloPacketNode* RoutingTableService::findHelloPacketNode(HelloPacket* helloPacketNode, uint16_t address) {
    for (size_t i = 0; i < helloPacketNode->getHelloPacketNodesSize(); i++) {
        if (helloPacketNode->helloPacketNodes[i].address == address) {
            return &helloPacketNode->helloPacketNodes[i];
        }
    }

    return nullptr;
}

uint8_t RoutingTableService::get_transmitted_link_quality(HelloPacket* helloPacket) {
    HelloPacketNode* helloPacketNode = findHelloPacketNode(helloPacket, WiFiService::getLocalAddress());
    if (helloPacketNode == nullptr)
        return LM_MAX_METRIC;

    return helloPacketNode->received_link_quality;
}

void RoutingTableService::updateMetricOfNextHop(RouteNode* rNode) {
    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->via == rNode->networkNode.address && node->networkNode.address != rNode->networkNode.address) {
                updateMetric(node, node->networkNode.hop_count, rNode->received_link_quality, rNode->transmitted_link_quality);
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
}

bool RoutingTableService::updateMetric(RouteNode* rNode, uint8_t hops, uint8_t rlq, uint8_t tlq) {
    bool updated = false;
    if (rNode->networkNode.hop_count != hops) {
        rNode->networkNode.hop_count = hops;
        updated = true;
    }

    uint8_t factor_hops = LM_REDUCED_FACTOR_HOP_COUNT * hops * LM_MAX_METRIC;

    printf("Factor hops: %d\n", factor_hops);

    uint8_t quality_link = (rlq + tlq) / 2;

    printf("Received link quality: %d\n", rlq);
    printf("Transmitted link quality: %d\n", tlq);

    printf("Quality link: %d\n", quality_link);

    uint8_t factor_link_quality = LM_MAX_METRIC / std::sqrt((LM_MAX_METRIC / rNode->receivedMetric) ^ 2 + (LM_MAX_METRIC / quality_link) ^ 2);

    printf("Factor link quality: %d\n", factor_link_quality);

    // Update the received link quality
    uint8_t new_metric = std::min(factor_hops, factor_link_quality);

    printf("New metric: %d\n", new_metric);

    // TODO: Use some kind of hysteresis to avoid oscillations?
    if (rNode->networkNode.metric != new_metric) {
        rNode->networkNode.metric = new_metric;
        updated = true;
    }

    return updated;

}

bool RoutingTableService::processHelloPacket(HelloPacket* p, int8_t receivedSNR, Packet<uint8_t>** out_send_packet) {
    ESP_LOGI(LM_TAG, "Hello packet from %X", p->src);

    if (p->routingTableId < routingTableId) {
        ESP_LOGI(LM_TAG, "Hello packet from %X with old routing table id %d", p->src, p->routingTableId);
        return false;
    }

    if (p->routingTableId > routingTableId || p->routingTableSize != routingTableSize()) {
        ESP_LOGI(LM_TAG, "Hello packet from %X with different routing table id %d", p->src, p->routingTableId);
        // Ask for the updated routing table
        RTRequestPacket* rtRequestPacket = PacketService::createRoutingTableRequestPacket(p->src, WiFiService::getLocalAddress());

        *out_send_packet = reinterpret_cast<Packet<uint8_t>*>(rtRequestPacket);

        return false;
    }

    bool updated = false;

    uint8_t transmitted_link_quality = get_transmitted_link_quality(p);

    printf("Transmitted link quality: %d\n", transmitted_link_quality);

    routingTableList->setInUse();

    uint8_t factor_hops = LM_REDUCED_FACTOR_HOP_COUNT * 1 * LM_MAX_METRIC;

    bool block_routing_table = false;
    RouteNode* rNode = findNode(p->src, block_routing_table);
    if (rNode == nullptr) {
        rNode = new RouteNode(p->src, factor_hops, ROLE_DEFAULT, p->src, 1, LM_MAX_METRIC, transmitted_link_quality, LM_MAX_METRIC);
        resetTimeoutRoutingNode(rNode);

        // Add to the routing table
        routingTableList->Append(rNode);
        routingTableList->releaseInUse();

        routingTableId = p->routingTableId + 1;

        updated = true;
    }
    else {
        rNode->transmitted_link_quality = transmitted_link_quality;
        rNode->hasReceivedHelloPacket = true;

        bool updated = updateMetric(rNode, 1, rNode->received_link_quality, transmitted_link_quality);

        resetTimeoutRoutingNode(rNode);
        rNode->receivedSNR = receivedSNR;

        routingTableList->releaseInUse();

        if (updated) {
            updateMetricOfNextHop(rNode);
        }

        printRoutingTable();
    }

    return updated;
}

void RoutingTableService::processRoute(RoutePacket* p, int8_t receivedSNR) {
    // TODO: Implement
    if ((p->packetSize - sizeof(RoutePacket)) % sizeof(NetworkNode) != 0) {
        ESP_LOGE(LM_TAG, "Invalid route packet size");
        return;
    }

    size_t numNodes = p->getNetworkNodesSize();
    ESP_LOGI(LM_TAG, "Route packet from %X with size %d", p->src, numNodes);

    // NetworkNode* receivedNode = new NetworkNode(p->src, 1, p->nodeRole);
    // processRoute(p->src, receivedNode);
    // delete receivedNode;

    // resetReceiveSNRRoutePacket(p->src, receivedSNR);

    // for (size_t i = 0; i < numNodes; i++) {
    //     NetworkNode* node = &p->networkNodes[i];
    //     node->metric++;
    //     processRoute(p->src, node);
    // }

    // printRoutingTable();
}

void RoutingTableService::resetReceiveSNRRoutePacket(uint16_t src, int8_t receivedSNR) {
    RouteNode* rNode = findNode(src);
    if (rNode == nullptr)
        return;

    ESP_LOGI(LM_TAG, "Reset Receive SNR from %X: %d", src, receivedSNR);

    rNode->receivedSNR = receivedSNR;
}

void RoutingTableService::processRoute(uint16_t via, NetworkNode* node) {
    if (node->address != WiFiService::getLocalAddress()) {

        RouteNode* rNode = findNode(node->address);
        //If nullptr the node is not inside the routing table, then add it
        if (rNode == nullptr) {
            addNodeToRoutingTable(node, via);
            return;
        }

        //Update the metric and restart timeout if needed
        if (node->metric < rNode->networkNode.metric) {
            rNode->networkNode.metric = node->metric;
            rNode->via = via;
            resetTimeoutRoutingNode(rNode);
            ESP_LOGI(LM_TAG, "Found better route for %X via %X metric %d", node->address, via, node->metric);
        }
        else if (node->metric == rNode->networkNode.metric) {
            //Reset the timeout, only when the metric is the same as the actual route.
            resetTimeoutRoutingNode(rNode);
        }

        // Update the Role only if the node that sent the packet is the next hop
        if (getNextHop(node->address) == via && node->role != rNode->networkNode.role) {
            ESP_LOGI(LM_TAG, "Updating role of %X to %d", node->address, node->role);
            rNode->networkNode.role = node->role;
        }
    }
}

void RoutingTableService::addNodeToRoutingTable(NetworkNode* node, uint16_t via) {
    // TODO: implement
    ESP_LOGE(LM_TAG, "TODO: NOT IMPLEMENTED");
    return;
    // if (routingTableList->getLength() >= RTMAXSIZE) {
    //     ESP_LOGW(LM_TAG, "Routing table max size reached, not adding route and deleting it");
    //     return;
    // }

    // if (calculateMaximumMetricOfRoutingTable() < node->metric) {
    //     ESP_LOGW(LM_TAG, "Trying to add a route with a metric higher than the maximum of the routing table, not adding route and deleting it");
    //     return;
    // }

    // RouteNode* rNode = new RouteNode(node->address, node->metric, node->role, via);

    // //Reset the timeout of the node
    // resetTimeoutRoutingNode(rNode);

    // routingTableList->setInUse();

    // routingTableList->Append(rNode);

    // routingTableList->releaseInUse();

    ESP_LOGI(LM_TAG, "New route added: %X via %X metric %d, role %d", node->address, via, node->metric, node->role);
}

NetworkNode* RoutingTableService::getAllNetworkNodes() {
    routingTableList->setInUse();

    int routingSize = routingTableSize();

    // If the routing table is empty return nullptr
    if (routingSize == 0) {
        routingTableList->releaseInUse();
        return nullptr;
    }

    NetworkNode* payload = new NetworkNode[routingSize];

    if (routingTableList->moveToStart()) {
        for (int i = 0; i < routingSize; i++) {
            RouteNode* currentNode = routingTableList->getCurrent();
            payload[i] = currentNode->networkNode;

            if (!routingTableList->next())
                break;
        }
    }

    routingTableList->releaseInUse();

    return payload;
}

void RoutingTableService::resetTimeoutRoutingNode(RouteNode* node) {
    node->timeout = millis() + DEFAULT_TIMEOUT * 1000;
}

void RoutingTableService::printRoutingTable() {
    ESP_LOGI(LM_TAG, "Current routing table:");

    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        size_t position = 0;

        do {
            RouteNode* node = routingTableList->getCurrent();

            ESP_LOGI(LM_TAG, "%d - %X via %X metric %d hop_count %d role %d",
                position,
                node->networkNode.address,
                node->via,
                node->networkNode.metric,
                node->networkNode.hop_count,
                node->networkNode.role);

            position++;
        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
}

void RoutingTableService::manageTimeoutRoutingTable() {
    ESP_LOGI(LM_TAG, "Checking routes timeout");

    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->timeout < millis()) {
                ESP_LOGW(LM_TAG, "Route timeout %X via %X", node->networkNode.address, node->via);

                delete node;
                routingTableList->DeleteCurrent();
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();

    printRoutingTable();
}

uint8_t RoutingTableService::calculateMaximumMetricOfRoutingTable() {
    routingTableList->setInUse();

    uint8_t maximumMetricOfRoutingTable = 0;

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.metric > maximumMetricOfRoutingTable)
                maximumMetricOfRoutingTable = node->networkNode.metric;

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();

    return maximumMetricOfRoutingTable + 1;
}


size_t RoutingTableService::oneHopSize() {
    size_t oneHopSize = 0;

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.metric == 1)
                oneHopSize++;

        } while (routingTableList->next());
    }

    return oneHopSize;
}

bool RoutingTableService::getAllHelloPacketsNode(HelloPacketNode** helloPacketNode, size_t* size) {
    routingTableList->setInUse();

    size_t oneHop = oneHopSize();

    *size = 0;

    // If the routing table is empty return nullptr
    if (oneHop == 0) {
        routingTableList->releaseInUse();
        return true;
    }

    *size = oneHop;
    *helloPacketNode = new HelloPacketNode[oneHop];

    if (routingTableList->moveToStart()) {
        size_t position = 0;

        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.metric == 1) {
                (*helloPacketNode)[position] = HelloPacketNode(node->networkNode.address, node->receivedSNR);
                position++;
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();

    return true;
}

LM_LinkedList<RouteNode>* RoutingTableService::routingTableList = new LM_LinkedList<RouteNode>();
uint8_t RoutingTableService::routingTableId = 0;