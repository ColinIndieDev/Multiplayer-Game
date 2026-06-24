#pragma once

#include <cpl/cpl.h>

typedef struct {
    vec2f pos;
    vec2f dir;
    int bounce;
    bool active;
} projectile_t;
#define PROJECTILE_INITIALIZER(pos, dir) ((projectile_t){pos, dir, 0, true})

typedef struct {
    struct {
        vec2f pos;
        vec2f size;
        vec4f color;
    } attribs;

    projectile_t *projectiles;
    unsigned int score;
} player_t;

typedef enum : uint8_t {
    WEAPON_SHOTGUN = 0,
    WEAPON_SHOCKWAVE,
    WEAPON_GUN,
    WEAPON_PISTOL,
    WEAPONS_SIZE
} weapons;

#define EXTERN_GAME_H_VARIABLES                                                \
    extern player_t enemy;                                                     \
    extern bool enemy_exist;                                                   \
    extern float projectile_speed;                                             \
    extern player_t player;                                                    \
    extern int health;                                                         \
    extern vec2f map_size;                                                     \
    extern bool sent_death_msg;                                                \
    extern weapons selected_weapon;                                            \
    extern vec2f                                                               \
        obstacles[(MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE)];      \
    extern int obstacles_size;                                                 \
    extern unsigned int max_bounces;                                           \
    extern game_mode cur_mode;

#define MAX_HEALTH 10

#define MAP_SIZE 1000
#define PATTERN_SIZE 100

void game_init_start_pos();
void game_run();

void play_shoot_sfx();
