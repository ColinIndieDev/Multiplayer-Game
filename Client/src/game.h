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

typedef enum : uint8_t {
    WEAPON_SHOTGUN,
    WEAPON_SHOCKWAVE,
    WEAPON_GUN,
    WEAPON_PISTOL,
    WEAPONS_SIZE
} weapons;

#define EXTERN_GAME_H_VARIABLES                                                \
    extern player_t enemy;                                                     \
    extern bool enemy_exist;                                                   \
    extern float projectile_speed;                                             \
    extern projectile_t *enemy_projectiles;                                    \
    extern player_t player;                                                    \
    extern int health;                                                         \
    extern vec2f map_size;                                                     \
    extern bool sent_death_msg;                                                \
    extern unsigned int score;                                                 \
    extern unsigned int enemy_score;                                           \
    extern weapons selected_weapon;                                            \
    extern vec2f                                                               \
        obstacles[(MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE)];      \
    extern int obstacles_size;                                                 \
    extern pthread_mutex_t enemy_projectiles_mutex;                            \
    extern pthread_mutex_t obstacles_mutex;

#define MAX_HEALTH 10

#define MAP_SIZE 1000
#define PATTERN_SIZE 100

void game_run();
