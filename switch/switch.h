#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pcap.h>
#include <time.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>

// Определение констант
#define TABLE_SIZE 100 // Размер таблицы
#define INTERFACE_LEN 32 // Длина интерфейса
#define MAC_ADDR_LEN 6 // Длина MAC-адреса
#define TIMEOUT 60 // Время ожидания (в секундах)
#define BUFFER_SIZE 256 // Размер буфера

// Структура для хранения конфигурации
struct config {
	int modify_flag; // Флаг модификации
	uint16_t urg_ptr_value; // Значение указателя срочности
};

// Структура для хранения аргументов обработчика пакетов
struct handler_args {
	char interface[BUFFER_SIZE]; // Интерфейс
	pcap_t* handle;
};

// Структура для хранения строки коммутационной таблицы
struct switch_row {
	time_t create_time; // Время создания
	u_char mac[MAC_ADDR_LEN]; // MAC-адрес
	char interface[INTERFACE_LEN]; // Интерфейс
};

// Структура для хранения коммутационной таблицы
struct switch_table {
	struct switch_row* rows; // Строки таблицы
	int size; // Размер таблицы
	int max_size; // Максимальный размер таблицы
	int record_lifetime; // Время жизни записи в таблице
};

// Функция чтения конфигурационного файла
extern int read_config(const char*, struct config*);

// Функция инициализации таблицы коммутации
void init_switch_table(struct switch_table*);

// Функция обновления таблицы коммутации
void update_switch_table(struct switch_table*, const char*, u_char*);

// Функция поиска интерфейса по MAC-адресу
char* get_interface(struct switch_table*, u_char*);

// Функция модификации TCP сегмента
void modify_tcp_segment(struct tcphdr*);

// Функция проверки TCP сегмента
struct tcphdr* check_tcp_segment(const u_char*);

// Обработчик пакетов
void packet_handler(unsigned char*, const struct pcap_pkthdr*, const u_char*);

// Функция запуска pcap_loop в отдельном потоке
void* pcap_loop_thread(void*);
