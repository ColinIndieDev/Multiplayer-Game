#pragma once

#include <stdint.h>

typedef enum : uint8_t {
    PACKET_INFO,
    PACKET_RECEIVE_ID,
    PACKET_POS_SYNC,
    PACKET_PROJECTILE_SYNC,
    PACKET_LEAVE,
    PACKET_JOIN,
    PACKET_DEAD,
    PACKET_NEW_ROUND
} packet_id;

#define EXTERN_NETWORKING_H_VARIABLES                                          \
    extern client_t client;                                                    \
    extern int id;

void networking_init();
void networking_close();
