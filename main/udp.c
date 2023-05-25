#include "udp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/inet.h"
#include <lwip/netdb.h>
#include "config.h"
#include "boromir_client.h"

void udp_sender(void *pvParameters) {
	printf("sender active\n");
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	BaseType_t xStatus;
	struct msg message;
	for(;;) {
		xStatus = xQueueReceive(client->writing_queue, &message, portMAX_DELAY);
		if (xStatus == pdTRUE) {
			sendto(client->sockfd, message.msg, 256,  0, (struct sockaddr*)&message.addr,  sizeof(message.addr));
		}
	}
	vTaskDelete(NULL);
}

void udp_receiver(void *pvParameters) {
	printf("receiver active\n");
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	for(;;) {
		char buf[256];
	    recvfrom(client->sockfd, buf, sizeof(buf), 0, NULL, NULL);
	    printf("recv %.10s\n", buf);
	    //xQueueSendToBack(client->writing_queue, buf, 0);
	}
	shutdown(client->sockfd, 0);
	close(client->sockfd);
	vTaskDelete(NULL);
}
