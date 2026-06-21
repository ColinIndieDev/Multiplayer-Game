#define CPL_IMPLEMENTATION
#include <cpl/cpl.h>

#define CPRNG_IMPL
#include <cpstd/cprng.h>

#include <stdio.h>

#define MAX_CLIENTS 2

#define MAP_SIZE 1000
#define PATTERN_SIZE 100

bool running = true;

typedef enum : uint8_t {
    PACKET_INFO,
    PACKET_RECEIVE_ID,
    PACKET_POS_SYNC,
    PACKET_PROJECTILE_SYNC,
    PACKET_LEAVE,
    PACKET_JOIN,
    PACKET_DEAD,
    PACKET_NEW_ROUND,
    PACKET_RECEIVE_OBSTACLES
} packet_id;

typedef enum { PLAYER_RED = 0, PLAYER_BLUE, PLAYER_TYPE_SIZE } player_type;

bool player_active[PLAYER_TYPE_SIZE] = {false};

bool round_end = false;
unsigned int last_round_ms = 0;
unsigned int next_round_dt_ms = 5000;

vec2f obstacles[(MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE)];
int obstacles_size = 0;

int main(void) {
    server_t server;
    server_init(&server, 7777, ENET_HOST_ANY, MAX_CLIENTS);

    cprng_rand_seed();

    for (int i = 0; i < (MAP_SIZE / PATTERN_SIZE) * (MAP_SIZE / PATTERN_SIZE);
         i++) {
        int col = i % (MAP_SIZE / PATTERN_SIZE);
        int row = i / (MAP_SIZE / PATTERN_SIZE);
        if (cprng_rand() % 5 == 0) {
            obstacles[obstacles_size++] =
                VEC2F(col * PATTERN_SIZE, row * PATTERN_SIZE);
        }
    }

    while (running) {
        ENetEvent event;
        while (enet_host_service(server.server, &event, NET_SEC(1)) > 0) {
            if (round_end &&
                enet_time_get() >= last_round_ms + next_round_dt_ms) {
                round_end = false;
                {
                    packet_writer writer;
                    packet_writer_init(&writer, PACKET_NEW_ROUND);
                    broadcast_packet(&server, &writer, NET_PACKET_RELIABLE);
                }

                {
                    obstacles_size = 0;
                    for (int i = 0; i < (MAP_SIZE / PATTERN_SIZE) *
                                            (MAP_SIZE / PATTERN_SIZE);
                         i++) {
                        int col = i % (MAP_SIZE / PATTERN_SIZE);
                        int row = i / (MAP_SIZE / PATTERN_SIZE);
                        if (cprng_rand() % 5 == 0) {
                            obstacles[obstacles_size++] =
                                VEC2F(col * PATTERN_SIZE, row * PATTERN_SIZE);
                        }
                    }
                    packet_writer writer;
                    packet_writer_init(&writer, PACKET_RECEIVE_OBSTACLES);
                    packet_write_int(&writer, obstacles_size);
                    for (int i = 0; i < obstacles_size; i++) {
                        packet_write_float(&writer, obstacles[i].x);
                        packet_write_float(&writer, obstacles[i].y);
                    }
                    send_packet_to_client(event.peer, &writer,
                                          NET_PACKET_RELIABLE);
                }

                printf("Started new round!\n");
            }

            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                int new_player_id = -1;
                for (player_type id = 0; id < PLAYER_TYPE_SIZE; id++) {
                    if (!player_active[id]) {
                        new_player_id = id;
                        break;
                    }
                }

                if (new_player_id == -1) {
                    enet_peer_disconnect_now(event.peer, 0);
                    printf("Server is full!\n");
                } else {
                    event.peer->data = (void *)(uintptr_t)new_player_id;
                    player_active[new_player_id] = true;

                    {
                        packet_writer writer;
                        packet_writer_init(&writer, PACKET_RECEIVE_ID);
                        packet_write_int(&writer, new_player_id);
                        send_packet_to_client(event.peer, &writer,
                                              NET_PACKET_RELIABLE);
                    }

                    printf("Client with ID %d connected!\n", new_player_id);

                    {
                        packet_writer writer;
                        packet_writer_init(&writer, PACKET_JOIN);
                        packet_write_int(&writer, new_player_id);
                        broadcast_packet(&server, &writer, NET_PACKET_RELIABLE);
                    }

                    for (player_type id = 0; id < PLAYER_TYPE_SIZE; id++) {
                        if (player_active[id] && id != new_player_id) {
                            packet_writer writer;
                            packet_writer_init(&writer, PACKET_JOIN);
                            packet_write_int(&writer, id);
                            send_packet_to_client(event.peer, &writer,
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
                        send_packet_to_client(event.peer, &writer,
                                              NET_PACKET_RELIABLE);
                    }
                }

                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                packet_reader reader;
                packet_id flag = packet_reader_init(&reader, event.packet->data,
                                                    event.packet->dataLength);

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
                    broadcast_packet(&server, &writer,
                                     ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
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
                    broadcast_packet(&server, &writer, NET_PACKET_RELIABLE);
                    break;
                }
                case PACKET_DEAD: {
                    packet_writer writer;
                    packet_writer_init(&writer, PACKET_DEAD);
                    packet_write_int(&writer, packet_read_int(&reader));
                    broadcast_packet(&server, &writer, NET_PACKET_RELIABLE);
                    round_end = true;
                    last_round_ms = enet_time_get();

                    printf("Round ended!\n");

                    break;
                }
                default:
                    break;
                }

                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                int id = (int)(uintptr_t)event.peer->data;
                player_active[id] = false;
                printf("Client with ID %d disconnected!\n", id);
                event.peer->data = NULL;

                packet_writer writer;
                packet_writer_init(&writer, PACKET_LEAVE);
                packet_write_int(&writer, id);
                broadcast_packet(&server, &writer, NET_PACKET_RELIABLE);
                break;
            }
            default:
                break;
            }
        }
    }

    server_destroy(&server);
}
