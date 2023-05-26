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
#include "boromir.h"

void udp_sender(void *pvParameters) {
	printf("sender active\n");
	struct boromir_client* client = (struct boromir_client*) pvParameters;
	BaseType_t xStatus;
	struct msg message;
	for(;;) {
		xStatus = xQueueReceive(client->writing_queue, &message, portMAX_DELAY);
		if (xStatus == pdTRUE) {
			sendto(client->sockfd, message.msg, 256,  0, (struct sockaddr*)&message.addr,  sizeof(message.addr));
			free(message.msg);
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
	    struct message msg;
	    parse_message(buf, &msg);
	    if (!msg.err) {
	    	//TODO: подумать о том, чтобы создавать отдельные таски
	    	if (msg.type == BROADCAST) {
	    		process_broadcast(client, &msg);
	    	} else if (msg.type == CONNECT) {
	    		process_connect(client, &msg);
	    	} else if (msg.type == ESTABLISHING) {
	    		process_establishing(client, &msg);
	    	} else if (msg.type == BEACON) {
	    		process_beacon(client, &msg);
	    	} else if (msg.type == BEACON_ANSW) {
	    		process_beacon_answ(client, &msg);
	    	}
	    }

	}
	shutdown(client->sockfd, 0);
	close(client->sockfd);
	vTaskDelete(NULL);
}
