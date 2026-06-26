#include "network.h"
#include "enet.h"

#include <cpl/cpl.h>
#include <cpstd/rand.h>

bool running = true;

bool player_active[PLAYER_TYPE_SIZE] = {false};

bool round_end = false;
unsigned int last_round_ms = 0;
unsigned int next_round_dt_ms = 5000;

vec2f obstacles[(MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE)];
int obstacles_size = 0;

game_mode cur_mode = MODE_CLASSIC;

server_t server;

void network_gen_map() {
    obstacles_size = 0;
    for (int i = 0; i < (MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE);
         i++) {
        int col = i % (MAP_SIZE / PATTERN_SIZE);
        int row = i / (MAP_SIZE / PATTERN_SIZE);
        if (pcg_rand() % 5 == 0 && col != 0 &&
            col != (MAP_SIZE / PATTERN_SIZE) - 1 && row != 0 &&
            row != (MAP_SIZE / PATTERN_SIZE) - 1) {
            obstacles[obstacles_size++] =
                VEC2F(col * PATTERN_SIZE, row * PATTERN_SIZE);
        }
    }
}

void network_init() {
    server_init(&server, 7777, ENET_HOST_ANY, MAX_CLIENTS);

    pcg_rand_seed();

    cur_mode = pcg_rand() % GAME_MODE_SIZE;
    network_gen_map();
}

void network_destroy() { server_destroy(&server); }

// {{{ network_run() Helper

void network_handle_connection(ENetEvent *event) {
    int new_player_id = -1;
    for (player_type id = 0; id < PLAYER_TYPE_SIZE; id++) {
        if (!player_active[id]) {
            new_player_id = id;
            break;
        }
    }

    if (new_player_id == -1) {
        enet_peer_disconnect_now(event->peer, 0);
        printf("Server is full!\n");
    } else {
        event->peer->data = (void *)(uintptr_t)new_player_id;
        player_active[new_player_id] = true;

        {
            packet_writer writer;
            packet_writer_init(&writer, PACKET_RECEIVE_ID);
            packet_write_int(&writer, new_player_id);
            packet_send_to_client(event->peer, &writer, NET_PACKET_RELIABLE);
        }

        printf("Client with ID %d connected!\n", new_player_id);

        {
            packet_writer writer;
            packet_writer_init(&writer, PACKET_JOIN);
            packet_write_int(&writer, new_player_id);
            packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
        }

        for (player_type id = 0; id < PLAYER_TYPE_SIZE; id++) {
            if (player_active[id] && id != new_player_id) {
                packet_writer writer;
                packet_writer_init(&writer, PACKET_JOIN);
                packet_write_int(&writer, id);
                packet_send_to_client(event->peer, &writer,
                                      NET_PACKET_RELIABLE);
            }
        }

        {
            packet_writer writer;
            packet_writer_init(&writer, PACKET_RECEIVE_OBSTACLES);
            packet_write_int(&writer, obstacles_size);
            for (int i = 0; i < obstacles_size; i++) {
                packet_write_float(&writer, obstacles[i].x);
                packet_write_float(&writer, obstacles[i].y);
            }
            packet_send_to_client(event->peer, &writer, NET_PACKET_RELIABLE);
        }

        {
            packet_writer writer;
            packet_writer_init(&writer, PACKET_RECEIVE_GAME_MODE);
            packet_write_int(&writer, cur_mode);
            packet_send_to_client(event->peer, &writer, NET_PACKET_RELIABLE);
        }
    }
}

void network_handle_packets(ENetEvent *event) {
    packet_reader reader;
    uint8_t flag = packet_reader_init(&reader, event->packet->data,
                                      event->packet->dataLength);

    switch (flag) {
    case PACKET_INFO:
        printf("Read: %f\n", packet_read_float(&reader));
        break;
    case PACKET_POS_SYNC: {
        packet_writer writer;
        packet_writer_init(&writer, PACKET_POS_SYNC);
        packet_write_int(&writer, packet_read_int(&reader));
        packet_write_float(&writer, packet_read_float(&reader));
        packet_write_float(&writer, packet_read_float(&reader));
        packet_broadcast(&server, &writer, NET_PACKET_UNRELIABLE);
        break;
    }
    case PACKET_PROJECTILE_SYNC: {
        packet_writer writer;
        packet_writer_init(&writer, PACKET_PROJECTILE_SYNC);
        packet_write_int(&writer, packet_read_int(&reader));
        packet_write_float(&writer, packet_read_float(&reader));
        packet_write_float(&writer, packet_read_float(&reader));
        int count = packet_read_int(&reader);
        packet_write_int(&writer, count);
        for (int i = 0; i < count; i++) {
            packet_write_float(&writer, packet_read_float(&reader));
            packet_write_float(&writer, packet_read_float(&reader));
        }
        packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
        break;
    }
    case PACKET_DEAD: {
        packet_writer writer;
        packet_writer_init(&writer, PACKET_DEAD);
        packet_write_int(&writer, packet_read_int(&reader));
        packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
        round_end = true;
        last_round_ms = enet_time_get();

        printf("Round ended!\n");

        break;
    }
    case NET_PACKET_AUDIO_VOICE_MSG:
        audio_server_broadcast_voice_msg(&reader, &server);
        break;
    default:
        break;
    }

    enet_packet_destroy(event->packet);
}

void network_handle_disconnection(ENetEvent *event) {
    int id = (int)(uintptr_t)event->peer->data;
    player_active[id] = false;
    printf("Client with ID %d disconnected!\n", id);
    event->peer->data = NULL;

    packet_writer writer;
    packet_writer_init(&writer, PACKET_LEAVE);
    packet_write_int(&writer, id);
    packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
}

void network_handle_new_round() {
    if (round_end && enet_time_get() >= last_round_ms + next_round_dt_ms) {
        round_end = false;
        {
            packet_writer writer;
            packet_writer_init(&writer, PACKET_NEW_ROUND);
            packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
        }

        {
            network_gen_map();

            packet_writer writer;
            packet_writer_init(&writer, PACKET_RECEIVE_OBSTACLES);
            packet_write_int(&writer, obstacles_size);
            for (int i = 0; i < obstacles_size; i++) {
                packet_write_float(&writer, obstacles[i].x);
                packet_write_float(&writer, obstacles[i].y);
            }
            packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
        }

        {
            cur_mode = pcg_rand() % GAME_MODE_SIZE;
            packet_writer writer;
            packet_writer_init(&writer, PACKET_RECEIVE_GAME_MODE);
            packet_write_int(&writer, cur_mode);
            packet_broadcast(&server, &writer, NET_PACKET_RELIABLE);
        }

        printf("Started new round!\n");
    }
}

// }}}

void network_run() {
    while (running) {
        ENetEvent event;
        while (enet_host_service(server.server, &event, NET_SEC(1)) > 0) {
            network_handle_new_round();

            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                network_handle_connection(&event);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                network_handle_packets(&event);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                network_handle_disconnection(&event);
                break;
            default:
                break;
            }
        }
    }
}
