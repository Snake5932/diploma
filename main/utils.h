#ifndef UTILS
#define UTILS

#include "stdint.h"

struct bad_conn {
	uint8_t ssid[32];
	uint8_t ssid_len;
	uint32_t timestamp;
};

char is_bad_conn(uint8_t ssid[33], void** bad_conns, int bad_conn_num);

void rand_bytes(uint8_t* buf, unsigned int len);

#endif
