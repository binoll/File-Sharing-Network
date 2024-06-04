#include "switch.h"

// Объявление таблицы MAC-адресов и мьютекса
struct config cfg;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct switch_table switch_table;
pcap_t* handle1 = NULL;
pcap_t* handle2 = NULL;
char interface1[BUFFER_SIZE];
char interface2[BUFFER_SIZE];

void init_switch_table(struct switch_table* table) {
	table->size = 0;
	table->rows = (struct switch_row*) malloc(table->max_size * sizeof(struct switch_row));
}

void update_switch_table(struct switch_table* table, const char* interface, u_char* mac) {
	pthread_mutex_lock(&mutex);
	int oldest = 0; // Здесь будем храниться индекс самой старой строки

	// ищем соотвествие mac адреса в таблице
	for (int i = 0; i < table->size; ++i) {
		if (memcmp(table->rows[i].mac, mac, MAC_ADDR_LEN * sizeof(u_char)) == 0) {
			strcpy(table->rows[i].interface, interface);
			time(&table->rows[i].create_time);
			pthread_mutex_unlock(&mutex);
			return;
		}

		if (table->rows[i].create_time < table->rows[oldest].create_time)
			oldest = i;
	}

	// Если таблица не заполнена до конца, ставим строку на пустое место
	if (table->size != table->max_size) {
		strcpy(table->rows[table->size].interface, interface);
		memcpy(table->rows[table->size].mac, mac, MAC_ADDR_LEN);
		time(&table->rows[table->size].create_time);
		++table->size;
		pthread_mutex_unlock(&mutex);
		return;
	}

	// Если таблица заполнена до конца, ставим строку вместо самой старой
	strcpy(table->rows[oldest].interface, interface);
	memcpy(table->rows[oldest].mac, mac, MAC_ADDR_LEN);
	time(&table->rows[oldest].create_time);
	pthread_mutex_unlock(&mutex);
}

char* get_interface(struct switch_table* table, u_char* mac) {
	time_t cur_time;
	time(&cur_time);

	for (int i = 0; i < table->size; ++i) {
		time_t diff = cur_time - table->rows[i].create_time;
		if (diff < table->record_lifetime && memcmp(table->rows[i].mac, mac, MAC_ADDR_LEN) == 0) {
			char* interface = (char*) malloc(INTERFACE_LEN);
			interface[0] = '\0';
			strcpy(interface, table->rows[i].interface);
			return interface;
		}
	}

	return NULL;
}

// Пересчет контрольной суммы TCP сегмента
unsigned short tcp_checksum(const void* buf, int len) {
	unsigned long sum;
	for (sum = 0; len > 0; len--) {
		sum += *(unsigned char*) buf++;
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (unsigned short) ~((sum & 0xffff) + (sum >> 16));
}

// Функция модификации TCP сегмента
void modify_tcp_segment(struct tcphdr* tcp_header) {
	if (tcp_header != NULL && cfg.modify_flag) {
		tcp_header->urg = cfg.modify_flag;
		tcp_header->urg_ptr = htons(cfg.urg_ptr_value);

		// Recalculate the TCP checksum
		tcp_header->check = 0;
		tcp_header->check = tcp_checksum(tcp_header, sizeof(struct tcphdr));
	}
}

// Функция проверки TCP сегмента
struct tcphdr* check_tcp_segment(const u_char* packet) {
	const struct ether_header* eth_header = (struct ether_header*) packet;
	int ethernet_header_length = sizeof(struct ether_header);

	// Проверяем, является ли пакет IP-пакетом
	if (ntohs(eth_header->ether_type) == ETHERTYPE_IP) {
		const struct ip* ip_header = (struct ip*) (packet + ethernet_header_length);
		int ip_header_length = ip_header->ip_hl * 4;

		// Проверяем, является ли IP-пакет TCP
		if (ip_header->ip_p == IPPROTO_TCP) {
			return (struct tcphdr*) (packet + ethernet_header_length + ip_header_length);
		}
	}

	return NULL;
}

// Обработчик пакетов
void packet_handler(unsigned char* user, const struct pcap_pkthdr* header, const u_char* packet) {
	struct handler_args* args = (struct handler_args*) user;
	const char* interface = args->interface;
	const char* out_interface = NULL;
	struct ether_header* eth_header = (struct ether_header*) packet;
	pcap_t* handle = NULL;

	// Обновляем таблицу MAC-адресов
	update_switch_table(&switch_table, interface, eth_header->ether_shost);

	// Проверяем TCP сегмент и модифицируем его при необходимости
	struct tcphdr* tcp_header = check_tcp_segment(packet);
	if (tcp_header != NULL) {
		modify_tcp_segment(tcp_header);
	}

	// Выбираем интерфейс для отправки пакета
	out_interface = get_interface(&switch_table, eth_header->ether_dhost);
	if (out_interface != NULL) {
		if (strcmp(out_interface, interface1) == 0) {
			if (pcap_inject(handle1, packet, header->len) == -1) {
				fprintf(stderr, "Error sending packet: %s\n", pcap_geterr(handle));
			}
		} else {
			if (pcap_inject(handle2, packet, header->len) == -1) {
				fprintf(stderr, "Error sending packet: %s\n", pcap_geterr(handle));
			}
		}
	} else {
		if (pcap_inject(handle1, packet, header->len) == -1) {
			fprintf(stderr, "Error sending packet: %s\n", pcap_geterr(handle));
		}
		if (pcap_inject(handle2, packet, header->len) == -1) {
			fprintf(stderr, "Error sending packet: %s\n", pcap_geterr(handle));
		}
	}
}

void* pcap_loop_thread(void* arg) {
	struct handler_args* args = (struct handler_args*) arg;

	pcap_setdirection(args->handle, PCAP_D_IN);

	if (pcap_loop(args->handle, 0, packet_handler, (void*) args) < 0) {
		fprintf(stderr, "Error occurred while processing packets: %s\n", pcap_geterr(args->handle));
	}

	pcap_close(args->handle);

	return NULL;
}

// Главная функция
int main(int argc, char* argv[]) {
	char buffer[PCAP_ERRBUF_SIZE];
	char path_config[BUFFER_SIZE];
	struct handler_args* args1 = NULL;
	struct handler_args* args2 = NULL;
	pthread_t thread1, thread2;

	// Проверяем количество аргументов
	if (argc != 4) {
		fprintf(stdout, "Usage: %s (interface1) (interface2) (init.cfg)\n", argv[0]);
		return EXIT_FAILURE;
	}

	args1 = malloc(sizeof(struct handler_args*));
	args2 = malloc(sizeof(struct handler_args*));

	if (args1 == NULL || args2 == NULL) {
		fprintf(stderr, "Error: Can not allocate memory\n");
		return EXIT_FAILURE;
	}

	// Инициализируем аргументы
	strcpy(args1->interface, argv[1]);
	strcpy(args2->interface, argv[2]);
	strcpy(interface1, argv[1]);
	strcpy(interface2, argv[2]);
	strcpy(path_config, argv[3]);

	// Читаем конфигурационный файл
	if (read_config(path_config, &cfg) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	// Инициализируем таблицу MAC-адресов
	init_switch_table(&switch_table);

	// Открываем первый интерфейс для захвата пакетов
	handle1 = pcap_open_live(args1->interface, BUFSIZ, 1, 1000, buffer);
	if (handle1 == NULL) {
		fprintf(stderr, "Failed to open device %s: %s\n", args1->interface, buffer);
		return EXIT_FAILURE;
	}

	// Открываем второй интерфейс для захвата пакетов
	handle2 = pcap_open_live(args2->interface, BUFSIZ, 1, 1000, buffer);
	if (handle2 == NULL) {
		fprintf(stderr, "Failed to open device %s: %s\n", args2->interface, buffer);
		pcap_close(handle1);
		return EXIT_FAILURE;
	}

	args1->handle = handle1;
	args2->handle = handle2;

	if (pthread_create(&thread1, NULL, pcap_loop_thread, (void*) args1) != 0) {
		fprintf(stderr, "Error creating thread for interface %s\n", args1->interface);
		pcap_close(handle1);
		pcap_close(handle2);
		return EXIT_FAILURE;
	}

	if (pthread_create(&thread2, NULL, pcap_loop_thread, (void*) args2) != 0) {
		fprintf(stderr, "Error creating thread for interface %s\n", args2->interface);
		pcap_close(handle1);
		pcap_close(handle2);
		return EXIT_FAILURE;
	}

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);

	return EXIT_SUCCESS;
}

