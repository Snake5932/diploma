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

struct sockaddr_in cliaddr;

void udp_sender(void *pvParameters) {
	printf("sender active\n");
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	BaseType_t xStatus;
	char buf[10];
	cliaddr.sin_family    = AF_INET;
	cliaddr.sin_addr.s_addr = inet_addr("192.168.2.11");
	cliaddr.sin_port = htons(PORT);
	for(;;) {
		xStatus = xQueueReceive(client->writing_queue, buf, 10000 /portTICK_RATE_MS);
		if (xStatus == pdTRUE) {
			sendto(client->sockfd, buf, sizeof(buf),  0, (struct sockaddr*) &cliaddr,  sizeof(cliaddr));
			printf("snd %.10s\n", buf);
		}
	}
	vTaskDelete(NULL);
}

void udp_receiver(void *pvParameters) {
	printf("receiver active\n");
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	for(;;) {
		char buf[10];
	    recvfrom(client->sockfd, buf, sizeof(buf), 0, NULL, NULL);
	    printf("recv %.10s\n", buf);
	    //xQueueSendToBack(client->writing_queue, buf, 0);
	}
	shutdown(client->sockfd, 0);
	close(client->sockfd);
	vTaskDelete(NULL);
}
