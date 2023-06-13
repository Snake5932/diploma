#include "boromir.h"
#include "boromir_client.h"
#include "tcpip_adapter.h"

//TODO: был прецедент падения программы при ошибке парсинга

void parse_message(char* msg, uint32_t size, struct message* res) {
	res->err = 0;
	if (msg == NULL || size > 255 || size < 2) {
		res->err = 1;
		return;
	}
	if ((msg[0] & 0x0F) == 0x01) {
		parse_broadcast(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x02) {
		parse_connect(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x03) {
		parse_established(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x04) {
		parse_beacon(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x05) {
		parse_beacon_answ(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x06) {
		parse_role_update(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x07) {
		parse_net_id_update(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x08) {
		parse_recv_answ(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x09) {
		parse_basic_role_v(msg, (uint8_t)size, res);
	} else if ((msg[0] & 0x0F) == 0x0A) {
		parse_basic_ssid_v(msg, (uint8_t)size, res);
	} else {
		res->err = 1;
	}
}

void parse_broadcast(char* msg, uint8_t size, struct message* res) {
	res->type = BROADCAST;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
	uint8_t cur_pos = 3 + res->ssid_len;
	memcpy(res->sender_mac, &msg[cur_pos], 6);
	cur_pos = cur_pos + 7;
	res->order = msg[cur_pos];
	cur_pos = cur_pos + 1;
	memcpy(res->network_id, &msg[cur_pos], 4);
	cur_pos = cur_pos + 4;
	memcpy(&res->ip, &msg[cur_pos], 4);
}

void parse_connect(char* msg, uint8_t size, struct message* res) {
	res->type = CONNECT;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
	uint8_t cur_pos = 3 + res->ssid_len;
	memcpy(res->sender_mac, &msg[cur_pos], 6);
	cur_pos = cur_pos + 6;
	memcpy(&res->roles, &msg[cur_pos], 4);
	cur_pos = cur_pos + 4;
	memcpy(&res->ip, &msg[cur_pos], 4);
}

void parse_established(char* msg, uint8_t size, struct message* res) {
	res->type = ESTABLISHING;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
	uint8_t cur_pos = 3 + res->ssid_len;
	memcpy(res->network_id, &msg[cur_pos], 4);
}

void parse_beacon(char* msg, uint8_t size, struct message* res) {
	res->type = BEACON;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
}

void parse_beacon_answ(char* msg, uint8_t size, struct message* res) {
	res->type = BEACON_ANSW;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
}

void parse_role_update(char* msg, uint8_t size, struct message* res) {
	res->type = ROLE_UPD;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	memcpy(res->msg_id, &msg[2], 4);
	res->ssid_len = msg[6];
	memcpy(res->sender_ssid, &msg[7], res->ssid_len);
	uint8_t cur_pos = 7 + res->ssid_len;
	memcpy(&res->roles, &msg[cur_pos], 4);
}

void parse_net_id_update(char* msg, uint8_t size, struct message* res) {
	res->type = NET_ID_UPD;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	memcpy(res->msg_id, &msg[2], 4);
	res->ssid_len = msg[6];
	memcpy(res->sender_ssid, &msg[7], res->ssid_len);
	uint8_t cur_pos = 7 + res->ssid_len;
	memcpy(res->network_id, &msg[cur_pos], 4);
}

void parse_recv_answ(char* msg, uint8_t size, struct message* res) {
	res->type = RECV_ANSW;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	memcpy(res->msg_id, &msg[2], 4);
	res->ssid_len = msg[6];
	memcpy(res->sender_ssid, &msg[7], res->ssid_len);
}

void parse_basic_role_v(char* msg, uint8_t size, struct message* res) {
	res->type = BASIC_ROLE_V;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
	uint8_t cur_pos = 3 + res->ssid_len;
	res->init_ssid_len = msg[cur_pos];
	memcpy(res->init_sender_ssid, &msg[cur_pos + 1], res->init_ssid_len);
	cur_pos = cur_pos + 1 + res->init_ssid_len;
	memcpy(&res->roles, &msg[cur_pos], 4);
	cur_pos = cur_pos + 4;
	res->data_len = msg[cur_pos];
	res->data = (uint8_t*)malloc(res->data_len * sizeof(uint8_t));
	memcpy(res->data, &msg[cur_pos + 1], res->data_len);
}

void parse_basic_ssid_v(char* msg, uint8_t size, struct message* res) {
	res->type = BASIC_ROLE_V;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	if (size != msg[1]) {
		res->err = 1;
		return;
	}
	res->ssid_len = msg[2];
	memcpy(res->sender_ssid, &msg[3], res->ssid_len);
	uint8_t cur_pos = 3 + res->ssid_len;
	res->init_ssid_len = msg[cur_pos];
	memcpy(res->init_sender_ssid, &msg[cur_pos + 1], res->init_ssid_len);
	cur_pos = cur_pos + 1 + res->init_ssid_len;
	res->dest_ssid_len = msg[cur_pos];
	memcpy(res->dest_ssid, &msg[cur_pos + 1], res->dest_ssid_len);
	cur_pos = cur_pos + 1 + res->dest_ssid_len;
	res->data_len = msg[cur_pos];
	res->data = (uint8_t*)malloc(res->data_len * sizeof(uint8_t));
	memcpy(res->data, &msg[cur_pos + 1], res->data_len);
}

//сообщение рассылаемое сервером при подключении к нему клиента
char* make_broadcast(struct boromir_client* client) {
	uint32_t size = 3 + client->ssid_len + 16;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x01 - broadcast, 6 бит ставим в 1, это значит, что на сообщение требуется обязательный ответ
	msg[0] = 0x01 | 1 << 6;
	msg[1] = (uint8_t)size;
	//длина и ssid отправителя
	msg[2] = client->ssid_len;
	memcpy(&msg[3], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 3 + client->ssid_len;
	memcpy(&msg[cur_pos], client->station_mac, 6);
	cur_pos = cur_pos + 6;
	//длина и порядок отправителя, я использую отдельную переменную в один байт, но возможны и другие варианты
	msg[cur_pos] = 0x01;
	memcpy(&msg[cur_pos + 1], &client->order, 1);
	cur_pos = cur_pos + 2;
	//id предлагаемой сети
	memcpy(&msg[cur_pos], client->network_id, 4);
	cur_pos = cur_pos + 4;
	tcpip_adapter_ip_info_t ip_info;
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
	memcpy(&msg[cur_pos], &ip_info.ip.addr, 4);
	return msg;
}

//сообщение - ответ на broadcast
char* make_connect(struct boromir_client* client) {
	uint32_t size = 3 + client->ssid_len + 14;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x02 - connect
	msg[0] = 0x02 | 1 << 6;
	msg[1] = (uint8_t)size;
	//длина и ssid отправителя
	msg[2] = client->ssid_len;
	memcpy(&msg[3], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 3 + client->ssid_len;
	//mac, чтобы ap мог определить кто пишет
	memcpy(&msg[cur_pos], client->station_mac, 6);
	cur_pos = cur_pos + 6;
	//копируем 4 байта ролей
	memcpy(&msg[cur_pos], &client->tree_roles, 4);
	cur_pos = cur_pos + 4;
	tcpip_adapter_ip_info_t ip_info;
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
	memcpy(&msg[cur_pos], &ip_info.ip.addr, 4);
	return msg;
}

//подтверждение установки соединения от сервера
char* make_established(struct boromir_client* client) {
	uint32_t size = 3 + client->ssid_len + 4;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x03 - established, без бита ответа, так как он не нужен
	msg[0] = 0x03;
	msg[1] = (uint8_t)size;
	//длина и ssid отправителя
	msg[2] = client->ssid_len;
	memcpy(&msg[3], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 3 + client->ssid_len;
	//id предлагаемой сети
	memcpy(&msg[cur_pos], client->network_id, 4);
	return msg;
}

//сообщение поддержки соединения, рассылаются точками доступа, таким образом клиенты узнают о доступности сервера
//в свою очередь сервер делает вывод по наличию ответа
char* make_beacon(struct boromir_client* client) {
	uint32_t size = 3 + client->ssid_len;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x04 - beacon
	msg[0] = 0x04 | 1 << 6;
	msg[1] = (uint8_t)size;
	//длина и ssid отправителя
	msg[2] = client->ssid_len;
	memcpy(&msg[3], client->client_ssid, client->ssid_len);
	return msg;
}

char* make_beacon_answ(struct boromir_client* client) {
	uint32_t size = 3 + client->ssid_len;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x05 - ответ на beacon
	msg[0] = 0x05;
	msg[1] = (uint8_t)size;
	//длина и ssid отправителя
	msg[2] = client->ssid_len;
	memcpy(&msg[3], &client->client_ssid, client->ssid_len);
	return msg;
}

char* make_role_update(struct boromir_client* client, uint8_t msg_id[4]) {
	uint32_t size = 7 + client->ssid_len + 4;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x06 - обновление доступных ролей
	msg[0] = 0x06 | 1 << 6;
	msg[1] = (uint8_t)size;
	//id сообщения для определения ответа
	memcpy(&msg[2], msg_id, 4);
	//длина и ssid отправителя
	msg[6] = client->ssid_len;
	memcpy(&msg[7], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 7 + client->ssid_len;
	memcpy(&msg[cur_pos], &client->tree_roles, 4);
	return msg;
}

char* make_net_id_update(struct boromir_client* client, uint8_t msg_id[4]) {
	uint32_t size = 7 + client->ssid_len + 4;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x07 - обновление id сети
	msg[0] = 0x07 | 1 << 6;
	msg[1] = (uint8_t)size;
	//id сообщения для определения ответа
	memcpy(&msg[2], msg_id, 4);
	//длина и ssid отправителя
	msg[6] = client->ssid_len;
	memcpy(&msg[7], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 7 + client->ssid_len;
	memcpy(&msg[cur_pos], client->network_id, 4);
	return msg;
}

char* make_recv_answ(struct boromir_client* client, uint8_t msg_id[4]) {
	uint32_t size = 7 + client->ssid_len;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x08 - универсальный ответ
	msg[0] = 0x08;
	msg[1] = (uint8_t)size;
	memcpy(&msg[2], msg_id, 4);
	//длина и ssid отправителя
	msg[6] = client->ssid_len;
	memcpy(&msg[7], &client->client_ssid, client->ssid_len);
	return msg;
}

char* make_basic_role_v(uint8_t* sender_ssid, uint8_t ssid_len, uint8_t* init_sender_ssid, uint8_t init_ssid_len, uint32_t dest_role, uint8_t* data, uint8_t data_len) {
	uint32_t size = 3 + ssid_len + 1 + init_ssid_len + 5 + data_len;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	//тип 0x09 - универсальный ответ
	msg[0] = 0x09;
	msg[1] = (uint8_t)size;
	//длина и ssid отправителя
	msg[2] = ssid_len;
	memcpy(&msg[3], sender_ssid, ssid_len);
	uint8_t cur_pos = 3 + ssid_len;
	memcpy(&msg[3], init_sender_ssid, init_ssid_len);
	uint8_t cur_pos = 1 + init_ssid_len;
	memcpy(&msg[cur_pos], &dest_role, 4);
	cur_pos = cur_pos + 4;
	msg[cur_pos] = data_len;
	memcpy(&msg[cur_pos + 1], data, data_len);
	return msg;
}

char* make_basic_ssid_v(uint8_t* sender_ssid, uint8_t ssid_len, uint8_t* init_sender_ssid, uint8_t init_ssid_len, uint8_t* dest_ssid, uint8_t dest_ssid_len, uint8_t* data, uint8_t data_len) {
	uint32_t size = 3 + ssid_len + 1 + init_ssid_len + 1 + dest_ssid_len + 1 + data_len;
	if (size > 255) {
		return NULL;
	}
	char* msg = (char*)malloc(size * sizeof(char));
	msg[0] = 0x0A;
	msg[1] = (uint8_t)size;
	msg[2] = ssid_len;
	memcpy(&msg[3], sender_ssid, ssid_len);
	uint8_t cur_pos = 3 + ssid_len;
	memcpy(&msg[3], init_sender_ssid, init_ssid_len);
	uint8_t cur_pos = 1 + init_ssid_len;
	memcpy(&msg[cur_pos], &dest_ssid, dest_ssid_len);
	cur_pos = cur_pos + 1 + dest_ssid_len;
	msg[cur_pos] = data_len;
	memcpy(&msg[cur_pos + 1], data, data_len);
	return msg;
}
