#define ENET_IMPLEMENTATION
#include <enet.h>

#include <stdio.h>

#include "server.h"

#define MAX_CLIENTS 10

bool running = true;

typedef enum : uint8_t {
    PACKET_INFO
} packet_id;

int main(void) {
    server_t server;
    server_init(&server, 7777, ENET_HOST_ANY, MAX_CLIENTS);

    while (running) {
        ENetEvent event;
        while (enet_host_service(server.server, &event, SEC_TO_MS(1)) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                char ip[46];
                enet_address_get_host_ip(&event.peer->address, ip, sizeof(ip));
                char *display_ip = ip;
                if (strncmp(ip, "::ffff:", 7) == 0) {
                    display_ip = ip + 7;
                }
                printf("A new client connected from %s:%u!\n", display_ip,
                       event.peer->address.port);
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                printf("A packet of length %u was received on channel %u!\n",
                       (unsigned int)event.packet->dataLength, event.channelID);

                packet_reader reader;
                packet_id flag = packet_reader_init(&reader, event.packet->data, event.packet->dataLength);

                switch (flag) {
                    case PACKET_INFO:
                        printf("Read: %f\n", packet_read_float(&reader));
                        break;
                }

                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                event.peer->data = NULL;
                break;
            }
            default:
                break;
            }
        }
    }

    server_destroy(&server);
}
