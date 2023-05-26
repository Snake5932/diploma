#include "boromir_client.h"
#include "utils.h"
#include "config.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "udp.h"
#include "wifi.h"
#include "boromir.h"
#include <stdlib.h>

void free_boromir_client(struct boromir_client* client) {
	for (int i = 0; i < MAX_AP_CONN; i++) {
			free(client->children_conns[i]);
	}
	free(client->parent_conn);
	free_queue(client->bad_conns);
	free(client);
}

struct boromir_client* new_boromir_client(uint32_t roles) {
	srand(24664347);
	struct boromir_client* client = (struct boromir_client*)malloc(sizeof(struct boromir_client));
	client->prohibit_conn = 0;
	client->roles = roles;
	client->writing_queue = xQueueCreate(50, sizeof(struct msg));
	client->bad_conns = init_queue(5);
	client->conn_mutex = xSemaphoreCreateMutex();
	for (int i = 0; i < MAX_AP_CONN; i++) {
		client->children_conns[i] = NULL;
	}
	client->parent_conn = NULL;
	client->order = ORDER;
	client->broadcast_addr.sin_family = AF_INET;
	client->broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
	client->broadcast_addr.sin_port = htons(PORT);
	rand_bytes(client->network_id, 4);
	client->ssid_len = strlen(SSID);
	memcpy(client->client_ssid, SSID, client->ssid_len);
	return client;
}

void start_client(struct boromir_client* client) {
	ESP_ERROR_CHECK(wifi_init(client));
	ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, client->station_mac));

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

	xTaskCreate(connection_manager, "connection_manager", 2048, client, 3, NULL);

	xTaskCreate(udp_receiver, "udp_receiver", 2048, client, tskIDLE_PRIORITY, NULL);
	xTaskCreate(udp_sender, "udp_sender", 2048, client, tskIDLE_PRIORITY, NULL);
}

void send_msg(struct boromir_client* client, char* data, uint8_t data_len, uint32_t role, char* dest_name, int dest_name_len) {

}

void connection_manager(void *pvParameters) {
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	for(;;) {
		xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
		uint32_t now = (uint32_t)xTaskGetTickCount();
		for (int i = 0; i < MAX_AP_CONN; i++) {
			if (client->children_conns[i] != NULL) {
				if ((now - client->children_conns[i]->timestamp)/portTICK_RATE_MS > 300) {
					esp_wifi_deauth_sta(client->children_conns[i]->aid);
					printf("deauthenticated sta as unresponsive\n");
				} else {
					if (client->children_conns[i]->status == ESTABLISHED) {
						struct msg message = {.addr = client->children_conns[i]->cliaddr, make_beacon(client)};
						if (xQueueSendToBack(client->writing_queue, &message, 2000) == pdTRUE) {
							printf("sent beacon\n");
						} else {
							free(message.msg);
						}
					} else if (client->children_conns[i]->status == BROADCAST_PREPARED) {
						if (!client->prohibit_conn) {
							struct msg message = {.addr = client->broadcast_addr, make_broadcast(client)};
							if (xQueueSendToFront(client->writing_queue, &message, 2000) == pdTRUE) {
								client->children_conns[i]->status = BROADCAST_SENT;
								client->children_conns[i]->timestamp = (uint32_t)xTaskGetTickCount();
								printf("broadcast sent\n");
							} else {
								free(message.msg);
							}
						}
					} else if (client->children_conns[i]->status == CONNECT_RECEIVED) {
						if (!client->prohibit_conn) {
							struct msg message = {.addr = client->broadcast_addr, make_established(client)};
							if (xQueueSendToFront(client->writing_queue, &message, 2000) == pdTRUE) {
								client->children_conns[i]->status = ESTABLISHED;
								client->children_conns[i]->timestamp = (uint32_t)xTaskGetTickCount();
								printf("connection established\n");
							} else {
								free(message.msg);
							}
						}
					}
				}
			}
		}

		if (client->parent_conn != NULL) {
			if ((now - client->parent_conn->timestamp)/portTICK_RATE_MS > 300) {
				esp_wifi_disconnect();
				printf("disconnected from ap\n");
			} else {
				if (client->parent_conn->status == BROADCAST_RECEIVED) {
					struct msg message = {.addr = client->broadcast_addr, make_connect(client)};
					if (xQueueSendToFront(client->writing_queue, &message, 2000) == pdTRUE) {
						client->parent_conn->status = CONNECT_SENT;
						client->parent_conn->timestamp = (uint32_t)xTaskGetTickCount();
						printf("connect sent\n");
					} else {
						free(message.msg);
					}
				}
			}
		}

		xSemaphoreGive(client->conn_mutex);
		vTaskDelay(10000/portTICK_RATE_MS);
	}
	vTaskDelete(NULL);
}

//TODO: не забыть про сообщения установки соединения и служебные сообщения по удалению ролей и пр.
//TODO: запрет на новые подключения во время подключения станции
void set_child_conn(struct boromir_client* client, uint8_t mac[6], uint16_t aid) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] == NULL) {
			printf("setting child\n");
			struct connection* conn = (struct connection*)malloc(sizeof(struct connection));
			conn->aid = aid;
			memcpy(conn->mac, mac, 6);
			conn->status = BROADCAST_PREPARED;

			if (!client->prohibit_conn) {
				struct msg message = {.addr = client->broadcast_addr, make_broadcast(client)};
				if (xQueueSendToFront(client->writing_queue, &message, 2000) == pdTRUE) {
					conn->status = BROADCAST_SENT;
					printf("broadcast sent\n");
				} else {
					free(message.msg);
				}
			}

			conn->timestamp = (uint32_t)xTaskGetTickCount();
			client->children_conns[i] = conn;
			break;
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void remove_child_conn(struct boromir_client* client, uint8_t mac[6]) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL && cmp_mac(client->children_conns[i]->mac, mac)) {
			printf("deleting child\n");
			free(client->children_conns[i]);
			client->children_conns[i] = NULL;
			break;
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void set_parent_conn(struct boromir_client* client, uint8_t* ssid, uint8_t ssid_len) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("setting parent\n");
	struct connection* conn = (struct connection*)malloc(sizeof(struct connection));
	memcpy(conn->ssid, ssid, ssid_len);
	conn->ssid_len = ssid_len;
	conn->status = WIFI_CONNECTED;
	conn->timestamp = (uint32_t)xTaskGetTickCount();
	client->parent_conn = conn;
	xSemaphoreGive(client->conn_mutex);
}

void remove_parent_conn(struct boromir_client* client) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("deleting parent\n");
	free(client->parent_conn);
	client->parent_conn = NULL;
	rand_bytes(client->network_id, 4);
	xSemaphoreGive(client->conn_mutex);
}

void process_broadcast(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing broadcast\n");

	if (client->parent_conn != NULL &&
		cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
			msg->sender_ssid, msg->ssid_len)) {
		if (client->parent_conn->status == WIFI_CONNECTED) {
			if (!cmp_slices(client->network_id, 4, msg->network_id, 4)) {
				if (!has_conn(client->children_conns, msg->sender_mac) || cmp_order(&client->order, 1, &msg->order, 1) > 0) {

					printf("%u\n", msg->ip);
					client->parent_conn->cliaddr.sin_family = AF_INET;
					client->parent_conn->cliaddr.sin_addr.s_addr = msg->ip;
					client->parent_conn->cliaddr.sin_port = htons(PORT);

					client->parent_conn->status = BROADCAST_RECEIVED;

					client->prohibit_conn = 1;

					struct msg message = {.addr = client->parent_conn->cliaddr, make_connect(client)};
					if (xQueueSendToFront(client->writing_queue, &message, 2000) == pdTRUE) {
						client->parent_conn->status = CONNECT_SENT;
						printf("connect sent\n");
					} else {
						free(message.msg);
					}

					client->parent_conn->timestamp = (uint32_t)xTaskGetTickCount();

				} else {
					//TODO: disconnect?
				}
			} else {
				//TODO: disconnect?
			}
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void process_connect(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing connect\n");
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL && cmp_mac(client->children_conns[i]->mac, msg->sender_mac)) {
			if (client->children_conns[i]->status == BROADCAST_SENT) {

				client->children_conns[i]->cliaddr.sin_family = AF_INET;
				client->children_conns[i]->cliaddr.sin_addr.s_addr = msg->ip;
				client->children_conns[i]->cliaddr.sin_port = htons(PORT);

				client->children_conns[i]->status = CONNECT_RECEIVED;
				client->children_conns[i]->roles = msg->roles;

				client->children_conns[i]->ssid_len = msg->ssid_len;
				memcpy(client->children_conns[i]->ssid, msg->sender_ssid, msg->ssid_len);

				if (!client->prohibit_conn) {
					struct msg message = {.addr = client->children_conns[i]->cliaddr, make_established(client)};
					if (xQueueSendToFront(client->writing_queue, &message, 2000) == pdTRUE) {
						client->children_conns[i]->status = ESTABLISHED;
						printf("established sent\n");
					} else {
						free(message.msg);
					}
				}
				client->children_conns[i]->timestamp = (uint32_t)xTaskGetTickCount();
			}
			break;
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void process_establishing(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing establishing\n");
	if (client->parent_conn != NULL &&
		cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
			msg->sender_ssid, msg->ssid_len)) {
		if (client->parent_conn->status == CONNECT_SENT) {
			printf("conn to parent established\n");
			client->parent_conn->timestamp = (uint32_t)xTaskGetTickCount();
			memcpy(client->network_id, msg->network_id, 4);
			client->prohibit_conn = 0;
			client->parent_conn->status = ESTABLISHED;
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void process_beacon_answ(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing beacon answ\n");
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL &&
				cmp_slices(client->children_conns[i]->ssid, client->children_conns[i]->ssid_len,
						msg->sender_ssid, msg->ssid_len)) {
			if (client->children_conns[i]->status == ESTABLISHED) {
				client->children_conns[i]->timestamp = (uint32_t)xTaskGetTickCount();
			}
			break;
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void process_beacon(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing beacon\n");
	if (client->parent_conn != NULL &&
			cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
					msg->sender_ssid, msg->ssid_len)) {
		if (client->parent_conn->status == ESTABLISHED) {
			struct msg message = {.addr = client->parent_conn->cliaddr, make_beacon_answ(client)};
			if (xQueueSendToBack(client->writing_queue, &message, 2000) == pdTRUE) {
				printf("sent beacon answ\n");
			} else {
				free(message.msg);
			}
			client->parent_conn->timestamp = (uint32_t)xTaskGetTickCount();
		}
	}
	xSemaphoreGive(client->conn_mutex);
}
