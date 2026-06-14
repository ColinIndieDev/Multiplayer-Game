#pragma once

#include <enet.h>

#include <signal.h>

typedef struct {
    ENetHost *server;
    ENetAddress address;
} server_t;

#define SEC_TO_MS(s) ((s) * 1000)

void server_init(server_t *server, int port, struct in6_addr host,
                 int max_clients);

void server_destroy(server_t *server);

typedef struct {
    uint8_t data[1024];
    size_t size;
} packet_writer;

void packet_writer_init(packet_writer *writer, uint8_t packet_id);
void packet_write_char(packet_writer *writer, char val);
void packet_write_int(packet_writer *writer, int val);
void packet_write_uint(packet_writer *writer, unsigned int val);
void packet_write_float(packet_writer *writer, float val);
void packet_write_bool(packet_writer *writer, bool val);

typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
} packet_reader;

uint8_t packet_reader_init(packet_reader *reader, uint8_t *data, size_t size);
char packet_read_char(packet_reader *reader);
int packet_read_int(packet_reader *reader);
unsigned int packet_read_uint(packet_reader *reader);
float packet_read_float(packet_reader *reader);
bool packet_read_bool(packet_reader *reader);

void send_packet(ENetPeer *peer, packet_writer *writer, int packet_flag);
void broadcast_packet(server_t *server, packet_writer *writer, int packet_flag);
