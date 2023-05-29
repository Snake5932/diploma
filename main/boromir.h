#ifndef BOROMIR
#define BOROMIR

#include "boromir_client.h"

void parse_message(char* msg, uint32_t size, struct message* res);

void parse_broadcast(char* msg, uint8_t size, struct message* res);

void parse_connect(char* msg, uint8_t size, struct message* res);

void parse_established(char* msg, uint8_t size, struct message* res);

void parse_beacon(char* msg, uint8_t size, struct message* res);

void parse_beacon_answ(char* msg, uint8_t size, struct message* res);

void parse_role_update(char* msg, uint8_t size, struct message* res);

void parse_net_id_update(char* msg, uint8_t size, struct message* res);

void parse_net_id_update(char* msg, uint8_t size, struct message* res);

void parse_recv_answ(char* msg, uint8_t size, struct message* res);

void parse_basic_role_v(char* msg, uint8_t size, struct message* res);

char* make_broadcast(struct boromir_client* client);

char* make_connect(struct boromir_client* client);

char* make_established(struct boromir_client* client);

char* make_beacon(struct boromir_client* client);

char* make_beacon_answ(struct boromir_client* client);

char* make_role_update(struct boromir_client* client, uint8_t msg_id[4]);

char* make_net_id_update(struct boromir_client* client, uint8_t msg_id[4]);

char* make_recv_answ(struct boromir_client* client, uint8_t msg_id[4]);

char* make_basic_role_v(uint8_t* dest_ssid, uint8_t ssid_len, uint32_t dest_role, uint8_t* data, uint8_t data_len);

#endif
