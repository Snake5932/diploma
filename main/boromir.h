#ifndef BOROMIR
#define BOROMIR

#include "boromir_client.h"

void parse_message(char* msg, struct message* res);

void parse_broadcast(char* msg, struct message* res);

void parse_connect(char* msg, struct message* res);

void parse_established(char* msg, struct message* res);

void parse_beacon(char* msg, struct message* res);

void parse_beacon_answ(char* msg, struct message* res);

char* make_broadcast(struct boromir_client* client);

char* make_connect(struct boromir_client* client);

char* make_established(struct boromir_client* client);

char* make_beacon(struct boromir_client* client);

char* make_beacon_answ(struct boromir_client* client);

#endif
