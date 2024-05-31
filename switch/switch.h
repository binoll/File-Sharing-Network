#pragma once

#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 256
#define TABLE_SIZE 256
#define TIMEOUT 60

struct mac_entry {
	u_char mac[ETHER_ADDR_LEN];
	char interface[BUFFER_SIZE];
	time_t timestamp;
};

struct config {
	int modify_flag;
	uint16_t urg_ptr_value;
};

struct handler_args {
	char interface1[BUFFER_SIZE];
	char interface2[BUFFER_SIZE];
	struct config cfg;
};

struct mac_entry mac_table[TABLE_SIZE];

extern void read_config(const char*, struct config*);
