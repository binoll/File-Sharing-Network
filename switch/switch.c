#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct tcphdr* check_tcp_segment(const u_char* packet) {
	const struct ether_header* eth_header = (struct ether_header*) packet;
	int ethernet_header_length = sizeof(struct ether_header);

	if (ntohs(eth_header->ether_type) == ETHERTYPE_IP) {
		const struct ip* ip_header = (struct ip*) (packet + ethernet_header_length);
		int ip_header_length = ip_header->ip_hl * 4;

		if (ip_header->ip_p == IPPROTO_TCP) {
			return (struct tcphdr*) (packet + ethernet_header_length + ip_header_length);
		}
	}
	return NULL;
}

void modify_tcp_segment(const u_char* packet) {
	struct tcphdr* tcp_header = check_tcp_segment(packet);
	if (tcp_header != NULL) {
		tcp_header->th_flags = tcp_header->th_flags + 1;
	}
}

void* tcp_segment_before(void* interface) {
	int* result = NULL;
	char buffer[PCAP_ERRBUF_SIZE];
	pcap_t* handle;
	const u_char* packet;
	struct pcap_pkthdr header;
	const char* iface = (const char*) interface;

	handle = pcap_open_live(iface, BUFSIZ, 1, 1000, buffer);
	if (handle == NULL) {
		fprintf(stdout, "Could not open device: %s", iface);
		*result = -1;
		return result;
	}

	while (true) {
		packet = pcap_next(handle, &header);
		if (packet == NULL) {
			continue;
		}

		struct tcphdr* tcp_header = check_tcp_segment(packet);
		if (tcp_header != NULL) {
			fprintf(stdout, "TCP segment flag before modify: %i", tcp_header->th_flags);
		}
	}
}

void* tcp_segment_after(void* interface) {
	int* result = NULL;
	char buffer[PCAP_ERRBUF_SIZE];
	pcap_t* handle;
	const u_char* packet;
	struct pcap_pkthdr header;
	const char* iface = (const char*) interface;

	handle = pcap_open_live(iface, BUFSIZ, 1, 1000, buffer);
	if (handle == NULL) {
		fprintf(stdout, "Could not open device: %s", iface);
		*result = -1;
		return result;
	}

	while (true) {
		packet = pcap_next(handle, &header);
		if (packet == NULL) {
			continue;
		}

		struct tcphdr* tcp_header = check_tcp_segment(packet);
		if (tcp_header != NULL) {
			fprintf(stdout, "TCP segment flag after modify: %i", tcp_header->th_flags);
		}
	}
}

int main(int argc, char* argv[]) {
	char buffer[PCAP_ERRBUF_SIZE];
	pcap_t* handle = NULL;
	const u_char* packet = NULL;
	char* interface_before_modify = NULL;
	char* interface_modify = NULL;
	char* interface_after_modify = NULL;
	struct pcap_pkthdr* header;
	pthread_t thread_before;
	pthread_t thread_after;

	if (argc != 4) {
		fprintf(stdout, "Usage: %s <interface_before_modify> <interface_modify> <interface_after_modify>\n", argv[0]);
		return EXIT_FAILURE;
	}

	strcpy(interface_before_modify, argv[1]);
	strcpy(interface_modify, argv[2]);
	strcpy(interface_after_modify, argv[3]);

	handle = pcap_open_live(interface_modify, BUFSIZ, 1, 1000, buffer);
	if (handle == NULL) {
		fprintf(stdout, "Could not open device: %s", interface_modify);
		return -1;
	}

	if (pthread_create(&thread_before, NULL, tcp_segment_before, interface_before_modify) != 0) {
		fprintf(stdout, "Error creating thread_before\n");
		return -1;
	}
	if (pthread_create(&thread_after, NULL, tcp_segment_after, interface_after_modify) != 0) {
		fprintf(stdout, "Error creating thread_after\n");
		return -1;
	}
	pthread_detach(thread_before);
	pthread_detach(thread_before);

	while (true) {
		packet = pcap_next(handle, header);

		if (packet == NULL) {
			continue;
		}
		modify_tcp_segment(packet);
	}
	pcap_close(handle);
	return 0;
}