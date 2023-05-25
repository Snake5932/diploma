#ifndef BOROMIR
#define BOROMIR

#include "boromir_client.h"

char* make_broadcast(struct boromir_client* client);

char* make_connect(struct boromir_client* client);

char* make_established(struct boromir_client* client);

char* make_beacon(struct boromir_client* client);

char* make_beacon_answ(struct boromir_client* client);

#endif
