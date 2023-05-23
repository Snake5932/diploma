#include "boromir_client.h"
#include "utils.h"
#include "config.h"
#include "lwip/sockets.h"
#include "udp.h"
#include "wifi.h"

struct boromir_client* new_boromir_client(uint32_t roles) {
	struct boromir_client* client = (struct boromir_client*)malloc(sizeof(struct boromir_client));
	client->in_network = 0;
	client->roles = roles;
	client->writing_queue = xQueueCreate(10, 10);
	client->bad_conns = init_queue(5);
	return client;
}

void free_boromir_client(struct boromir_client* client) {
	free_queue(client->bad_conns);
	free(client);
}

void start_client(struct boromir_client* client) {
	ESP_ERROR_CHECK(wifi_init(client));

	do {
		client->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (client->sockfd == -1) {
			printf("failed to create socket\n");
			vTaskDelay(1000/portTICK_RATE_MS);
		}
	} while (client->sockfd == -1);
	printf("socket created\n");
	memset(&client->servaddr, 0, sizeof(struct sockaddr_in));
	client->servaddr.sin_family    = AF_INET;
	client->servaddr.sin_addr.s_addr = INADDR_ANY;
	client->servaddr.sin_port = htons(PORT);

	int ret;
	do {
		ret = bind(client->sockfd, (struct sockaddr *)&client->servaddr, sizeof(struct sockaddr_in));
		if (ret != 0) {
			printf("failed to bind sock\n");
			vTaskDelay(1000/portTICK_RATE_MS);
		}
	} while (ret != 0);
	printf("socket binded\n");

	xTaskCreate(udp_receiver, "udp_receiver", 2048, client, tskIDLE_PRIORITY, NULL);
	xTaskCreate(udp_sender, "udp_sender", 2048, client, tskIDLE_PRIORITY, NULL);
}

void send_msg(struct boromir_client* client, char data[10], uint32_t role, char* receiver) {
	xQueueSendToBack(client->writing_queue, data, 0);
}
