#pragma once

#include <cpl/cpl.h>

typedef struct {
    vec2f pos;
    vec2f dir;
    bool active;
} projectile_t;

typedef struct {
    vec2f pos;
    vec2f size;
    vec4f color;
} player_t;

#define EXTERN_GAME_H_VARIABLES                                                \
    extern player_t enemy;                                                     \
    extern bool enemy_exist;                                                   \
    extern float projectile_speed;                                             \
    extern projectile_t *enemy_projectiles;                                    \
    extern player_t player;                                                    \
    extern int health;                                                         \
    extern vec2f map_size;                                                     \
    extern bool sent_death_msg;

#define MAX_HEALTH 10

void game_run();
