#ifndef UTILS
#define UTILS

#include "stdint.h"
#include "boromir_client.h"

struct bad_conn {
	uint8_t ssid[32];
	uint8_t ssid_len;
	uint32_t timestamp;
};

char is_bad_conn(uint8_t ssid[33], void** bad_conns, int bad_conn_num);

void rand_bytes(uint8_t* buf, unsigned int len);

char cmp_mac(uint8_t *mac1, uint8_t* mac2);

char cmp_slices(uint8_t *s1, uint8_t len1, uint8_t* s2, uint8_t len2);

char cmp_order(uint8_t *o1, uint8_t len1, uint8_t* o2, uint8_t len2);

char has_conn(struct connection** conns, uint8_t* mac);

#endif
