#include "game.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>

#define CPRNG_IMPL
#include <cpstd/cprng.h>

#include <cpl/cpl.h>
#include <cpstd/cpvec.h>
#include <math.h>

#include "networking.h"

EXTERN_NETWORKING_H_VARIABLES

float weapon_cooldowns[WEAPONS_SIZE] = {
    0.7f, // WEAPON_SHOTGUN
    1.0f, // WEAPON_SHOCKWAVE
    0.5f, // WEAPON_GUN
    0.2f  // WEAPON_PISTOL
};

player_t player = {.pos = VEC2F(0, 0), .size = VEC2F(40, 40), .color = BLUE};
vec2f vel = VEC2F(0, 0);
weapons selected_weapon = WEAPON_SHOTGUN;
float last_shot = 0.0f;
int health = MAX_HEALTH;
unsigned int score = 0;

bool sent_death_msg = false;

player_t enemy = {.pos = VEC2F(0, 0), .size = VEC2F(40, 40), .color = RED};
bool enemy_exist = false;
unsigned int enemy_score = 0;

float projectile_speed = 500.0f;
float projectile_radius = 10.0f;
projectile_t *projectiles = NULL;
projectile_t *enemy_projectiles = NULL;

vec2f map_size = VEC2F(MAP_SIZE, MAP_SIZE);
vec2f pattern_size = VEC2F(PATTERN_SIZE, PATTERN_SIZE);

vec2f obstacles[(MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE)];
int obstacles_size = 0;

font f;

pthread_mutex_t enemy_projectiles_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t obstacles_mutex = PTHREAD_MUTEX_INITIALIZER;

void game_sync_death();
void game_sync_pos();
void game_sync_projectiles(vec2f pos, vec2f *dir, int count);
void game_handle_controls();
void game_init();

void game_run() {
    game_init();

    while (!window_should_close()) {
        update();

        game_handle_controls();

        clear_background(BLACK);

        begin_draw(SHAPE_2D_UNLIT, true);

        draw_rect(VEC2F(0, 0), map_size, GREEN, 0);
        for (int i = 0; i < (int)(map_size.x * map_size.y /
                                  (pattern_size.x * pattern_size.y));
             i++) {
            int col = i % (int)(map_size.x / pattern_size.x);
            int row = i / (int)(map_size.x / pattern_size.x);
            if ((col + row) % 2 == 0) {
                draw_rect(VEC2F(col * pattern_size.x, row * pattern_size.y),
                          pattern_size, LIME_GREEN, 0);
            }
        }

        pthread_mutex_lock(&obstacles_mutex);
        for (int i = 0; i < obstacles_size; i++) {
            draw_rect(obstacles[i], pattern_size, LIGHT_BLUE, 0);
        }
        pthread_mutex_unlock(&obstacles_mutex);

        draw_rect(player.pos, player.size,
                  health > 0 ? player.color
                             : RGBA(player.color.r, player.color.g,
                                    player.color.b, 75),
                  0);

        if (enemy_exist) {
            draw_rect(enemy.pos, enemy.size, enemy.color, 0);
        }

        foreach_vec(p, projectiles) {
            draw_circle(p->pos, projectile_radius, player.color);
        }

        foreach_vec(p, enemy_projectiles) {
            draw_circle(p->pos, projectile_radius, enemy.color);
        }

        begin_draw(SHAPE_2D_UNLIT, false);

        vec2f bar_size = VEC2F(get_screen_width() * 0.33f, 30);
        float offset = 15.0f;
        float border_thickness = 5.0f;
        vec2f border_pos = VEC2F(offset, get_screen_height() - bar_size.y -
                                             (border_thickness * 2) - offset);
        vec2f border_size = VEC2F(bar_size.x + (border_thickness * 2),
                                  bar_size.y + (border_thickness * 2));

        draw_rect(border_pos, border_size, WHITE, 0);

        vec2f bar_pos = VEC2F(border_pos.x + border_thickness,
                              border_pos.y + border_thickness);

        draw_rect(bar_pos, bar_size, BLACK, 0);
        draw_rect(bar_pos,
                  VEC2F(bar_size.x * ((float)health / MAX_HEALTH), bar_size.y),
                  player.color, 0);

        begin_draw(TEXT, false);

        float scale = 1.0f;
        vec2f off = VEC2F(10, 10);

        {
            char txt[100];
            char *weapon;
            switch (selected_weapon) {
            case WEAPON_SHOTGUN:
                weapon = "Shotgun";
                break;
            case WEAPON_SHOCKWAVE:
                weapon = "Shockwave";
                break;
            case WEAPON_GUN:
                weapon = "Gun";
                break;
            case WEAPON_PISTOL:
                weapon = "Pistol";
                break;
            default:
                weapon = "???";
                break;
            }
            snprintf(txt, 100, "Weapon: %s", weapon);

            vec2f txt_size = get_text_size(&f, txt, scale);
            draw_text_shadow(&f, txt,
                             VEC2F(get_screen_width() - txt_size.x - off.x,
                                   get_screen_height() - txt_size.y - off.y),
                             scale, WHITE, VEC2F(3, 3), BLACK);
        }

        if (id == 0) {
            {
                char txt[100];
                snprintf(txt, 100, "You: %d", score);
                draw_text_shadow(&f, txt, off, scale, player.color, VEC2F(3, 3),
                                 LIGHT_BLUE);
            }
            {
                char txt[100];
                snprintf(txt, 100, "Opponent: %d", enemy_score);
                float txt_width = get_text_size(&f, txt, scale).x;
                draw_text_shadow(
                    &f, txt,
                    VEC2F(get_screen_width() - txt_width - off.x, off.y), scale,
                    enemy.color, VEC2F(3, 3), ORANGE);
            }
        } else {
            {
                char txt[100];
                snprintf(txt, 100, "Opponent: %d", enemy_score);
                draw_text_shadow(&f, txt, off, scale, enemy.color, VEC2F(3, 3),
                                 ORANGE);
            }
            {
                char txt[100];
                snprintf(txt, 100, "You: %d", score);
                float txt_width = get_text_size(&f, txt, scale).x;
                draw_text_shadow(
                    &f, txt,
                    VEC2F(get_screen_width() - txt_width - off.x, off.y), scale,
                    player.color, VEC2F(3, 3), LIGHT_BLUE);
            }
        }

        end_frame();
    }
    close_window();
}

void game_init() {
    init_window(800, 800, "Multiplayer Game", OPENGL_VER_3_3);
    cprng_rand_seed();
    create_font(&f, "assets/fonts/default.ttf", "default", FILTER_LINEAR);

    if (id == 0) {
        player.pos = VEC2F(0, (map_size.y * 0.5f) - (player.size.y * 0.5f));
        enemy.pos = VEC2F(map_size.x - enemy.size.x,
                          (map_size.y * 0.5f) - (enemy.size.y * 0.5f));
    } else {
        enemy.pos = VEC2F(0, (map_size.y * 0.5f) - (enemy.size.y * 0.5f));
        player.pos = VEC2F(map_size.x - player.size.x,
                           (map_size.y * 0.5f) - (player.size.y * 0.5f));
    }

    projectiles = vec_init(projectiles, 10);
    enemy_projectiles = vec_init(enemy_projectiles, 10);

    selected_weapon = cprng_rand() % WEAPONS_SIZE;
}

void game_handle_controls() {
    if (health <= 0 && !sent_death_msg) {
        health = 0;
        enemy_score++;
        sent_death_msg = true;
        game_sync_death();
    }

    if (is_key_pressed(KEY_ESCAPE)) {
        destroy_window();
    }

    get_cam_2D()->pos = VEC2F(
        player.pos.x + (player.size.x * 0.5f) - (get_screen_width() * 0.5f),
        player.pos.y + (player.size.y * 0.5f) - (get_screen_height() * 0.5f));

    float speed = 100.0f;
    vel = VEC2F(0, 0);
    if (is_key_down(KEY_W)) {
        vel.y = -1;
    }
    if (is_key_down(KEY_A)) {
        vel.x = -1;
    }
    if (is_key_down(KEY_S)) {
        vel.y = 1;
    }
    if (is_key_down(KEY_D)) {
        vel.x = 1;
    }
    player.pos.x += speed * vel.x * get_dt();
    player.pos.y += speed * vel.y * get_dt();

    if (player.pos.x < 0) {
        player.pos.x = 0;
    } else if (player.pos.x + player.size.x > MAP_SIZE) {
        player.pos.x = MAP_SIZE - player.size.x;
    }
    if (player.pos.y < 0) {
        player.pos.y = 0;
    } else if (player.pos.y + player.size.y > MAP_SIZE) {
        player.pos.y = MAP_SIZE - player.size.y;
    }

    // TODO add collision for obstacles

    game_sync_pos();

    if (selected_weapon == WEAPON_SHOTGUN || selected_weapon == WEAPON_PISTOL ||
        selected_weapon == WEAPON_SHOCKWAVE) {
        if (is_key_pressed(KEY_SPACE) && health > 0 &&
            get_time() >= last_shot + weapon_cooldowns[selected_weapon]) {
            if (selected_weapon == WEAPON_PISTOL) {
                vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
                vec2f pos = VEC2F(player.pos.x + (player.size.x * 0.5f),
                                  player.pos.y + (player.size.y * 0.5f));
                vec2f dir = vec2f_norm(VEC2F(mouse.x - pos.x, mouse.y - pos.y));

                game_sync_projectiles(pos, &dir, 1);

                vec_push(projectiles, ((projectile_t){pos, dir, true}));
            } else if (selected_weapon == WEAPON_SHOCKWAVE) {
                vec2f pos = VEC2F(player.pos.x + (player.size.x * 0.5f),
                                  player.pos.y + (player.size.y * 0.5f));
                vec2f dirs[9] = {VEC2F(0, -1), VEC2F(1, -1),  VEC2F(1, 0),
                                 VEC2F(-1, 1), VEC2F(0, 1),   VEC2F(-1, 1),
                                 VEC2F(-1, 0), VEC2F(-1, -1), VEC2F(1, 1)};
                for (int i = 0; i < 9; i++) {
                    vec2f dir = vec2f_norm(dirs[i]);

                    vec_push(projectiles, ((projectile_t){pos, dir, true}));
                }
                game_sync_projectiles(pos, dirs, 9);
            } else {
                vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
                vec2f pos = VEC2F(player.pos.x + (player.size.x * 0.5f),
                                  player.pos.y + (player.size.y * 0.5f));
                float dx = mouse.x - pos.x;
                float dy = mouse.y - pos.y;

                float rad = atan2f(dy, dx);
                vec2f dirs[3] = {VEC2F(cosf(rad - 0.33f), sinf(rad - 0.33f)),
                                 VEC2F(cosf(rad), sinf(rad)),
                                 VEC2F(cosf(rad + 0.33f), sinf(rad) + 0.33f)};
                for (int i = 0; i < 3; i++) {

                    vec_push(projectiles, ((projectile_t){pos, dirs[i], true}));
                }
                game_sync_projectiles(pos, dirs, 3);
            }
            last_shot = get_time();
        }
    } else {
        if (is_key_down(KEY_SPACE) && health > 0 &&
            get_time() >= last_shot + weapon_cooldowns[selected_weapon]) {

            vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
            vec2f pos = VEC2F(player.pos.x + (player.size.x * 0.5f),
                              player.pos.y + (player.size.y * 0.5f));
            vec2f dir = vec2f_norm(VEC2F(mouse.x - pos.x, mouse.y - pos.y));

            game_sync_projectiles(pos, &dir, 1);

            vec_push(projectiles, ((projectile_t){pos, dir, true}));

            last_shot = get_time();
        }
    }

    foreach_vec(p, projectiles) {
        p->pos.x += p->dir.x * projectile_speed * get_dt();
        p->pos.y += p->dir.y * projectile_speed * get_dt();
        if (p->pos.x < 0 || p->pos.x > map_size.x || p->pos.y < 0 ||
            p->pos.y > map_size.y) {
            p->active = false;
        }
        rect_collider enemy_collider = {.pos = enemy.pos, .size = enemy.size};
        circle_collider projectile_collider = {.pos = p->pos,
                                               .radius = projectile_radius};
        if (check_collision_circle_rect(projectile_collider, enemy_collider) &&
            p->active && enemy_exist) {
            p->active = false;
        }
    }
    vec_erase_if(p, projectiles, !p->active);

    pthread_mutex_lock(&enemy_projectiles_mutex);
    foreach_vec(p, enemy_projectiles) {
        p->pos.x += p->dir.x * projectile_speed * get_dt();
        p->pos.y += p->dir.y * projectile_speed * get_dt();
        if (p->pos.x < 0 || p->pos.x > map_size.x || p->pos.y < 0 ||
            p->pos.y > map_size.y) {
            p->active = false;
        }
        rect_collider player_collider = {.pos = player.pos,
                                         .size = player.size};
        circle_collider projectile_collider = {.pos = p->pos,
                                               .radius = projectile_radius};
        if (check_collision_circle_rect(projectile_collider, player_collider) &&
            p->active && health > 0) {
            p->active = false;
            health--;
        }
    }
    vec_erase_if(p, enemy_projectiles, !p->active);
    pthread_mutex_unlock(&enemy_projectiles_mutex);
}

void game_sync_death() {
    packet_writer writer;
    packet_writer_init(&writer, PACKET_DEAD);
    packet_write_int(&writer, id);
    send_packet_to_server(&client, &writer, NET_PACKET_RELIABLE);
}

void game_sync_pos() {
    packet_writer writer;
    packet_writer_init(&writer, PACKET_POS_SYNC);
    packet_write_int(&writer, id);
    packet_write_float(&writer, player.pos.x);
    packet_write_float(&writer, player.pos.y);
    send_packet_to_server(&client, &writer,
                          ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
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
    send_packet_to_server(&client, &writer, NET_PACKET_RELIABLE);
}
