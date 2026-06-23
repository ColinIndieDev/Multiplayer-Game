#include "networking.h"

#include <cpl/cpl.h>
#include <cpstd/rand.h>

#include "game.h"

EXTERN_GAME_H_VARIABLES

client_t client;
int id = -1;
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;

void parse_data(char *packet_data, size_t packet_data_len, void *arg) {
    (void)arg;

    packet_reader reader;
    packet_id flag =
        packet_reader_init(&reader, (uint8_t *)packet_data, packet_data_len);

    switch (flag) {
    case PACKET_RECEIVE_ID:
        id = packet_read_int(&reader);
        break;
    case PACKET_POS_SYNC: {
        if (id == packet_read_int(&reader)) {
            break;
        }
        enemy.attribs.pos.x = packet_read_float(&reader);
        enemy.attribs.pos.y = packet_read_float(&reader);
        break;
    }
    case PACKET_PROJECTILE_SYNC: {
        if (id == packet_read_int(&reader)) {
            break;
        }
        vec2f epos =
            VEC2F(packet_read_float(&reader), packet_read_float(&reader));
        int count = packet_read_int(&reader);
        for (int i = 0; i < count; i++) {
            vec2f dir =
                VEC2F(packet_read_float(&reader), packet_read_float(&reader));

            unsigned int delay_ms = client.peer->roundTripTime;
            float elapsed = (float)delay_ms * 0.001f;

            vec2f pos = VEC2F(epos.x + (dir.x * projectile_speed * elapsed),
                              epos.y + (dir.y * projectile_speed * elapsed));
            
            pthread_mutex_lock(&game_mutex);
            vec_push(enemy.projectiles, ((projectile_t){epos, dir, true}));
            pthread_mutex_unlock(&game_mutex);
        }
        break;
    }
    case PACKET_JOIN:
        if (packet_read_int(&reader) != id) {
            enemy_exist = true;
        }
        break;
    case PACKET_LEAVE:
        if (packet_read_int(&reader) != id) {
            enemy_exist = false;
        }
        break;
    case PACKET_DEAD:
        if (packet_read_int(&reader) != id) {
            enemy_exist = false;
            player.score++;
        }
        break;
    case PACKET_NEW_ROUND:
        game_init_start_pos();
        sent_death_msg = false;
        enemy_exist = true;
        selected_weapon = pcg_rand() % WEAPONS_SIZE;
        health = MAX_HEALTH;
        pthread_mutex_lock(&game_mutex);
        vec_clear(enemy.projectiles);
        vec_clear(player.projectiles);
        pthread_mutex_unlock(&game_mutex);
        break;
    case PACKET_RECEIVE_OBSTACLES:
        pthread_mutex_lock(&game_mutex);
        obstacles_size = packet_read_int(&reader);
        for (int i = 0; i < obstacles_size; i++) {
            obstacles[i].x = packet_read_float(&reader);
            obstacles[i].y = packet_read_float(&reader);
        }
        pthread_mutex_unlock(&game_mutex);
        break;
    default:
        break;
    }
}

void networking_init() {
    if (client_create(&client, "127.0.0.1", 7777, NET_SEC(5))) {
        printf("Connection successful\n");
    } else {
        fprintf(stderr, "Connection failed\n");
        exit(-1);
    }

    client_create_worker_loop(&client, parse_data, NULL);
}

void networking_close() {
    client_destroy_worker_loop();

    client_destroy(&client, NET_SEC(3));
    printf("Disconnection successful\n");
}
