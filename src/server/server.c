#include "server.h"

#include <stdio.h>

void server_init(server_t *server, int port, struct in6_addr host,
                 int max_clients) {
    if (enet_initialize()) {
        fprintf(stderr, "Failed to init ENet\n");
        exit(-1);
    }
    atexit(enet_deinitialize);

    server->address.host = host;
    server->address.port = port;

    server->server = enet_host_create(&server->address, max_clients, 1, 0, 0);
    if (!server) {
        fprintf(stderr, "Failed to create server host\n");
        exit(-1);
    }
}

void server_destroy(server_t *server) { enet_host_destroy(server->server); }

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

void send_packet(ENetPeer *peer, packet_writer *writer, int packet_flag) {
    ENetPacket *packet =
        enet_packet_create(writer->data, writer->size, packet_flag);
    enet_peer_send(peer, 0, packet);
}

void broadcast_packet(server_t *server, packet_writer *writer,
                      int packet_flag) {
    ENetPacket *packet =
        enet_packet_create(writer->data, writer->size, packet_flag);
    enet_host_broadcast(server->server, 0, packet);
}

// }}}
