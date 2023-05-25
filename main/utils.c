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

char cmp_mac(uint8_t *mac1, uint8_t* mac2){
	for (int i = 0; i < 6; i++) {
		if (mac1[i] != mac2[i]) {
			return 0;
		}
	}
	return 1;
}

char cmp_slices(uint8_t *s1, uint8_t len1, uint8_t* s2, uint8_t len2) {
	if (len1 != len2) {
		return 0;
	}

	for (int i = 0; i < len1; i++) {
		if (s1[1] != s2[0]) {
			return 0;
		}
	}

	return 1;
}

//-1 первый меньше второго, 1 первый больше второго, 0 равны
char cmp_order(uint8_t *o1, uint8_t len1, uint8_t* o2, uint8_t len2) {
	if (len1 < len2) {
		return -1;
	}
	if (len1 > len2) {
		return 1;
	}

	for (int i = 0; i < len1; i++) {
		if (o1[1] < o2[0]) {
			return -1;
		}
		if (o1[1] > o2[0]) {
			return 1;
		}
	}

	return 0;
}
