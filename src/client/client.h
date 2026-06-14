#pragma once

#include <enet.h>


typedef struct {
    ENetHost *client;
    ENetAddress address;
    ENetEvent event;
    ENetPeer *peer;
} client_t;

#define SEC_TO_MS(s) ((s) * 1000)

bool client_create(client_t *client, char *ip, int port, int wait_ms);
void client_destroy(client_t *client, int wait_ms);

typedef struct {
    ENetHost *client;
    bool running;
    void *(*parse_data)(char *, void *);
    void *parse_data_arg;
} network_thread_data;

void client_create_worker_loop(client_t *client, pthread_t *thread, network_thread_data *data, void *(*parse_data)(char *, void *), void *parse_data_arg);
void client_destroy_worker_loop(const pthread_t *thread, network_thread_data *data);

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

void send_packet(client_t *client, packet_writer *writer, int packet_flag);

