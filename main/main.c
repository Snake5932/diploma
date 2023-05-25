#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "boromir_client.h"
#include "wifi.h"


void app_main() {
    struct boromir_client* client = new_boromir_client(1);
    start_client(client);
}
