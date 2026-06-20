#include "networking.h"

#include <cpl/cpl.h>

#include "game.h"

EXTERN_GAME_H_VARIABLES

client_t client;
int id = -1;

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
        enemy.pos.x = packet_read_float(&reader);
        enemy.pos.y = packet_read_float(&reader);
        break;
    }
    case PACKET_PROJECTILE_SYNC: {
        if (id == packet_read_int(&reader)) {
            break;
        }
        vec2f epos =
            VEC2F(packet_read_float(&reader), packet_read_float(&reader));
        vec2f dir =
            VEC2F(packet_read_float(&reader), packet_read_float(&reader));

        unsigned int delay_ms = client.peer->roundTripTime;
        float elapsed = (float)delay_ms * 0.001f;

        vec2f pos = VEC2F(epos.x + (dir.x * projectile_speed * elapsed),
                          epos.y + (dir.x * projectile_speed * elapsed));

        vec_push(enemy_projectiles, ((projectile_t){epos, dir, true}));
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
        }
        break;
    case PACKET_NEW_ROUND:
        if (id == 0) {
            player.pos = VEC2F(0, (map_size.y * 0.5f) - (player.size.y * 0.5f));
            enemy.pos = VEC2F(map_size.x - enemy.size.x,
                              (map_size.y * 0.5f) - (enemy.size.y * 0.5f));
        } else {
            enemy.pos = VEC2F(0, (map_size.y * 0.5f) - (enemy.size.y * 0.5f));
            player.pos = VEC2F(map_size.x - player.size.x,
                               (map_size.y * 0.5f) - (player.size.y * 0.5f));
        }
        sent_death_msg = false;
        enemy_exist = true;
        health = MAX_HEALTH;
        break;
    default:
        break;
    }
}

void networking_init() {
    if (client_create(&client, "127.0.0.1", 7777, NET_SEC(5))) {
        client_t client;
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
