#include "boromir.h"
#include "boromir_client.h"

//TODO: не забывать делать free после добавления в очередь

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
	//длина и порядок отправителя, я использую отдельную переменную в один байт, но возможны и другие варианты
	msg[cur_pos] = 1;
	memcpy(&msg[cur_pos + 1], &client->order, 1);
	cur_pos = cur_pos + 2;
	//копируем 4 байта ролей
	memcpy(&msg[cur_pos], &client->roles, 4);
	cur_pos = cur_pos + 4;
	//mac, чтобы ap мог определить кто пишет
	memcpy(&msg[cur_pos], client->station_mac, 6);
	cur_pos = cur_pos + 6;
	tcpip_adapter_ip_info_t ip_info;
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
	memcpy(&msg[cur_pos], &ip_info.ip.addr, 4);
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
