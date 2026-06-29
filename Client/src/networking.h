#pragma once

#include <stdint.h>

#define MAX_CLIENTS 4

typedef enum { 
    PLAYER_RED = 0, 
    PLAYER_BLUE,
    PLAYER_YELLOW,
    PLAYER_PURPLE,
    PLAYER_TYPE_SIZE 
} player_type;

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
    MODE_CLASSIC = 0,  // Normal - 1 time bounce
    MODE_SOLID,        // Normal - no bounce
    MODE_BOUNCY,       // Normal - 3 time bounce
    MODE_CHAOS,        // Normal - (almost) infinite bounce
    MODE_FRIENDLY_FIRE // After 1 bounce your bullets can hurt you too - 3 time
                       // bounce
} game_mode;

#define EXTERN_NETWORKING_H_VARIABLES                                          \
    extern client_t client;                                                    \
    extern int id;                                                             \
    extern pthread_mutex_t game_mutex;

void networking_init();
void networking_close();
