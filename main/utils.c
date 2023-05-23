#include "utils.h"
#include <stdlib.h>
#include "stdint.h"
#include "string.h"

void rand_bytes(uint8_t* buf, unsigned int len) {
	for (int i = 0; i < len; i++) {
		buf[i] = (uint8_t)rand();
	}
}

char is_bad_conn(uint8_t ssid[33], void** bad_conns, int bad_conn_num) {
	for (int i = 0; i < bad_conn_num; i++) {
		struct bad_conn* bc = (struct bad_conn*)bad_conns[i];
		if (strlen((char*)ssid) == bc->ssid_len && strncmp((char*)ssid, (char*)bc->ssid, bc->ssid_len) == 0) {
			return 1;
		}
	}
	return 0;
}
