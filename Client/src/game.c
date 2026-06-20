#include "game.h"

#include <cpl/cpl.h>
#include <cpstd/cpvec.h>

#include "networking.h"

EXTERN_NETWORKING_H_VARIABLES

player_t player = {.pos = VEC2F(0, 0), .size = VEC2F(40, 40), .color = BLUE};
int health = MAX_HEALTH;

bool sent_death_msg = false;

player_t enemy = {.pos = VEC2F(0, 0), .size = VEC2F(40, 40), .color = RED};
bool enemy_exist = false;

float projectile_speed = 500.0f;
float projectile_radius = 10.0f;
projectile_t *projectiles = NULL;
projectile_t *enemy_projectiles = NULL;

vec2f map_size = VEC2F(1000, 1000);
vec2f pattern_size = VEC2F(100, 100);

font f;

void game_sync_death();
void game_sync_pos();
void game_sync_projectiles(vec2f pos, vec2f dir);
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
        for (int i = 0; i < (int)(map_size.x * map_size.y / (pattern_size.x * pattern_size.y));
             i++) {
            int col = i % (int)(map_size.x / pattern_size.x);
            int row = i / (int)(map_size.x / pattern_size.x);
            if ((col + row) % 2 == 0) {
                draw_rect(VEC2F(col * pattern_size.x, row * pattern_size.y),
                          pattern_size, LIME_GREEN, 0);
            }
        }

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
        vec2f border_pos = VEC2F(offset, get_screen_height() - bar_size.y - (border_thickness * 2) - offset);
        vec2f border_size = VEC2F(bar_size.x + (border_thickness * 2), bar_size.y + (border_thickness * 2));

        draw_rect(border_pos, border_size, WHITE, 0);

        vec2f bar_pos = VEC2F(border_pos.x + border_thickness, border_pos.y + border_thickness);

        draw_rect(bar_pos, bar_size, BLACK, 0);
        draw_rect(bar_pos, VEC2F(bar_size.x * ((float)health / MAX_HEALTH), bar_size.y), player.color, 0);

        begin_draw(TEXT, false);

        char id_txt[100];
        snprintf(id_txt, 100, "ID: %d", id);
        draw_text(&f, id_txt, VEC2F(10, 10), 0.6f, WHITE);

        end_frame();
    }
    close_window();
}

void game_init() {
    init_window(800, 800, "Multiplayer Game", OPENGL_VER_3_3);

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
}

void game_handle_controls() {
    if (health <= 0 && !sent_death_msg) {
        health = 0;
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
    if (is_key_down(KEY_W)) {
        player.pos.y -= speed * get_dt();
    }
    if (is_key_down(KEY_A)) {
        player.pos.x -= speed * get_dt();
    }
    if (is_key_down(KEY_S)) {
        player.pos.y += speed * get_dt();
    }
    if (is_key_down(KEY_D)) {
        player.pos.x += speed * get_dt();
    }

    if (is_key_pressed(KEY_SPACE) && health > 0) {
        vec2f mouse = get_screen_to_world_2D(get_mouse_pos());
        vec2f pos = VEC2F(player.pos.x + (player.size.x * 0.5f),
                          player.pos.y + (player.size.y * 0.5f));
        vec2f dir = vec2f_norm(VEC2F(mouse.x - pos.x, mouse.y - pos.y));

        game_sync_projectiles(pos, dir);

        vec_push(projectiles, ((projectile_t){pos, dir, true}));
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
            p->active) {
            p->active = false;
            health--;
        }
    }
    vec_erase_if(p, enemy_projectiles, !p->active);

    game_sync_pos();
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

void game_sync_projectiles(vec2f pos, vec2f dir) {
    packet_writer writer;
    packet_writer_init(&writer, PACKET_PROJECTILE_SYNC);
    packet_write_int(&writer, id);
    packet_write_float(&writer, pos.x);
    packet_write_float(&writer, pos.y);
    packet_write_float(&writer, dir.x);
    packet_write_float(&writer, dir.y);
    send_packet_to_server(&client, &writer, NET_PACKET_RELIABLE);
}
