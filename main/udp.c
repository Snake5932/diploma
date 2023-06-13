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

//			for (int i = 0; i < 50; i++) {
//				printf("%x ", message.msg[i]);
//			}
//			printf("\n");

			sendto(client->sockfd, message.msg, (size_t)message.msg[1],  0, (struct sockaddr*)&message.addr,  sizeof(message.addr));
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
	    uint32_t size = recvfrom(client->sockfd, buf, sizeof(buf), 0, NULL, NULL);
	    if (size != -1) {
	    	struct message msg;
	    	parse_message(buf, size, &msg);
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
	    		} else if (msg.type == ROLE_UPD) {
	    			process_role_upd(client, &msg);
	    		} else if (msg.type == NET_ID_UPD) {
	    			process_net_id_upd(client, &msg);
	    		} else if (msg.type == RECV_ANSW) {
	    			process_recv_answ(client, &msg);
	    		} else if (msg.type == BASIC_ROLE_V) {
	    			process_basic_role_v(client, &msg);
	    		} else if (msg.type == BASIC_SSID_V) {
	    			process_basic_ssid_v(client, &msg);
	    		} else {
	    			printf("unknown message type\n");
	    		}
	    	} else {
	    		printf("error while parsing\n");
	    	}
	    } else {
	    	printf("error while reading from socket");
	    }
	}
	shutdown(client->sockfd, 0);
	close(client->sockfd);
	vTaskDelete(NULL);
}
