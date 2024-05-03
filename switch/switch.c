#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256

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

void modify_tcp_segment(struct tcphdr* tcp_header) {
	if (tcp_header != NULL) {
		tcp_header->urg = 1;
		tcp_header->urg_ptr = htons(33333);
	}
}


void tcp_segment_before(struct tcphdr* tcp_header, const char* interface) {
	if (tcp_header != NULL) {
		fprintf(stdout, "TCP segment flag \"URG\" before modify \"%hu\" on interface %s\n", ntohs(tcp_header->urg_ptr),
		        interface);
	}
}

void tcp_segment_after(struct tcphdr* tcp_header, const char* interface) {
	if (tcp_header != NULL) {
		fprintf(stdout, "TCP segment flag \"URG\" after modify \"%hu\" on interface %s\n", ntohs(tcp_header->urg_ptr),
		        interface);
	}
}

int main(int argc, char* argv[]) {
	char buffer[PCAP_ERRBUF_SIZE];
	char interface[BUFFER_SIZE];
	pcap_t* handle_before = NULL;
	const u_char* packet = NULL;
	struct pcap_pkthdr header;

	if (argc != 2) {
		fprintf(stdout, "Usage: %s (interface)\n", argv[0]);
		return EXIT_FAILURE;
	}

	strcpy(interface, argv[1]);

	handle_before = pcap_open_live(interface, BUFSIZ, 1, 1000, buffer);
	if (handle_before == NULL) {
		fprintf(stdout, "\nCould not open device: %s", interface);
		return -1;
	}

	for (unsigned long long i = 0;; ++i) {
		packet = pcap_next(handle_before, &header);
		if (packet == NULL) {
			continue;
		}

		struct tcphdr* tcp_header = check_tcp_segment(packet);
		if (tcp_header == NULL) {
			continue;
		}

		fprintf(stdout, "\n+-----------------Start of TCP segment %llu-----------------+\n", i);
		tcp_segment_before(tcp_header, interface);
		modify_tcp_segment(tcp_header);

		if (pcap_sendpacket(handle_before, packet, header.len) != 0) {
			fprintf(stdout, "Error sending packet: %s\n", pcap_geterr(handle_before));
			pcap_close(handle_before);
			return -1;
		}

		packet = pcap_next(handle_before, &header);
		if (packet != NULL) {
			struct tcphdr* modified_tcp_header = check_tcp_segment(packet);
			tcp_segment_after(modified_tcp_header, interface);
		}
		fprintf(stdout, "+------------------End of TCP segment %llu------------------+\n", i);
	}
	pcap_close(handle_before);
	return 0;
}
