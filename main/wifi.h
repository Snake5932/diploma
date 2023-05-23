#ifndef WIFI
#define WIFI
#include "esp_event.h"
#include "esp_system.h"
#include "boromir_client.h"

esp_err_t wifi_init(struct boromir_client* client);

void connect_to_ap(void *pvParameters);

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#endif
