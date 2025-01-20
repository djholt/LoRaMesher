#include <Arduino.h>
#include "LoraMesher.h"
#include "display.h"

// Heltec V3
#define BOARD_LED   35
#define LED_ON      HIGH
#define LED_OFF     LOW

LoraMesher& radio = LoraMesher::getInstance();

#define PAYLOAD_CHARS 32

uint32_t dataCounter = 0;
struct dataPacket {
    int32_t counter = -1;
    char message[PAYLOAD_CHARS];
};

dataPacket* helloPacket = new dataPacket;
dataPacket* userPacket = new dataPacket;

/**
 * @brief Flash the lead
 *
 * @param flashes number of flashes
 * @param delaymS delay between is on and off of the LED
 */
void led_Flash(uint16_t flashes, uint16_t delaymS) {
    uint16_t index;
    for (index = 1; index <= flashes; index++) {
        digitalWrite(BOARD_LED, LED_ON);
        vTaskDelay(delaymS / portTICK_PERIOD_MS);
        digitalWrite(BOARD_LED, LED_OFF);
        vTaskDelay(delaymS / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Print the counter of the packet
 *
 * @param data
 */
void printPacketToScreen(dataPacket* data, uint16_t sourceAddress) {
    char text[32];
    snprintf(text, 32, ("%X-> %d\n"), sourceAddress, data->counter);
    Screen.changeLineThree(String(text));
}

/**
 * @brief Iterate through the payload of the packet and print the counter of the packet
 *
 * @param packet
 */
void printDataPacket(AppPacket<dataPacket>* packet) {
    //Get the payload to iterate through it
    dataPacket* dPacket = packet->payload;
    size_t payloadLength = packet->getPayloadLength();

    Serial.printf("Packet arrived from %X with length %d and size %d bytes\n", packet->src, payloadLength, packet->payloadSize);
    printPacketToScreen(&dPacket[0], packet->src);

    Serial.println("---- Payload ----");
    for (size_t i = 0; i < payloadLength; i++) {
        dataPacket data = dPacket[i];
        Serial.printf("Packet part %d: %d => %s\n", i, data.counter, data.message);
    }
    Serial.println("---- Payload Done ----");
}

/**
 * @brief Function that process the received packets
 *
 */
void processReceivedPackets(void*) {
    for (;;) {
        /* Wait for the notification of processReceivedPackets and enter blocking */
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        led_Flash(1, 100); //one quick LED flashes to indicate a packet has arrived

        //Iterate through all the packets inside the Received User Packets Queue
        while (radio.getReceivedQueueSize() > 0) {
            Serial.println("ReceivedUserData_TaskHandle notify received");
            Serial.printf("Queue receiveUserData size: %d\n", radio.getReceivedQueueSize());

            //Get the first element inside the Received User Packets Queue
            AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();

            //Print the data packet
            printDataPacket(packet);

            //Delete the packet when used. It is very important to call this function to release the memory of the packet.
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

/**
 * @brief Create a Receive Messages Task and add it to the LoRaMesher
 *
 */
void createReceiveMessages() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    if (res != pdPASS) {
        Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
    }
}

/**
 * @brief Initialize LoRaMesher
 *
 */
void setupLoraMesher() {
    //Get the configuration of the LoRaMesher
    LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();

    // Heltec V3
    config.loraCs  = 8;
    config.loraRst = 12;
    config.loraIrq = 14;
    config.loraIo1 = 13;

    config.module = LoraMesher::LoraModules::SX1262_MOD;

    //Init the loramesher with a configuration
    radio.begin(config);

    //Create the receive task and add it to the LoRaMesher
    createReceiveMessages();

    //Set the task handle to the LoRaMesher
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);

    //Start LoRaMesher
    radio.start();

    Serial.println("Lora initialized");
}

/**
 * @brief Displays the address in the first line
 *
 */
void printAddressDisplay() {
    char addrStr[15];
    snprintf(addrStr, 15, "Id: %X\r\n", radio.getLocalAddress());

    Screen.changeLineOne(String(addrStr));
}

/**
 * @brief Print the routing table into the display
 *
 */
void printRoutingTableToDisplay() {

    //Set the routing table list that is being used and cannot be accessed (Remember to release use after usage)
    LM_LinkedList<RouteNode>* routingTableList = radio.routingTableListCopy();

    routingTableList->setInUse();

    Screen.changeSizeRouting(radio.routingTableSize());

    char text[15];
    for (int i = 0; i < radio.routingTableSize(); i++) {
        RouteNode* rNode = (*routingTableList)[i];
        NetworkNode node = rNode->networkNode;
        snprintf(text, 15, ("|%X(%d)->%X"), node.address, node.metric, rNode->via);
        Screen.changeRoutingText(text, i);
    }

    //Release routing table list usage.
    routingTableList->releaseInUse();

    // Delete routing table list
    delete routingTableList;

    Screen.changeLineFour();
}

/**
 * @brief Every 300 seconds it will send a counter to a position of the dataTable
 *
 */
void sendLoRaMessage(void*) {
    int dataTablePosition = 0;

    for (;;) {
        if (radio.routingTableSize() == 0) {
            vTaskDelay(300000 / portTICK_PERIOD_MS);
            continue;
        }

        if (radio.routingTableSize() <= dataTablePosition)
            dataTablePosition = 0;

        LM_LinkedList<RouteNode>* routingTableList = radio.routingTableListCopy();

        uint16_t addr = (*routingTableList)[dataTablePosition]->networkNode.address;

        Serial.printf("Send data packet nº %d to %X (%d)\n", dataCounter, addr, dataTablePosition);

        dataTablePosition++;

        helloPacket->counter = dataCounter;
        strncpy(helloPacket->message, "<HELLO>", sizeof(helloPacket->message)-1);
        helloPacket->message[sizeof(helloPacket->message)-1] = '\0';

        //Create packet and send it.
        radio.createPacketAndSend(addr, helloPacket, 1);

        //Print second line in the screen
        Screen.changeLineTwo("Send " + String(dataCounter));

        //Increment data counter
        ++dataCounter;

        //Print routing Table to Display
        printRoutingTableToDisplay();

        //Release routing table list usage.
        delete routingTableList;

        //Wait 300 seconds to send the next packet
        vTaskDelay(300000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Setup the Task to create and send periodical messages
 *
 */
void createSendMessages() {
    TaskHandle_t sendLoRaMessage_Handle = NULL;
    BaseType_t res = xTaskCreate(
        sendLoRaMessage,
        "Send LoRa Message routine",
        4098,
        (void*) 1,
        1,
        &sendLoRaMessage_Handle);
    if (res != pdPASS) {
        Serial.printf("Error: Send LoRa Message task creation gave error: %d\n", res);
        vTaskDelete(sendLoRaMessage_Handle);
    }
}

void sendUserPacket(uint32_t recipientAddr, char *recipientPayload) {
    LM_LinkedList<RouteNode> *routingTableList = radio.routingTableListCopy();
    routingTableList->setInUse();

    for (int i = 0; i < radio.routingTableSize(); i++) {
        RouteNode *rNode = (*routingTableList)[i];
        NetworkNode node = rNode->networkNode;
        if (node.address == recipientAddr) {
            Serial.printf("Sending packet to %X via %X with payload: %s\n", node.address, rNode->via, recipientPayload);
            strncpy(userPacket->message, recipientPayload, sizeof(userPacket->message)-1);
            userPacket->message[sizeof(userPacket->message)-1] = '\0';
            radio.createPacketAndSend(node.address, userPacket, 1);
        }
    }

    routingTableList->releaseInUse();
    delete routingTableList;
}

const byte serialRxBufferSize = 255;
char serialRxBuffer[serialRxBufferSize];
boolean serialRxDataReceived = false;

void receiveSerialData() {
    static byte i = 0;
    char recv;

    while (Serial.available() > 0 && serialRxDataReceived == false) {
        recv = Serial.read();

        if (recv == '\n') {
            serialRxBuffer[i] = '\0';
            i = 0;
            serialRxDataReceived = true;
        } else {
            serialRxBuffer[i] = recv;
            i++;
            if (i >= serialRxBufferSize) {
                i = serialRxBufferSize - 1;
            }
        }
    }
}

void processSerialInput() {
    if (serialRxDataReceived) {
        Serial.print("Received: ");
        Serial.println(serialRxBuffer);
        serialRxDataReceived = false;

        char *separator = strchr(serialRxBuffer, ':');
        if (separator != 0) {
            *separator = '\0';
            uint32_t recipientAddr = strtoul(serialRxBuffer, NULL, 16);
            char *recipientPayload = ++separator;
            sendUserPacket(recipientAddr, recipientPayload);
        } else if (strcmp(serialRxBuffer, "routes") == 0) {
            RoutingTableService::printRoutingTable();
        }
    }
}

void setup() {
    // Heltec V3
    Wire.begin(17, 18);

    Serial.begin(115200);
    pinMode(BOARD_LED, OUTPUT); //setup pin as output for indicator LED

    Screen.initDisplay();
    Serial.println("Board Init");


    led_Flash(2, 125);          //two quick LED flashes to indicate program start
    setupLoraMesher();
    printAddressDisplay();
    //createSendMessages();
}

void loop() {
    receiveSerialData();
    processSerialInput();
    Screen.drawDisplay();
}
