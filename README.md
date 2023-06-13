# Diploma

Для запуска потребуется:
* компьютер под управлением Ubuntu
* toolchain и SDK для ESP8266
* кабель USB-miniUSB
* один микроконтроллер ESP8266 для проверки прошивки, и два или более микроконтроллеров для проверки работы сети

## Инструкция по настройке среды для запуска и прошивке

### Установка toolchain

Перед установкой необходимо выполнить следующую команду:

```bash
sudo apt-get install gcc git wget make libncurses-dev flex bison gperf python python-serial
```

Отметим, что здесь и далее используется Python2.7, а не Python3.

Далее необходимо скачать сам toolchain, [для 64 битной](https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz), [для 32 битной](https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-i686.tar.gz).

Архив необходимо распаковать в папку ~/esp.

Далее обновим переменную PATH:

```bash
export PATH="$PATH:$HOME/esp/xtensa-lx106-elf/bin"
```

Также необходимо получить права на использование порта USB, сделать это можно двумя способами:

```bash
sudo usermod -a -G dialout $USER
```
или (пример для /dev/ttyUSB0)

```bash
sudo chmod -R 777 /dev/ttyUSB0
```

### Установка SDK

Чтобы установить SDK выполним следующие команды:

```bash
cd ~/esp
git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
```

Далее необходимо определить переменную окружения для SDK, назовем ее IDF_PATH, она должна указывать на ~/esp/ESP8266_RTOS_SDK

Установим необходимые пакеты для сборки:

```bash
python -m pip install --user -r $IDF_PATH/requirements.txt
```

### Прошивка

Чтобы прошить микроконтроллер, необходимо папку с проектом перенести в директорию ~/esp.

Далее, перейдя в папку проекта, необходимо выполнить команду make menuconfig и перейти к Serial flasher config > Default serial port.
Там необходимо вписать нужный порт, сохранить и выйти.

После этого можно прошить микроконтроллер командой make flash.

С помощью команды make monitor можно следить за работой микроконтроллера из консоли.

Команды выше можно объединить с помощью make flash monitor.

## Инструкция к функциям клиента

### Конфигурация

Сначала необходимо настроить конфигурацию клиента. Для этого в файле config.h должны быть определены следующие параметры:

```с
#define BROADCAST_IP "192.168.2.255"
#define AP_IP "192.168.2.10"
#define NETMASK "255.255.255.0"
#define SSID "boromir-1"
#define SSID_PREFIX "boromir"
#define WIFI_PASS "111111111"
#define MAX_AP_CONN 1
#define PORT 5555
#define ORDER 10
#define RAND_SEED 234534862
```

* Значения PORT, WIFI_PASS, SSID_PREFIX должны быть одинаковыми для всех микроконтроллеров.
* AP_IP - адрес точки доступа данного микроконтроллера, NETMASK определяет маску, с помощью чего можно вычислить адрес сети,
адрес сети должен быть уникальным для каждого микроконтроллера. Соответсвенно BROADCAST_IP - широковещательный адрес в этой сети.
* SSID - определяет SSID точки доступа, оно же ID узла.
* ORDER - порядок узла в сети, уникален для каждого узла.
* RAND_SEED - случайное число для инициализации генератора случайных чисел, уникальное для каждого узла.
* MAX_AP_CONN - максимальное число клиентов точки доступа, от 1 до 4.

### Функции клиента

Для работы с клиентом используются следующие функции:

```c
struct boromir_client* new_boromir_client(uint32_t roles);

void start_client(struct boromir_client* client);

void send_msg(struct boromir_client* client, uint8_t* data, uint8_t data_len, uint32_t role, uint8_t* dest_name, uint8_t dest_name_len);

void set_callback(struct boromir_client* client, boromir_event_handler_t event_handler, void* usr);

void free_boromir_client(struct boromir_client* client);
```

где boromir_event_handler определяется как:

```c
typedef void (*boromir_event_handler_t)(void* usr, void* event_data, uint8_t data_len);
```

* функция new_boromir_client принимает на вход список ролей данного клиента и возвращает указатель на экземпляр клиента
* функция set_callback позволяет определить обработчик входящих сообщений, она принимает на вход указатель на клиент,
функцию-обработчик и указатель на пользовательские данные
* функция start_client запускает работы клиента
* функция send_msg позволяет отправить сообщение, на вход она принимает указатель на клиент, данные для передачи и их длину,
а также роль получателя или его ID, при этом роль имеет приоритет перед ID, и для его использования значение роли должно быть равно нулю.
Также должна передаваться лишь одна роль, в случае если в переменной роли в 1 установлены несколько бит, сообщение может не дойти до получателя
* функция free_boromir_client позволяет освободить память

Пример использования клиента есть в файле main.c.
