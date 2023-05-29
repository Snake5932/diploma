#include "utils.h"
#include <stdlib.h>
#include "stdint.h"
#include "string.h"
#include "config.h"

void rand_bytes(uint8_t* buf, unsigned int len) {
	for (int i = 0; i < len; i++) {
		buf[i] = (uint8_t)rand();
	}
}

void rand_char(char* buf, unsigned int len) {
	for (int i = 0; i < len; i++) {
		buf[i] = (rand() % (122 - 97 + 1)) + 97;
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
		if (s1[i] != s2[i]) {
			return 0;
		}
	}

	return 1;
}

//-1 первый меньше второго, 1 первый больше второго, 0 равны
char cmp_order(uint8_t* o1, uint8_t len1, uint8_t* o2, uint8_t len2) {
	if (len1 < len2) {
		return -1;
	}
	if (len1 > len2) {
		return 1;
	}

	for (int i = 0; i < len1; i++) {
		if (o1[i] < o2[i]) {
			return -1;
		}
		if (o1[i] > o2[i]) {
			return 1;
		}
	}

	return 0;
}

char has_conn(struct connection** conns, uint8_t* mac) {
	for (int i = 0; i < MAX_AP_CONN; i++) {
		if (conns[i] != NULL) {
			if (cmp_mac(conns[i]->mac, mac)) {
				return 1;
			}
		}
	}
	return 0;
}


void free_map_entry(void* key, size_t ksize, uintptr_t value, void* usr)
{
	free((void*)value);
	free(key);
}
