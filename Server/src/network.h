#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MAX_CLIENTS 2

#define MAP_SIZE 1000
#define PATTERN_SIZE 100

typedef enum : uint8_t {
    PACKET_INFO,
    PACKET_RECEIVE_ID,
    PACKET_POS_SYNC,
    PACKET_PROJECTILE_SYNC,
    PACKET_LEAVE,
    PACKET_JOIN,
    PACKET_DEAD,
    PACKET_NEW_ROUND,
    PACKET_RECEIVE_OBSTACLES
} packet_id;

typedef enum { 
    PLAYER_RED = 0, 
    PLAYER_BLUE, 
    PLAYER_TYPE_SIZE 
} player_type;

void network_init();
void network_destroy();
void network_run();
