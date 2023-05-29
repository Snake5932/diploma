#include "boromir_client.h"
#include "utils.h"
#include "config.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "udp.h"
#include "wifi.h"
#include "boromir.h"
#include <stdlib.h>

void resend_critical(void* key, size_t ksize, uintptr_t value, void* usr) {
	struct boromir_client* client = (struct boromir_client*)usr;
	struct msg* message = (struct msg*)value;

	if (message->type == NET_ID_UPD) {
		int ind = -1;
		for (int i = 0; i < MAX_AP_CONN; i++) {
			if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED &&
					cmp_slices(client->children_conns[i]->ssid, client->children_conns[i]->ssid_len,
							message->dest_ssid, message->ssid_len)) {
				if (client->children_conns[i]->status == ESTABLISHED) {
					ind = i;
				}
				break;
			}
		}

		if (ind != -1) {
			printf("resending net_id_upd\n");
			free(message);
			struct msg* new_message = (struct msg*)malloc(sizeof(struct msg));
			new_message->addr = client->children_conns[ind]->cliaddr;
			memcpy(new_message->dest_ssid, client->children_conns[ind]->ssid, client->children_conns[ind]->ssid_len);
			new_message->ssid_len = client->children_conns[ind]->ssid_len;
			new_message->type = NET_ID_UPD;
			new_message->msg = make_net_id_update(client, key);
			if (xQueueSendToFront(client->writing_queue, new_message, 2000) != pdTRUE) {
				free(new_message->msg);
			}
			hashmap_set(client->critical_msg, key, 4, (uintptr_t)(void*)new_message);
		} else {
			hashmap_remove_free(client->critical_msg, key, 4, free_map_entry, NULL);
		}
	} else if (message->type == ROLE_UPD) {
		if (client->parent_conn != NULL && client->parent_conn->status == ESTABLISHED &&
				cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len, message->dest_ssid, message->ssid_len)) {
			printf("resending role_upd\n");
			free(message);
			struct msg* new_message = (struct msg*)malloc(sizeof(struct msg));
			new_message->addr = client->parent_conn->cliaddr;
			memcpy(new_message->dest_ssid, client->parent_conn->ssid, client->parent_conn->ssid_len);
			new_message->ssid_len = client->parent_conn->ssid_len;
			new_message->type = ROLE_UPD;
			new_message->msg = make_role_update(client, key);
			if (xQueueSendToFront(client->writing_queue, new_message, 2000) != pdTRUE) {
				free(new_message->msg);
			}
			hashmap_set(client->critical_msg, key, 4, (uintptr_t)(void*)new_message);
		} else {
			hashmap_remove_free(client->critical_msg, key, 4, free_map_entry, NULL);
		}
	} else {
		printf("found smth else\n");
//		if (xQueueSendToFront(client->writing_queue, &message, 2000) != pdTRUE) {
//			free(new_message->msg);
//		}
//		hashmap_set(client->critical_msg, key, 4, (uintptr_t)(void*)new_message);
	}
}

void free_boromir_client(struct boromir_client* client) {
	for (int i = 0; i < MAX_AP_CONN; i++) {
			free(client->children_conns[i]);
	}
	free(client->parent_conn);
	free_queue(client->bad_conns);
	hashmap_free(client->critical_msg);
	free(client);
}

struct boromir_client* new_boromir_client(uint32_t roles) {
	srand(RAND_SEED);
	struct boromir_client* client = (struct boromir_client*)malloc(sizeof(struct boromir_client));
	client->prohibit_conn = 0;
	client->roles = roles;
	client->tree_roles = roles;
	client->writing_queue = xQueueCreate(50, sizeof(struct msg));
	client->bad_conns = init_queue(5);
	client->conn_mutex = xSemaphoreCreateMutex();
	for (int i = 0; i < MAX_AP_CONN; i++) {
		client->children_conns[i] = NULL;
	}
	client->parent_conn = NULL;
	client->order = ORDER;
	client->broadcast_addr.sin_family = AF_INET;
	client->broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
	client->broadcast_addr.sin_port = htons(PORT);
	rand_bytes(client->network_id, 4);
	client->ssid_len = strlen(SSID);
	memcpy(client->client_ssid, SSID, client->ssid_len);
	client->critical_msg = hashmap_create();
	client->event_handler = NULL;
	client->usr = NULL;
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
						struct msg message = {.addr = client->children_conns[i]->cliaddr, .msg = make_beacon(client)};
						if (xQueueSendToBack(client->writing_queue, &message, 2000) == pdTRUE) {
							printf("sent beacon\n");
						} else {
							free(message.msg);
						}
					} else if (client->children_conns[i]->status == BROADCAST_PREPARED) {
						if (!client->prohibit_conn) {
							struct msg message = {.addr = client->broadcast_addr, .msg = make_broadcast(client)};
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
							struct msg message = {.addr = client->broadcast_addr, .msg = make_established(client)};
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
				printf("disconnected from ap as unresponsive\n");
				esp_wifi_disconnect();
			} else {
				if (client->parent_conn->status == BROADCAST_RECEIVED) {
					struct msg message = {.addr = client->broadcast_addr, .msg = make_connect(client)};
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

		hashmap_iterate(client->critical_msg, resend_critical, client);

		xSemaphoreGive(client->conn_mutex);
		vTaskDelay(10000/portTICK_RATE_MS);
	}
	vTaskDelete(NULL);
}

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
				struct msg message = {.addr = client->broadcast_addr, .msg = make_broadcast(client)};
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

	enum connection_status status = DUMMY;

	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL && cmp_mac(client->children_conns[i]->mac, mac)) {
			printf("deleting child\n");
			status = client->children_conns[i]->status;
			free(client->children_conns[i]);
			client->children_conns[i] = NULL;
			break;
		}
	}

	uint32_t new_roles = client->roles;

	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL) {
			new_roles = new_roles | client->children_conns[i]->roles;
		}
	}

	client->tree_roles = new_roles;

	if (client->parent_conn != NULL && (status == ESTABLISHED || status == CONNECT_RECEIVED)) {
		printf("updating roles\n");
		uint8_t* msg_id = (uint8_t*)malloc(4 * sizeof(uint8_t));
		rand_bytes(msg_id, 4);
		struct msg* message = (struct msg*)malloc(sizeof(struct msg));
		message->addr = client->parent_conn->cliaddr;
		memcpy(message->dest_ssid, client->parent_conn->ssid, client->parent_conn->ssid_len);
		message->ssid_len = client->parent_conn->ssid_len;
		message->type = ROLE_UPD;
		message->msg = make_role_update(client, msg_id);
		if (xQueueSendToFront(client->writing_queue, message, 2000) != pdTRUE) {
			free(message->msg);
		}
		hashmap_set(client->critical_msg, msg_id, 4, (uintptr_t)(void*)message);
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

	enum connection_status status = DUMMY;

	if (client->parent_conn != NULL) {
		status = client->parent_conn->status;
	}

	free(client->parent_conn);
	client->parent_conn = NULL;

	if (status == ESTABLISHED) {
		rand_bytes(client->network_id, 4);

		for (int i = 0; i < MAX_AP_CONN; i++) {
			if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED) {
				uint8_t* msg_id = (uint8_t*)malloc(4 * sizeof(uint8_t));
				rand_bytes(msg_id, 4);
				struct msg* message = (struct msg*)malloc(sizeof(struct msg));
				message->addr = client->children_conns[i]->cliaddr;
				memcpy(message->dest_ssid, client->children_conns[i]->ssid, client->children_conns[i]->ssid_len);
				message->ssid_len = client->children_conns[i]->ssid_len;
				message->type = NET_ID_UPD;
				message->msg = make_net_id_update(client, msg_id);
				if (xQueueSendToFront(client->writing_queue, message, 2000) != pdTRUE) {
					free(message->msg);
				}
				hashmap_set(client->critical_msg, msg_id, 4, (uintptr_t)(void*)message);
			}
		}
	}

	xSemaphoreGive(client->conn_mutex);
}

void process_broadcast(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing broadcast from %.9s\n", msg->sender_ssid);

	if (client->parent_conn != NULL &&
		cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
			msg->sender_ssid, msg->ssid_len)) {
		if (client->parent_conn->status == WIFI_CONNECTED) {
			if (!cmp_slices(client->network_id, 4, msg->network_id, 4)) {
				if (!has_conn(client->children_conns, msg->sender_mac) || cmp_order(&client->order, 1, &msg->order, 1) > 0) {

					client->parent_conn->cliaddr.sin_family = AF_INET;
					client->parent_conn->cliaddr.sin_addr.s_addr = msg->ip;
					client->parent_conn->cliaddr.sin_port = htons(PORT);

					client->parent_conn->status = BROADCAST_RECEIVED;

					client->prohibit_conn = 1;

					struct msg message = {.addr = client->parent_conn->cliaddr, .msg = make_connect(client)};
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

//TODO: стоит переместить добавление ролей в часть с established

void process_connect(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing connect from %.9s\n", msg->sender_ssid);
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

				client->tree_roles = client->tree_roles | msg->roles;

				if (!client->prohibit_conn) {
					struct msg message = {.addr = client->children_conns[i]->cliaddr, .msg = make_established(client)};
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

	if (client->parent_conn != NULL && client->parent_conn->status == ESTABLISHED) {
		printf("updating roles\n");
		uint8_t* msg_id = (uint8_t*)malloc(4 * sizeof(uint8_t));
		rand_bytes(msg_id, 4);
		struct msg* message = (struct msg*)malloc(sizeof(struct msg));
		message->addr = client->parent_conn->cliaddr;
		memcpy(message->dest_ssid, client->parent_conn->ssid, client->parent_conn->ssid_len);
		message->ssid_len = client->parent_conn->ssid_len;
		message->type = ROLE_UPD;
		message->msg = make_role_update(client, msg_id);
		if (xQueueSendToFront(client->writing_queue, message, 2000) != pdTRUE) {
			free(message->msg);
		}
		hashmap_set(client->critical_msg, msg_id, 4, (uintptr_t)(void*)message);
	}

	xSemaphoreGive(client->conn_mutex);
}

void process_establishing(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing establishing from %.9s\n", msg->sender_ssid);
	if (client->parent_conn != NULL &&
		cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
			msg->sender_ssid, msg->ssid_len)) {
		if (client->parent_conn->status == CONNECT_SENT) {
			printf("conn to parent established\n");
			client->parent_conn->timestamp = (uint32_t)xTaskGetTickCount();
			memcpy(client->network_id, msg->network_id, 4);
			client->prohibit_conn = 0;
			client->parent_conn->status = ESTABLISHED;

			printf("updating net_id after establishing\n");
			for (int i = 0; i < MAX_AP_CONN; i++) {
				if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED) {
					uint8_t* msg_id = (uint8_t*)malloc(4 * sizeof(uint8_t));
					rand_bytes(msg_id, 4);
					struct msg* message = (struct msg*)malloc(sizeof(struct msg));
					message->addr = client->children_conns[i]->cliaddr;
					memcpy(message->dest_ssid, client->children_conns[i]->ssid, client->children_conns[i]->ssid_len);
					message->ssid_len = client->children_conns[i]->ssid_len;
					message->type = NET_ID_UPD;
					message->msg = make_net_id_update(client, msg_id);
					if (xQueueSendToFront(client->writing_queue, message, 2000) != pdTRUE) {
						free(message->msg);
					}
					hashmap_set(client->critical_msg, msg_id, 4, (uintptr_t)(void*)message);
				}
			}
		}
	}
	xSemaphoreGive(client->conn_mutex);
}

void process_beacon_answ(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing beacon answ from %.9s\n", msg->sender_ssid);
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
	printf("processing beacon from %.9s\n", msg->sender_ssid);
	if (client->parent_conn != NULL &&
			cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
					msg->sender_ssid, msg->ssid_len)) {
		if (client->parent_conn->status == ESTABLISHED) {
			struct msg message = {.addr = client->parent_conn->cliaddr, .msg = make_beacon_answ(client)};
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

void process_recv_answ(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing recv answ from %.9s\n", msg->sender_ssid);

	uintptr_t v;
	bool res = hashmap_get(client->critical_msg, msg->msg_id, 4, &v);
	if (!res) {
		return;
	}
	printf("removing after receiving answ\n");
	hashmap_remove_free(client->critical_msg, msg->msg_id, 4, free_map_entry, NULL);

	xSemaphoreGive(client->conn_mutex);
}

void process_net_id_upd(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing net id upd from %.9s\n", msg->sender_ssid);

	if (client->parent_conn != NULL &&
			client->parent_conn->status == ESTABLISHED &&
			cmp_slices(msg->sender_ssid, msg->ssid_len, client->parent_conn->ssid, client->parent_conn->ssid_len)) {

		memcpy(client->network_id, msg->network_id, 4);

		struct msg* message = (struct msg*)malloc(sizeof(struct msg));
		message->addr = client->parent_conn->cliaddr;
		message->msg = make_recv_answ(client, msg->msg_id);
		if (xQueueSendToBack(client->writing_queue, message, 2000) == pdTRUE) {
			printf("sent recv answ\n");
		} else {
			free(message->msg);
		}
		free(message);

		printf("updating net_id from upd msg\n");
		for (int i = 0; i < MAX_AP_CONN; i++) {
			if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED) {
				uint8_t* msg_id = (uint8_t*)malloc(4 * sizeof(uint8_t));
				rand_bytes(msg_id, 4);
				struct msg* message = (struct msg*)malloc(sizeof(struct msg));
				message->addr = client->children_conns[i]->cliaddr;
				memcpy(message->dest_ssid, client->children_conns[i]->ssid, client->children_conns[i]->ssid_len);
				message->ssid_len = client->children_conns[i]->ssid_len;
				message->type = NET_ID_UPD;
				message->msg = make_net_id_update(client, msg_id);
				if (xQueueSendToFront(client->writing_queue, message, 2000) != pdTRUE) {
					free(message->msg);
				}
				hashmap_set(client->critical_msg, msg_id, 4, (uintptr_t)(void*)message);
			}
		}
	}

	xSemaphoreGive(client->conn_mutex);
}

void process_role_upd(struct boromir_client* client, struct message* msg) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);
	printf("processing role upd from %.9s\n", msg->sender_ssid);
	int ind = -1;
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED &&
				cmp_slices(client->children_conns[i]->ssid, client->children_conns[i]->ssid_len,
						msg->sender_ssid, msg->ssid_len)) {
			client->children_conns[i]->roles = msg->roles;
			ind = i;
			break;
		}
	}

	if (ind != -1) {
		struct msg* message = (struct msg*)malloc(sizeof(struct msg));
		message->addr = client->children_conns[ind]->cliaddr;
		message->msg = make_recv_answ(client, msg->msg_id);
		if (xQueueSendToBack(client->writing_queue, message, 2000) == pdTRUE) {
			printf("sent recv answ\n");
		} else {
			free(message->msg);
		}
		free(message);
	} else {
		return;
	}

	uint32_t new_roles = client->roles;
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL) {
			new_roles = new_roles | client->children_conns[i]->roles;
		}
	}
	client->tree_roles = new_roles;

	if (client->parent_conn != NULL && client->parent_conn->status == ESTABLISHED) {
		printf("updating roles\n");
		uint8_t* msg_id = (uint8_t*)malloc(4 * sizeof(uint8_t));
		rand_bytes(msg_id, 4);
		struct msg* message = (struct msg*)malloc(sizeof(struct msg));
		message->addr = client->parent_conn->cliaddr;
		memcpy(message->dest_ssid, client->parent_conn->ssid, client->parent_conn->ssid_len);
		message->ssid_len = client->parent_conn->ssid_len;
		message->type = ROLE_UPD;
		message->msg = make_role_update(client, msg_id);
		if (xQueueSendToFront(client->writing_queue, message, 2000) != pdTRUE) {
			free(message->msg);
		}
		hashmap_set(client->critical_msg, msg_id, 4, (uintptr_t)(void*)message);
	}

	xSemaphoreGive(client->conn_mutex);
}

void process_basic_role_v(struct boromir_client* client, struct message* msg) {
	printf("processing basic role v from %.9s\n", msg->sender_ssid);
	struct handler_data* data = (struct handler_data*)malloc(sizeof(struct handler_data));
	data->usr = client->usr;
	data->event_data = msg->data;
	data->data_len = msg->data_len;
	data->event_handler = client->event_handler;

	char id[5];
	rand_char(id, 4);
	id[4] = '\0';

	if ((client->roles & msg->roles) != 0) {
		xTaskCreate(boromir_event_handler_caller, id, 2048, data, tskIDLE_PRIORITY, NULL);
		return;
	}

	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);

	struct connection** proposals = (struct connection**)malloc((MAX_AP_CONN) * sizeof(struct connection*));
	int ind = 0;
	struct sockaddr_in addr;

	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED &&
				!cmp_slices(client->children_conns[i]->ssid, client->children_conns[i]->ssid_len,
										msg->sender_ssid, msg->ssid_len) &&
				(client->children_conns[i]->roles & msg->roles) != 0) {
			proposals[ind] = client->children_conns[i];
			ind = ind + 1;
		}
	}

	if (ind != 0) {
		int num = rand() % ind;
		addr = proposals[num]->cliaddr;
		printf("chosen for resending: %.9s\n", proposals[num]->ssid);
	} else if (client->parent_conn != NULL &&
			client->parent_conn->status == ESTABLISHED &&
			!cmp_slices(client->parent_conn->ssid, client->parent_conn->ssid_len,
									msg->sender_ssid, msg->ssid_len)) {
		addr = client->parent_conn->cliaddr;
		printf("chosen parent for resending: %.9s\n", client->parent_conn->ssid);
		ind = 1;
	}
	xSemaphoreGive(client->conn_mutex);

	if (ind != 0) {
		struct msg message = {.addr = addr, .msg = make_basic_role_v(msg->sender_ssid, msg->ssid_len, msg->roles, msg->data, msg->data_len)};
		free(msg->data);
		if (xQueueSendToBack(client->writing_queue, &message, 2000) != pdTRUE) {
			free(message.msg);
		} else {
			printf("sent basic message\n");
		}
	} else {
		printf("no proper dest for basic role v\n");
	}

	free(proposals);
}

void send_msg(struct boromir_client* client, uint8_t* data, uint8_t data_len, uint32_t role, char* dest_name, int dest_name_len) {
	xSemaphoreTake(client->conn_mutex, portMAX_DELAY);

	printf("preparing to send basic\n");

	struct connection** proposals = (struct connection**)malloc((MAX_AP_CONN) * sizeof(struct connection*));
	int ind = 0;
	struct sockaddr_in addr;

	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (client->children_conns[i] != NULL && client->children_conns[i]->status == ESTABLISHED &&
				(client->children_conns[i]->roles & role) != 0) {
			proposals[ind] = client->children_conns[i];
			ind = ind + 1;
		}
	}

	if (ind != 0) {
		int num = rand() % ind;
		addr = proposals[num]->cliaddr;
		printf("chosen: %.9s\n", proposals[num]->ssid);
	} else if (client->parent_conn != NULL && client->parent_conn->status == ESTABLISHED) {
		addr = client->parent_conn->cliaddr;
		printf("chosen parent: %.9s\n", client->parent_conn->ssid);
		ind = 1;
	}
	xSemaphoreGive(client->conn_mutex);

	if (ind != 0) {
		struct msg message = {.addr = addr, .msg = make_basic_role_v(client->client_ssid, client->ssid_len, role, data, data_len)};
		if (xQueueSendToBack(client->writing_queue, &message, 2000) != pdTRUE) {
			free(message.msg);
		} else {
			printf("sent basic message\n");
		}
	} else {
		printf("no proper dest for basic role v\n");
	}

	free(proposals);
}

void set_callback(struct boromir_client* client, boromir_event_handler_t event_handler, void* usr) {
	client->event_handler = event_handler;
	client->usr = usr;
}

void boromir_event_handler_caller(void *pvParameters) {
	struct handler_data* data = (struct handler_data*) pvParameters;
	data->event_handler(data->usr, data->event_data, data->data_len);
	free(data);
	vTaskDelete(NULL);
}
