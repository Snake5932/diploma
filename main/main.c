#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "boromir_client.h"
#include "wifi.h"

void handler(void* usr, void* event_data, uint8_t data_len) {
	char* data = (char*)event_data;
	for (int i = 0; i < data_len; i++) {
		printf("%c", data[i]);
	}
	printf("\n");
	free(data);
}

void app_main() {
    struct boromir_client* client = new_boromir_client(1);
    start_client(client);
    set_callback(client, handler, NULL);
    for(;;) {
    	send_msg(client, (uint8_t[]){'f', 'r', 'o', 'm', ' ', '1', ' ', 't', 'o', ' ', 'b', 'i', 't', ' ', '5'}, 15, 1 << 5, NULL, 0);
    	vTaskDelay(2000);
    }
}
