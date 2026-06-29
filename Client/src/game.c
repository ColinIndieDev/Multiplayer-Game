#include "game.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include <cpl/cpl.h>
#include <cpstd/rand.h>
#include <cpstd/vector.h>

#include "networking.h"

// {{{ Global Variables

EXTERN_NETWORKING_H_VARIABLES

float weapon_cooldowns[WEAPONS_SIZE] = {
    0.7f, // WEAPON_SHOTGUN
    1.0f, // WEAPON_SHOCKWAVE
    0.5f, // WEAPON_GUN
    0.2f  // WEAPON_PISTOL
};


vec2f vel = VEC2F(0, 0);
weapons selected_weapon = WEAPON_SHOTGUN;
float last_shot = 0.0f;
int health = MAX_HEALTH;
bool sent_death_msg = false;
float speed = 100.0f;

#define PLAYER_SIZE 40
player_t players[PLAYER_TYPE_SIZE] = {
    {
        .attribs.pos = VEC2F(0, 0),
        .attribs.size = VEC2F(PLAYER_SIZE, PLAYER_SIZE),
        .attribs.color = RED,

        .score = 0,
        .projectiles = NULL
    },
    {
        .attribs.pos = VEC2F(0, 0),
        .attribs.size = VEC2F(PLAYER_SIZE, PLAYER_SIZE),
        .attribs.color = BLUE,

        .score = 0,
        .projectiles = NULL
    },
    {
        .attribs.pos = VEC2F(0, 0),
        .attribs.size = VEC2F(PLAYER_SIZE, PLAYER_SIZE),
        .attribs.color = YELLOW,

        .score = 0,
        .projectiles = NULL
    },
    {
        .attribs.pos = VEC2F(0, 0),
        .attribs.size = VEC2F(PLAYER_SIZE, PLAYER_SIZE),
        .attribs.color = PURPLE,

        .score = 0,
        .projectiles = NULL
    }
};
vec2f player_start_positions[PLAYER_TYPE_SIZE] = {
    {0, (MAP_SIZE * 0.5f) - (PLAYER_SIZE * 0.5f)},                      // RED
    {MAP_SIZE - PLAYER_SIZE, (MAP_SIZE * 0.5f) - (PLAYER_SIZE * 0.5f)}, // BLUE
    {(MAP_SIZE * 0.5f) - (PLAYER_SIZE * 0.5f), 0},                      // YELLOW
    {(MAP_SIZE * 0.5f) - (PLAYER_SIZE * 0.5f), MAP_SIZE - PLAYER_SIZE}  // PURPLE

};
bool enemy_exist[PLAYER_TYPE_SIZE] = {false};

float projectile_speed = 500.0f;
float projectile_radius = 10.0f;
unsigned int max_bounces = 0;

game_mode cur_mode = MODE_CLASSIC;

vec2f obstacles[(MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE)];
int obstacles_size = 0;

font f;

audio shoot_sfx;
audio projectile_collision_sfx;
audio hit_sfx;

// }}}

// {{{ Function Declarations

void game_sync_death();
void game_sync_pos();
void game_sync_projectiles(vec2f pos, vec2f *dir, int count);
void game_handle_player();
void game_init();
void game_draw();

// }}}

void game_run() {
    game_init();

    while (!window_should_close()) {
        if (id == -1) {
            continue;
        }
        update();
        audio_update();

        game_handle_player();

        game_draw();

        end_frame();
    }
    font_delete(&f);
    audio_close();
    window_close();
}

void game_init_start_pos() {
    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        players[i].attribs.pos = player_start_positions[i];
    }
}

void game_init() {
    window_init(800, 800, "Multiplayer Game", OPENGL_VER_3_3);
    audio_init();
    audio_init_voice_chat(&client, &id);
    enable_vsync(false);
    pcg_rand_seed();

    font_load(&f, "assets/fonts/default.ttf", "default", FILTER_LINEAR);

    shoot_sfx = audio_load("assets/sounds/shoot.mp3");
    projectile_collision_sfx = audio_load("assets/sounds/collision.mp3");
    hit_sfx = audio_load("assets/sounds/hit.mp3");

    game_init_start_pos();

    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        players[i].projectiles = vec_init(players[i].projectiles, 10);
    }
    pthread_mutex_unlock(&game_mutex);

    selected_weapon = pcg_rand() % WEAPONS_SIZE;
}

// {{{ game_draw() Helper

void game_draw_map() {
    draw_rect(VEC2F(0, 0), VEC2F(MAP_SIZE, MAP_SIZE), GREEN, 0);
    for (int i = 0; i < (MAP_SIZE * MAP_SIZE / (PATTERN_SIZE * PATTERN_SIZE));
         i++) {
        int col = i % (MAP_SIZE / PATTERN_SIZE);
        int row = i / (MAP_SIZE / PATTERN_SIZE);
        if ((col + row) % 2 == 0) {
            draw_rect(VEC2F(col * PATTERN_SIZE, row * PATTERN_SIZE),
                      VEC2F(PATTERN_SIZE, PATTERN_SIZE), LIME_GREEN, 0);
        }
    }

    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < obstacles_size; i++) {
        draw_rect(obstacles[i], VEC2F(PATTERN_SIZE, PATTERN_SIZE), BLACK, 0);
    }
    pthread_mutex_unlock(&game_mutex);
}

void game_draw_players() {
    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        if (i == id) {
            draw_rect(players[i].attribs.pos, players[i].attribs.size,
              health > 0 ? players[i].attribs.color
                         : RGBA(players[i].attribs.color.r, players[i].attribs.color.g,
                                players[i].attribs.color.b, 75), 0);
        } else if (enemy_exist[i]) {
            draw_rect(players[i].attribs.pos, players[i].attribs.size, players[i].attribs.color, 0);
        }
    }

    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        foreach_vec(p, players[i].projectiles) {
            if (cur_mode == MODE_FRIENDLY_FIRE && p->bounce >= 1) {
                draw_circle(p->pos, projectile_radius, WHITE);
            } else {
                draw_circle(p->pos, projectile_radius, players[i].attribs.color);
            }
        }
    }
    pthread_mutex_unlock(&game_mutex);
}

void game_draw_health_bar() {
    vec2f bar_size = VEC2F(get_screen_width() * 0.33f, 30);
    float offset = 15.0f;
    float border_thickness = 5.0f;
    vec2f border_pos = VEC2F(offset, get_screen_height() - bar_size.y -
                                         (border_thickness * 2) - offset);
    vec2f border_size = VEC2F(bar_size.x + (border_thickness * 2),
                              bar_size.y + (border_thickness * 2));

    draw_rect(border_pos, border_size, WHITE, 0);

    vec2f bar_pos =
        VEC2F(border_pos.x + border_thickness, border_pos.y + border_thickness);

    draw_rect(bar_pos, bar_size, BLACK, 0);
    draw_rect(bar_pos,
              VEC2F(bar_size.x * ((float)health / MAX_HEALTH), bar_size.y),
              players[id].attribs.color, 0);
}

char *weapon_enum_to_str(weapons weapon) {
    switch (weapon) {
    case WEAPON_SHOTGUN:
        return "Shotgun";
    case WEAPON_SHOCKWAVE:
        return "Shockwave";
    case WEAPON_GUN:
        return "Gun";
    case WEAPON_PISTOL:
        return "Pistol";
    default:
        return "???";
    }
}

char *game_mode_enum_to_str(game_mode mode) {
    switch (mode) {
    case MODE_CLASSIC:
        return "Classic";
    case MODE_SOLID:
        return "Solid";
    case MODE_BOUNCY:
        return "Bouncy";
    case MODE_CHAOS:
        return "Chaos";
    case MODE_FRIENDLY_FIRE:
        return "Friendly Fire";
    default:
        return "???";
    }
}

void game_draw_ui() {
    float scale = 1.0f;
    vec2f off = VEC2F(10, 10);

    {
        vec2f txt_size = text_get_size(&f, scale, "Weapon: %s", weapon_enum_to_str(selected_weapon));
        vec2f txt_pos = VEC2F(get_screen_width() - txt_size.x - off.x, get_screen_height() - txt_size.y - off.y); 
        draw_text_shadow(&f, txt_pos, scale, WHITE, VEC2F(3, 3), BLACK, "Weapon: %s", weapon_enum_to_str(selected_weapon));
    }

    draw_text_shadow(&f, VEC2F(off.x, off.y), scale, WHITE, VEC2F(3, 3), BLACK, "Game Mode: %s", game_mode_enum_to_str(cur_mode));

    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        color_t color = players[i].attribs.color;
        draw_text_shadow(&f, VEC2F(off.x, off.y + 75 + (i * 35)), scale * 0.75f, color, VEC2F(3, 3), BLACK, "Score: %d", players[i].score);   
    }
    display_details(&f);
}

// }}}

void game_draw() {
    clear_background(BLACK);

    begin_draw(SHAPE_2D_UNLIT, true);

    game_draw_map();
    game_draw_players();

    begin_draw(SHAPE_2D_UNLIT, false);

    game_draw_health_bar();

    begin_draw(TEXT, false);

    game_draw_ui();
}

// {{{ SFX

void play_shoot_sfx() { 
    audio_play_sound(&shoot_sfx); 
}

// }}}

// {{{ game_handle_player() Helper

void game_move_and_collide() {
    players[id].attribs.pos.x += speed * vel.x * get_dt();
    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < obstacles_size; i++) {
        rect_collider player_collider = {.pos = players[id].attribs.pos,
                                         .size = players[id].attribs.size};
        rect_collider tile_collider = {
            .pos = obstacles[i], .size = VEC2F(PATTERN_SIZE, PATTERN_SIZE)};

        if (players[id].attribs.pos.x + players[id].attribs.size.x <=
                tile_collider.pos.x ||
            players[id].attribs.pos.x >=
                tile_collider.pos.x + tile_collider.size.x) {
            continue;
        }

        if (check_collision_rects(player_collider, tile_collider)) {
            if (vel.x < 0) {
                players[id].attribs.pos.x =
                    tile_collider.pos.x + tile_collider.size.x + 0.01f;
            } else if (vel.x > 0) {
                players[id].attribs.pos.x =
                    tile_collider.pos.x - players[id].attribs.size.x - 0.01f;
            }
            vel.x = 0;
        }
    }
    pthread_mutex_unlock(&game_mutex);

    players[id].attribs.pos.y += speed * vel.y * get_dt();
    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < obstacles_size; i++) {
        rect_collider player_collider = {.pos = players[id].attribs.pos,
                                         .size = players[id].attribs.size};
        rect_collider tile_collider = {
            .pos = obstacles[i], .size = VEC2F(PATTERN_SIZE, PATTERN_SIZE)};

        if (players[id].attribs.pos.y + players[id].attribs.size.y <=
                tile_collider.pos.y ||
            players[id].attribs.pos.y >=
                tile_collider.pos.y + tile_collider.size.y) {
            continue;
        }

        if (check_collision_rects(player_collider, tile_collider)) {
            if (vel.y < 0) {
                players[id].attribs.pos.y =
                    tile_collider.pos.y + tile_collider.size.y + 0.01f;
            } else if (vel.y > 0) {
                players[id].attribs.pos.y =
                    tile_collider.pos.y - players[id].attribs.size.y - 0.01f;
            }
            vel.y = 0;
        }
    }
    pthread_mutex_unlock(&game_mutex);

    if (players[id].attribs.pos.x < 0) {
        players[id].attribs.pos.x = 0;
    } else if (players[id].attribs.pos.x + players[id].attribs.size.x > MAP_SIZE) {
        players[id].attribs.pos.x = MAP_SIZE - players[id].attribs.size.x;
    }
    if (players[id].attribs.pos.y < 0) {
        players[id].attribs.pos.y = 0;
    } else if (players[id].attribs.pos.y + players[id].attribs.size.y > MAP_SIZE) {
        players[id].attribs.pos.y = MAP_SIZE - players[id].attribs.size.y;
    }
}

void game_handle_weapon() {
    if (selected_weapon == WEAPON_SHOTGUN || selected_weapon == WEAPON_PISTOL ||
        selected_weapon == WEAPON_SHOCKWAVE) {
        if (is_key_pressed(KEY_SPACE) && health > 0 &&
            get_time() >= last_shot + weapon_cooldowns[selected_weapon]) {
            if (selected_weapon == WEAPON_PISTOL) {
                vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
                vec2f pos = VEC2F(
                    players[id].attribs.pos.x + (players[id].attribs.size.x * 0.5f),
                    players[id].attribs.pos.y + (players[id].attribs.size.y * 0.5f));
                vec2f dir = vec2f_norm(VEC2F(mouse.x - pos.x, mouse.y - pos.y));

                play_shoot_sfx();

                pthread_mutex_lock(&game_mutex);
                vec_push(players[id].projectiles, PROJECTILE_INITIALIZER(pos, dir));
                pthread_mutex_unlock(&game_mutex);
                game_sync_projectiles(pos, &dir, 1);
            } else if (selected_weapon == WEAPON_SHOCKWAVE) {
                vec2f pos = VEC2F(
                    players[id].attribs.pos.x + (players[id].attribs.size.x * 0.5f),
                    players[id].attribs.pos.y + (players[id].attribs.size.y * 0.5f));
                vec2f dirs[9] = {
                    vec2f_norm(VEC2F(0, -1)), vec2f_norm(VEC2F(1, -1)),
                    vec2f_norm(VEC2F(1, 0)),  vec2f_norm(VEC2F(-1, 1)),
                    vec2f_norm(VEC2F(0, 1)),  vec2f_norm(VEC2F(-1, 1)),
                    vec2f_norm(VEC2F(-1, 0)), vec2f_norm(VEC2F(-1, -1)),
                    vec2f_norm(VEC2F(1, 1))};

                play_shoot_sfx();

                pthread_mutex_lock(&game_mutex);
                for (int i = 0; i < 9; i++) {
                    vec2f dir = dirs[i];

                    vec_push(players[id].projectiles,
                             PROJECTILE_INITIALIZER(pos, dir));
                }
                pthread_mutex_unlock(&game_mutex);
                game_sync_projectiles(pos, dirs, 9);
            } else {
                vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
                vec2f pos = VEC2F(
                    players[id].attribs.pos.x + (players[id].attribs.size.x * 0.5f),
                    players[id].attribs.pos.y + (players[id].attribs.size.y * 0.5f));
                float dx = mouse.x - pos.x;
                float dy = mouse.y - pos.y;

                float rad = atan2f(dy, dx);
                vec2f dirs[3] = {VEC2F(cosf(rad - 0.33f), sinf(rad - 0.33f)),
                                 VEC2F(cosf(rad), sinf(rad)),
                                 VEC2F(cosf(rad + 0.33f), sinf(rad) + 0.33f)};

                play_shoot_sfx();

                pthread_mutex_lock(&game_mutex);

                for (int i = 0; i < 3; i++) {

                    vec_push(players[id].projectiles,
                             PROJECTILE_INITIALIZER(pos, dirs[i]));
                }
                pthread_mutex_unlock(&game_mutex);
                game_sync_projectiles(pos, dirs, 3);
            }
            last_shot = get_time();
        }
    } else {
        if (is_key_down(KEY_SPACE) && health > 0 &&
            get_time() >= last_shot + weapon_cooldowns[selected_weapon]) {

            vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
            vec2f pos =
                VEC2F(players[id].attribs.pos.x + (players[id].attribs.size.x * 0.5f),
                      players[id].attribs.pos.y + (players[id].attribs.size.y * 0.5f));
            vec2f dir = vec2f_norm(VEC2F(mouse.x - pos.x, mouse.y - pos.y));

            play_shoot_sfx();

            pthread_mutex_lock(&game_mutex);
            vec_push(players[id].projectiles, PROJECTILE_INITIALIZER(pos, dir));
            pthread_mutex_unlock(&game_mutex);
            game_sync_projectiles(pos, &dir, 1);

            last_shot = get_time();
        }
    }
}

void game_handle_projectile(projectile_t *p) {
    p->pos.x += p->dir.x * projectile_speed * get_dt();
    if (p->pos.x - projectile_radius < 0) {
        p->pos.x = projectile_radius + 0.01f;
        p->dir.x *= -1;
        p->bounce++;

        if (cur_mode != MODE_CHAOS) {
            audio_play_sound(&projectile_collision_sfx);
        }

        if (p->bounce > max_bounces) {
            p->active = false;
        }
    } else if (p->pos.x + projectile_radius > MAP_SIZE) {
        p->pos.x = MAP_SIZE - projectile_radius - 0.01f;
        p->dir.x *= -1;
        p->bounce++;

        if (cur_mode != MODE_CHAOS) {
            audio_play_sound(&projectile_collision_sfx);
        }

        if (p->bounce > max_bounces) {
            p->active = false;
        }
    }
    for (int i = 0; i < obstacles_size; i++) {
        circle_collider projectile_collider = {.pos = p->pos,
                                               .radius = projectile_radius};
        rect_collider tile_collider = {
            .pos = obstacles[i], .size = VEC2F(PATTERN_SIZE, PATTERN_SIZE)};

        if (check_collision_circle_rect(projectile_collider, tile_collider) &&
            p->active) {
            if (p->dir.x > 0) {
                p->pos.x = tile_collider.pos.x - projectile_radius - 0.01f;
            } else if (p->dir.x < 0) {
                p->pos.x = tile_collider.pos.x + tile_collider.size.x +
                           projectile_radius + 0.01f;
            }
            p->dir.x *= -1;
            p->bounce++;

            if (cur_mode != MODE_CHAOS) {
                audio_play_sound(&projectile_collision_sfx);
            }

            if (p->bounce > max_bounces) {
                p->active = false;
            }
        }
    }

    p->pos.y += p->dir.y * projectile_speed * get_dt();
    if (p->pos.y - projectile_radius < 0) {
        p->pos.y = p->pos.y + projectile_radius + 0.01f;
        p->dir.y *= -1;
        p->bounce++;

        if (cur_mode != MODE_CHAOS) {
            audio_play_sound(&projectile_collision_sfx);
        }

        if (p->bounce > max_bounces) {
            p->active = false;
        }
    } else if (p->pos.y + projectile_radius > MAP_SIZE) {
        p->pos.y = p->pos.y - projectile_radius - 0.01f;
        p->dir.y *= -1;
        p->bounce++;

        if (cur_mode != MODE_CHAOS) {
            audio_play_sound(&projectile_collision_sfx);
        }

        if (p->bounce > max_bounces) {
            p->active = false;
        }
    }
    for (int i = 0; i < obstacles_size; i++) {
        circle_collider projectile_collider = {.pos = p->pos,
                                               .radius = projectile_radius};
        rect_collider tile_collider = {
            .pos = obstacles[i], .size = VEC2F(PATTERN_SIZE, PATTERN_SIZE)};

        if (check_collision_circle_rect(projectile_collider, tile_collider) &&
            p->active) {
            if (p->dir.y > 0) {
                p->pos.y = tile_collider.pos.y - projectile_radius - 0.01f;
            } else if (p->dir.y < 0) {
                p->pos.y = tile_collider.pos.y + tile_collider.size.y +
                           projectile_radius + 0.01f;
            }
            p->dir.y *= -1;
            p->bounce++;

            if (cur_mode != MODE_CHAOS) {
                audio_play_sound(&projectile_collision_sfx);
            }

            if (p->bounce > max_bounces) {
                p->active = false;
            }
        }
    }
}

void game_handle_projectiles() {
    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        foreach_vec(p, players[i].projectiles) {
            game_handle_projectile(p);
        }
    }
    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
        for (int j = 0; j < PLAYER_TYPE_SIZE; j++) {
            foreach_vec(p, players[j].projectiles) {
                rect_collider player_collider = {
                    .pos = players[i].attribs.pos,
                    .size = players[i].attribs.size
                };
                circle_collider projectile_collider = {
                    .pos = p->pos,
                    .radius = projectile_radius
                };
                if (check_collision_circle_rect(projectile_collider, player_collider) &&
                    p->active) {
                    if (cur_mode == MODE_FRIENDLY_FIRE && p->bounce >= 1) {
                        if (i == id) {
                            if (health > 0) {
                                audio_play_sound(&hit_sfx);
                                p->active = false;
                                health--;
                            }
                        } else if (enemy_exist[i]){
                            audio_play_sound(&hit_sfx);
                            p->active = false;
                        }
                    } else {
                        if (i == id) {
                            if (i != j && health > 0) {
                                audio_play_sound(&hit_sfx);
                                p->active = false;
                                health--;
                            }
                        } else if (i != j && enemy_exist[i]) {
                            audio_play_sound(&hit_sfx);
                            p->active = false;
                        }
                    }
                }
            }
        }
        vec_erase_if(p, players[i].projectiles, !p->active);
    }
    pthread_mutex_unlock(&game_mutex);
}

void game_handle_controls() {
    if (is_key_pressed(KEY_ESCAPE)) {
        window_destroy();
    }

    get_cam_2D()->pos = VEC2F(players[id].attribs.pos.x + (players[id].attribs.size.x * 0.5f) - (get_screen_width() * 0.5f),
                              players[id].attribs.pos.y + (players[id].attribs.size.y * 0.5f) - (get_screen_height() * 0.5f));

    vel = VEC2F(0, 0);
    if (is_key_down(KEY_LETTER_W)) {
        vel.y = -1;
    }
    if (is_key_down(KEY_LETTER_A)) {
        vel.x = -1;
    }
    if (is_key_down(KEY_LETTER_S)) {
        vel.y = 1;
    }
    if (is_key_down(KEY_LETTER_D)) {
        vel.x = 1;
    }
}

void game_handle_game_over() {
    if (health <= 0 && !sent_death_msg) {
        health = 0;
        sent_death_msg = true;
        game_sync_death();
    }
}

// }}}

void game_handle_player() {
    game_handle_game_over();
    game_handle_controls();
    game_move_and_collide();

    game_sync_pos();

    game_handle_weapon();
    game_handle_projectiles();
}

// {{{ Synchronization (with Server)

void game_sync_death() {
    packet_writer writer;
    packet_writer_init(&writer, PACKET_DEAD);
    packet_write_int(&writer, id);
    packet_send_to_server(&client, &writer, NET_PACKET_RELIABLE, NET_CHANNEL_RELIABLE);
}

void game_sync_pos() {
    packet_writer writer;
    packet_writer_init(&writer, PACKET_POS_SYNC);
    packet_write_int(&writer, id);
    packet_write_float(&writer, players[id].attribs.pos.x);
    packet_write_float(&writer, players[id].attribs.pos.y);
    packet_send_to_server(&client, &writer, NET_PACKET_UNRELIABLE, NET_CHANNEL_UNRELIABLE);
}

void game_sync_projectiles(vec2f pos, vec2f *dir, int count) {
    packet_writer writer;
    packet_writer_init(&writer, PACKET_PROJECTILE_SYNC);
    packet_write_int(&writer, id);
    packet_write_float(&writer, pos.x);
    packet_write_float(&writer, pos.y);
    packet_write_int(&writer, count);
    for (int i = 0; i < count; i++) {
        packet_write_float(&writer, dir[i].x);
        packet_write_float(&writer, dir[i].y);
    }
    packet_send_to_server(&client, &writer, NET_PACKET_RELIABLE, NET_CHANNEL_RELIABLE);
}

// }}}
