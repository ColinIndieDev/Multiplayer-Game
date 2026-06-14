#define CPL_IMPLEMENTATION
#include <cpl/cpl.h>

#define ENET_IMPLEMENTATION
#include <enet.h>

#include <pthread.h>

#include "client.h"

void *parse_data(char *packet_data, void *arg) {
    (void)packet_data;
    (void)arg;

    return NULL;
}

pthread_t thread;
network_thread_data data;

typedef enum : uint8_t { PACKET_INFO } packet_id;

int main(void) {
    client_t client;
    if (client_create(&client, "127.0.0.1", 7777, SEC_TO_MS(5))) {
        printf("Connection successful\n");
    } else {
        fprintf(stderr, "Connection failed\n");
        exit(-1);
    }

    client_create_worker_loop(&client, &thread, &data, parse_data, NULL);

    init_window(800, 800, "Multiplayer Game", OPENGL_VER_3_3);
    while (!window_should_close()) {
        update();

        packet_writer writer;
        packet_writer_init(&writer, PACKET_INFO);

        packet_write_float(&writer, 67.67f);

        send_packet(&client, &writer, ENET_PACKET_FLAG_RELIABLE);

        clear_background(BLACK);

        end_frame();
    }
    close_window();

    client_destroy_worker_loop(&thread, &data);

    client_destroy(&client, SEC_TO_MS(3));
    printf("Disconnection successful\n");
}
