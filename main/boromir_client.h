#ifndef BOROMIR_CLIENT
#define BOROMIR_CLIENT

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "queue.h"
#include "config.h"

struct boromir_client {
	uint8_t client_ssid[32];
	uint8_t ssid_len;
	uint8_t station_mac[6];
	char prohibit_conn;
	uint8_t network_id[4];
	uint32_t roles;
	struct sockaddr_in broadcast_addr;
	SemaphoreHandle_t conn_mutex;
	xQueueHandle writing_queue;
	int sockfd;
	struct sockaddr_in servaddr;
	struct queue* bad_conns;
	struct connection* children_conns[MAX_AP_CONN];
	struct connection* parent_conn;
	uint8_t order;
};

struct msg {
	struct sockaddr_in addr;
	char* msg;
};

struct connection;

struct boromir_client* new_boromir_client(uint32_t roles);

void free_boromir_client(struct boromir_client* client);

void start_client(struct boromir_client* client);

void send_msg(struct boromir_client* client, char* data, uint8_t data_len, uint32_t role, char* dest_name, int dest_name_len);

void set_callback();

void remove_child_conn(struct boromir_client* client, uint8_t mac[6]);

void set_child_conn(struct boromir_client* client, uint8_t mac[6], uint16_t aid);

void remove_parent_conn(struct boromir_client* client);

void set_parent_conn(struct boromir_client* client, uint8_t* ssid, uint8_t ssid_len);

void connection_manager(void *pvParameters);

#endif
