#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MAX_CLIENTS PLAYER_TYPE_SIZE

#define MAP_SIZE 2000
#define PATTERN_SIZE 100

typedef enum : uint8_t {
    PACKET_INFO = 0,
    PACKET_RECEIVE_ID,
    PACKET_POS_SYNC,
    PACKET_PROJECTILE_SYNC,
    PACKET_LEAVE,
    PACKET_JOIN,
    PACKET_DEAD,
    PACKET_NEW_ROUND,
    PACKET_RECEIVE_OBSTACLES,
    PACKET_RECEIVE_GAME_MODE,
    PACKET_ADD_SCORE
} packet_id;

typedef enum {
    MODE_CLASSIC = 0,   // Normal - 1 time bounce
    MODE_SOLID,         // Normal - no bounce
    MODE_BOUNCY,        // Normal - 3 time bounce
    MODE_CHAOS,         // Normal - (almost) infinite bounce
    MODE_FRIENDLY_FIRE, // After 1 bounce your bullets can hurt you too - 3 time
                        // bounce
    GAME_MODE_SIZE
} game_mode;

typedef enum { 
    PLAYER_RED = 0, 
    PLAYER_BLUE,
    PLAYER_YELLOW,
    PLAYER_PURPLE,
    PLAYER_TYPE_SIZE 
} player_type;

void network_init();
void network_destroy();
void network_run();
