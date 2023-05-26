#include "boromir.h"
#include "boromir_client.h"
#include "tcpip_adapter.h"

void parse_message(char* msg, struct message* res) {
	res->err = 0;
	if ((msg[0] & 0x0F) == 0x01) {
		parse_broadcast(msg, res);
	} else if ((msg[0] & 0x0F) == 0x02) {
		parse_connect(msg, res);
	} else if ((msg[0] & 0x0F) == 0x03) {
		parse_established(msg, res);
	} else if ((msg[0] & 0x0F) == 0x04) {
		parse_beacon(msg, res);
	} else if ((msg[0] & 0x0F) == 0x05) {
		parse_beacon_answ(msg, res);
	} else {
		res->err = 1;
	}
}

void parse_broadcast(char* msg, struct message* res) {
	res->type = BROADCAST;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	res->ssid_len = msg[1];
	memcpy(res->sender_ssid, &msg[2], res->ssid_len);
	uint8_t cur_pos = 2 + res->ssid_len;
	memcpy(res->sender_mac, &msg[cur_pos], 6);
	cur_pos = cur_pos + 7;
	res->order = msg[cur_pos];
	cur_pos = cur_pos + 1;
	memcpy(res->network_id, &msg[cur_pos], 4);
	cur_pos = cur_pos + 4;
	memcpy(&res->ip, &msg[cur_pos], 4);
}

void parse_connect(char* msg, struct message* res) {
	res->type = CONNECT;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	res->ssid_len = msg[1];
	memcpy(res->sender_ssid, &msg[2], res->ssid_len);
	uint8_t cur_pos = 2 + res->ssid_len;
	memcpy(res->sender_mac, &msg[cur_pos], 6);
	cur_pos = cur_pos + 8;
	memcpy(&res->roles, &msg[cur_pos], 4);
	cur_pos = cur_pos + 4;
	memcpy(&res->ip, &msg[cur_pos], 4);
}

void parse_established(char* msg, struct message* res) {
	res->type = ESTABLISHING;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	res->ssid_len = msg[1];
	memcpy(res->sender_ssid, &msg[2], res->ssid_len);
	uint8_t cur_pos = 2 + res->ssid_len;
	memcpy(res->network_id, &msg[cur_pos], 4);
}

void parse_beacon(char* msg, struct message* res) {
	res->type = BEACON;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	res->ssid_len = msg[1];
	memcpy(res->sender_ssid, &msg[2], res->ssid_len);
}

void parse_beacon_answ(char* msg, struct message* res) {
	res->type = BEACON_ANSW;
	if ((msg[0] & (1 << 6)) != 0) {
		res->must_be_answered = 1;
	} else {
		res->must_be_answered = 0;
	}
	res->ssid_len = msg[1];
	memcpy(res->sender_ssid, &msg[2], res->ssid_len);
}

//сообщение рассылаемое сервером при подключении к нему клиента
char* make_broadcast(struct boromir_client* client) {
	char* msg = (char*)malloc(256 * sizeof(char));
	memset(msg, 0, 256);
	//тип 0x01 - broadcast, 6 бит ставим в 1, это значит, что на сообщение требуется обязательный ответ
	msg[0] = 0x01 | 1 << 6;
	//длина и ssid отправителя
	msg[1] = client->ssid_len;
	memcpy(&msg[2], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 2 + client->ssid_len;
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
	//остальные байты не используются, но все сообщения для простоты будут фиксированного размера
	return msg;
}

//сообщение - ответ на broadcast
char* make_connect(struct boromir_client* client) {
	char* msg = (char*)malloc(256 * sizeof(char));
	memset(msg, 0, 256);
	//тип 0x02 - connect
	msg[0] = 0x02 | 1 << 6;
	//длина и ssid отправителя
	msg[1] = client->ssid_len;
	memcpy(&msg[2], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 2 + client->ssid_len;
	//mac, чтобы ap мог определить кто пишет
	memcpy(&msg[cur_pos], client->station_mac, 6);
	cur_pos = cur_pos + 6;
	//длина и порядок отправителя, я использую отдельную переменную в один байт, но возможны и другие варианты
	msg[cur_pos] = 1;
	memcpy(&msg[cur_pos + 1], &client->order, 1);
	cur_pos = cur_pos + 2;
	//копируем 4 байта ролей
	memcpy(&msg[cur_pos], &client->roles, 4);
	cur_pos = cur_pos + 4;
	tcpip_adapter_ip_info_t ip_info;
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
	memcpy(&msg[cur_pos], &ip_info.ip.addr, 4);
	printf("%u\n", ip_info.ip.addr);
	//остальные байты не используются
	return msg;
}

//подтверждение установки соединения от сервера
char* make_established(struct boromir_client* client) {
	char* msg = (char*)malloc(256 * sizeof(char));
	memset(msg, 0, 256);
	//тип 0x03 - established, без бита ответа, так как он не нужен
	msg[0] = 0x03;
	//длина и ssid отправителя
	msg[1] = client->ssid_len;
	memcpy(&msg[2], client->client_ssid, client->ssid_len);
	uint8_t cur_pos = 2 + client->ssid_len;
	//id предлагаемой сети
	memcpy(&msg[cur_pos], client->network_id, 4);
	//больше ничего не нужно, так как стороны уже получили всю необходимую информацию
	return msg;
}

//сообщение поддержки соединения, рассылаются точками доступа, таким образом клиенты узнают о доступности сервера
//в свою очередь сервер делает вывод по наличию ответа
char* make_beacon(struct boromir_client* client) {
	char* msg = (char*)malloc(256 * sizeof(char));
	memset(msg, 0, 256);
	//тип 0x04 - beacon
	msg[0] = 0x04 | 1 << 6;
	//длина и ssid отправителя
	msg[1] = client->ssid_len;
	memcpy(&msg[2], client->client_ssid, client->ssid_len);
	return msg;
}

char* make_beacon_answ(struct boromir_client* client) {
	char* msg = (char*)malloc(256 * sizeof(char));
	memset(msg, 0, 256);
	//тип 0x05 - ответ на beacon
	msg[0] = 0x05;
	//длина и ssid отправителя
	msg[1] = client->ssid_len;
	memcpy(&msg[2], &client->client_ssid, client->ssid_len);
	return msg;
}
