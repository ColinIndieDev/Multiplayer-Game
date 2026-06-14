#include "client.h"
#include "enet.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

// {{{ Thread

void *_net_worker_loop(void *arg) {
    network_thread_data *data = (network_thread_data *)arg;
    while (data->running) {
        ENetEvent event;
        while (enet_host_service(data->client, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                data->parse_data((char *)event.packet->data, data->parse_data_arg);
                enet_packet_destroy(event.packet);
            }
        }
    }
    free(data);
    return NULL;
}

void client_create_worker_loop(client_t *client, pthread_t *thread, network_thread_data *data, void *(*parse_data)(char *, void *), void *parse_data_arg) {
    data->client = client->client;
    data->parse_data = parse_data;
    data->parse_data_arg = parse_data_arg;
    data->running = true;
    pthread_create(thread, NULL, _net_worker_loop, data);
}

void client_destroy_worker_loop(const pthread_t *thread, network_thread_data *data) {
    data->running = false;
    pthread_join(*thread, NULL);
}

// }}}

// {{{ Client Creation and Deletion

bool client_create(client_t *client, char *ip, int port, int wait_ms) {
    if (enet_initialize()) {
        fprintf(stderr, "Failed to init ENet\n");
        exit(-1);
    }
    atexit(enet_deinitialize);

    client->client = NULL;
    client->client = enet_host_create(NULL, 1, 1, 0, 0);
    if (!client) {
        fprintf(stderr, "Failed to create client host\n");
        exit(-1);
    }

    if (!ip) {
        fprintf(stderr, "IP is invalid\n");
        exit(-1);
    }

    enet_address_set_host(&client->address, ip);
    client->address.port = port;

    client->peer = enet_host_connect(client->client, &client->address, 1, 0);
    if (!client->peer) {
        fprintf(stderr, "No peers available for connection\n");
        exit(-1);
    }

    if (enet_host_service(client->client, &client->event, wait_ms) > 0 &&
        client->event.type == ENET_EVENT_TYPE_CONNECT) {
        return true;
    }
    enet_peer_reset(client->peer);
    return false;
}

void client_destroy(client_t *client, int wait_ms) {
    enet_peer_disconnect(client->peer, 0);
    while (enet_host_service(client->client, &client->event, wait_ms) > 0) {
        if (client->event.type == ENET_EVENT_TYPE_RECEIVE) {
            enet_packet_destroy(client->event.packet);
        } else if (client->event.type == ENET_EVENT_TYPE_DISCONNECT) {
            break;
        }
    }
}

// }}}

// {{{ Packets

void packet_writer_init(packet_writer *writer, uint8_t packet_id) {
    writer->data[0] = packet_id;
    writer->size = 1;
}

void packet_write_char(packet_writer *writer, char val) {
    writer->data[writer->size++] = val;
}

void packet_write_int(packet_writer *writer, int val) {
    memcpy(&writer->data[writer->size], &val, sizeof(int));
    writer->size += sizeof(int);
}

void packet_write_uint(packet_writer *writer, unsigned int val) {
    memcpy(&writer->data[writer->size], &val, sizeof(unsigned int));
    writer->size += sizeof(unsigned int);
}

void packet_write_float(packet_writer *writer, float val) {
    memcpy(&writer->data[writer->size], &val, sizeof(float));
    writer->size += sizeof(float);
}

void packet_write_bool(packet_writer *writer, bool val) {
    memcpy(&writer->data[writer->size], &val, sizeof(bool));
    writer->size += sizeof(bool);
}

uint8_t packet_reader_init(packet_reader *reader, uint8_t *data, size_t size) {
    reader->data = data;
    reader->size = size;
    reader->pos = 0;

    return reader->data[reader->pos++];
}

char packet_read_char(packet_reader *reader) {
    return (char)reader->data[reader->pos++];
}

int packet_read_int(packet_reader *reader) {
    int val = 0;
    memcpy(&val, &reader->data[reader->pos], sizeof(int));
    reader->pos += sizeof(int);
    return val;
}

unsigned int packet_read_uint(packet_reader *reader) {
    unsigned int val = 0;
    memcpy(&val, &reader->data[reader->pos], sizeof(unsigned int));
    reader->pos += sizeof(unsigned int);
    return val;
}

float packet_read_float(packet_reader *reader) {
    float val = 0;
    memcpy(&val, &reader->data[reader->pos], sizeof(float));
    reader->pos += sizeof(float);
    return val;
}

bool packet_read_bool(packet_reader *reader) {
    bool val = 0;
    memcpy(&val, &reader->data[reader->pos], sizeof(bool));
    reader->pos += sizeof(bool);
    return val;
}

void send_packet(client_t *client, packet_writer *writer, int packet_flag) {
    ENetPacket *packet =
        enet_packet_create(writer->data, writer->size, packet_flag);
    enet_peer_send(client->peer, 0, packet);
}

// }}}
