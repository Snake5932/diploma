#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "boromir.h"
#include "wifi.h"
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "utils.h"
#include "boromir_client.h"

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	struct boromir_client* client = (struct boromir_client*) arg;
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		printf("sta start\n");
		xTaskCreate(connect_to_ap, "wifi_connect", 2048, client, tskIDLE_PRIORITY, NULL);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		printf("sta disconnected\n");

		remove_parent_conn(client);

		wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
		struct bad_conn* bc = (struct bad_conn*)malloc(sizeof(struct bad_conn));
		strncpy((char *)bc->ssid, (char *)event->ssid,
					sizeof(bc->ssid) / sizeof(bc->ssid[0]));
		bc->ssid_len = event->ssid_len;
		bc->timestamp = (uint32_t)xTaskGetTickCount();
		enqueue(client->bad_conns, (void*)bc);
		printf("adding to queue: %.20s\n", (char *)event->ssid);

		xTaskCreate(connect_to_ap, "wifi_connect", 2048, client, tskIDLE_PRIORITY, NULL);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
		printf("ap start\n");
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
		printf("sta connected to ap\n");

		wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
		set_child_conn(client, event->mac, event->aid);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		printf("sta disconnected from ap\n");

		wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
		remove_child_conn(client, event->mac);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
		printf("sta connected\n");

		wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
		set_parent_conn(client, event->ssid, event->ssid_len);
	}
}

void connect_to_ap(void *pvParameters) {
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	//удаляем просроченные
	int bad_conn_len = get_queue_len(client->bad_conns);
	int i = 0;
	while (i < bad_conn_len &&
			(uint32_t)xTaskGetTickCount() - ((struct bad_conn*)peek(client->bad_conns))->timestamp > 2000 ) {
		struct bad_conn* ptr = (struct bad_conn*)dequeue(client->bad_conns);
		printf("deleting from queue as expired: %.20s\n", ptr->ssid);
		free(ptr);
		i += 1;
	}

	struct queue_data qd = get_all(client->bad_conns);

	uint16_t ap_num = 0;
	esp_wifi_scan_start(NULL, true);
	esp_wifi_scan_get_ap_num(&ap_num);
	wifi_ap_record_t* ap_list = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_num);
	esp_wifi_scan_get_ap_records(&ap_num, ap_list);
	int8_t max_signal = -127;
	int max_signal_ind = -1;

	for (int i = 0; i < ap_num; i++) {
		if (!strncmp((char *)ap_list[i].ssid, SSID_PREFIX, strlen(SSID_PREFIX))
				&& !is_bad_conn(ap_list[i].ssid, qd.data, qd.count)
				&& ap_list[i].rssi > max_signal) {
			max_signal = ap_list[i].rssi;
			max_signal_ind = i;
		}
	}

	while (max_signal_ind == -1) {
		printf("no wi-fi found\n");

		free(ap_list);
		ap_num = 0;
		esp_wifi_scan_start(NULL, true);
		esp_wifi_scan_get_ap_num(&ap_num);
		ap_list = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_num);
		esp_wifi_scan_get_ap_records(&ap_num, ap_list);
		max_signal = -127;
		max_signal_ind = -1;

		for (int i = 0; i < ap_num; i++) {
			if (!strncmp((char *)ap_list[i].ssid, SSID_PREFIX, strlen(SSID_PREFIX))
					&& ap_list[i].rssi > max_signal) {
				max_signal = ap_list[i].rssi;
				max_signal_ind = i;
			}
		}
		vTaskDelay(1000);
	}

	wifi_config_t wifi_sta_config = {
		.sta = {
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.password = WIFI_PASS,
			},
		};
	strncpy((char *)wifi_sta_config.sta.ssid, (char *)ap_list[max_signal_ind].ssid,
			sizeof(wifi_sta_config.sta.ssid) / sizeof(wifi_sta_config.sta.ssid[0]));

	printf("connecting: %.20s\n", (char *)ap_list[max_signal_ind].ssid);

	free(ap_list);
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
	esp_wifi_connect();
	vTaskDelete(NULL);
}

esp_err_t wifi_init(struct boromir_client* client) {
	esp_err_t ret = nvs_flash_init();
	ESP_ERROR_CHECK(ret);

	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&event_handler,
		client));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

	tcpip_adapter_ip_info_t ap_ip_info;

	char ip_char[15];
	uint32_t ip;

	snprintf(ip_char, sizeof(ip_char), "%s", AP_IP);
	ip = ipaddr_addr(ip_char);
	memcpy(&ap_ip_info.ip, &ip, 4);
	snprintf(ip_char, sizeof(ip_char), "%s", GW_IP);
	ip = ipaddr_addr(ip_char);
	memcpy(&ap_ip_info.gw, &ip, 4);
	snprintf(ip_char, sizeof(ip_char), "%s", NETMASK);
	ip = ipaddr_addr(ip_char);
	memcpy(&ap_ip_info.netmask, &ip, 4);

	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ap_ip_info));

	wifi_config_t wifi_ap_config = {
		.ap = {
		    .ssid = SSID,
			.ssid_len = strlen(SSID),
		    .password = WIFI_PASS,
		    .authmode = WIFI_AUTH_WPA2_PSK,
		    .max_connection = MAX_AP_CONN,
		    },
		};
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config));

	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_ERROR_CHECK(esp_wifi_set_inactive_time(ESP_IF_WIFI_AP, 20));

	return ESP_OK;
}
