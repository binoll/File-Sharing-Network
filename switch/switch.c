#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <stdlib.h>
#include <string.h>

int BUFFER_SIZE = 256;

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
		tcp_header->urg = tcp_header->urg + 1;
	}
}

void tcp_segment_before(const u_char* packet, const char* interface) {
	struct tcphdr* tcp_header = check_tcp_segment(packet);
	if (tcp_header != NULL) {
		fprintf(stdout, "TCP segment flag \"URG\" before modify \"%i\" on "
		                "interface %s\n", tcp_header->urg, interface);
	}
}

void tcp_segment_after(const u_char* packet, const char* interface) {
	struct tcphdr* tcp_header = check_tcp_segment(packet);
	if (tcp_header != NULL) {
		fprintf(stdout, "TCP segment flag \"URG\" after modify \"%i\" on "
		                "interface %s\n", tcp_header->urg, interface);
	}
}

int main(int argc, char* argv[]) {
	char buffer[PCAP_ERRBUF_SIZE];
	char interface[BUFFER_SIZE];
	pcap_t* handle_before = NULL;
	pcap_t* handle_after = NULL;
	const u_char* packet = NULL;
	struct pcap_pkthdr header;

	if (argc != 3) {
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
		if (packet == NULL || check_tcp_segment(packet) == NULL) {
			continue;
		}
		fprintf(stdout, "\n+-----------------Start of TCP segment %llu-----------------+\n", i);
		tcp_segment_before(packet, interface);
		modify_tcp_segment(packet);
		tcp_segment_after(packet, interface);
		fprintf(stdout, "+------------------End of TCP segment %llu------------------+\n", i);
	}
	pcap_close(handle_before);
	pcap_close(handle_after);
	return 0;
}
