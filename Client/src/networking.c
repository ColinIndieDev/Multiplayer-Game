#include "networking.h"

#include <cpl/cpl.h>
#include <cpstd/rand.h>
#include <cpstd/vector.h>

#include "game.h"

EXTERN_GAME_H_VARIABLES

client_t client;
int id = -1;
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;

void parse_data(char *packet_data, size_t packet_data_len, net_channels channel, void *arg) {
    (void)arg;
 
    packet_reader reader;
    uint8_t flag = packet_reader_init(&reader, (uint8_t *)packet_data, packet_data_len);
    if (channel == NET_CHANNEL_VOICE_CHAT) {
        audio_client_handle_voice_msg(&reader);
    } else {
        switch (flag) {
        case PACKET_RECEIVE_ID:
            id = packet_read_int(&reader);
            break;
        case PACKET_POS_SYNC: {
            int packet_player_id = packet_read_int(&reader);
            if (id == packet_player_id) {
                break;
            }
            players[packet_player_id].attribs.pos.x = packet_read_float(&reader);
            players[packet_player_id].attribs.pos.y = packet_read_float(&reader);
            break;
        }
        case PACKET_PROJECTILE_SYNC: {
            int packet_player_id = packet_read_int(&reader);
            if (id == packet_player_id) {
                break;
            }
            vec2f epos =
                VEC2F(packet_read_float(&reader), packet_read_float(&reader));
            int count = packet_read_int(&reader);

            play_shoot_sfx();

            for (int i = 0; i < count; i++) {
                vec2f dir =
                    VEC2F(packet_read_float(&reader), packet_read_float(&reader));

                unsigned int delay_ms = client.peer->roundTripTime;
                float elapsed = (float)delay_ms * 0.001f;

                vec2f pos = VEC2F(epos.x + (dir.x * projectile_speed * elapsed),
                                  epos.y + (dir.y * projectile_speed * elapsed));

                pthread_mutex_lock(&game_mutex);
                vec_push(players[packet_player_id].projectiles, PROJECTILE_INITIALIZER(pos, dir));
                pthread_mutex_unlock(&game_mutex);
            }
            break;
        }
        case PACKET_JOIN: {
            int packet_player_id = packet_read_int(&reader);
            if (packet_player_id != id) {
                enemy_exist[packet_player_id] = true;
            }
            break;
        }
        case PACKET_LEAVE: {
            int packet_player_id = packet_read_int(&reader);
            if (packet_player_id != id) {
                enemy_exist[packet_player_id] = false;
            }
            break;
        }
        case PACKET_DEAD: {
            int packet_player_id = packet_read_int(&reader);
            if (packet_player_id != id) {
                enemy_exist[packet_player_id] = false;
            }
            break;
        }
        case PACKET_NEW_ROUND:
            game_init_start_pos();
            sent_death_msg = false;
            pthread_mutex_lock(&game_mutex);
            for (int i = 0; i < PLAYER_TYPE_SIZE; i++) {
                enemy_exist[i] = packet_read_bool(&reader);
                if (enemy_exist[i]) {
                    vec_clear(players[i].projectiles);
                }
            }
            pthread_mutex_unlock(&game_mutex);
            selected_weapon = pcg_rand() % WEAPONS_SIZE;
            health = MAX_HEALTH;
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
        case PACKET_RECEIVE_GAME_MODE: {
            game_mode mode = packet_read_int(&reader);
            switch (mode) {
            case MODE_CLASSIC:
                max_bounces = 1;
                break;
            case MODE_SOLID:
                max_bounces = 0;
                break;
            case MODE_BOUNCY:
                max_bounces = 3;
                break;
            case MODE_CHAOS:
                max_bounces = (unsigned int)-1;
                break;
            case MODE_FRIENDLY_FIRE:
                max_bounces = 3;
                break;
            }
            cur_mode = mode;
            break;
        }
        case PACKET_ADD_SCORE: {
            players[packet_read_int(&reader)].score++;
            break;
        }
        default:
            break;
        }
    }
}

void networking_init() {
    if (client_create(&client, "100.85.197.121", 7777, NET_SEC(5))) {
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
