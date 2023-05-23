#ifndef BOROMIR_CLIENT
#define BOROMIR_CLIENT

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "queue.h"

struct boromir_client {
	uint8_t client_ssid[32];
	uint8_t ssid_len;
	uint8_t in_network;
	uint8_t network_id[4];
	uint32_t roles;
	xQueueHandle writing_queue;
	int sockfd;
	struct sockaddr_in servaddr;
	struct queue* bad_conns;
};

struct boromir_client* new_boromir_client(uint32_t roles);

void free_boromir_client(struct boromir_client* client);

void start_client(struct boromir_client* client);

void send_msg(struct boromir_client* client, char data[10], uint32_t role, char* receiver);

void set_callback();

#endif
