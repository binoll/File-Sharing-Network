#include "switch.h"

void update_mac_table(const u_char* mac, const char* interface) {
	time_t current_time = time(NULL);

	for (int i = 0; i < TABLE_SIZE; ++i) {
		if (memcmp(mac_table[i].mac, mac, ETHER_ADDR_LEN) == 0) {
			mac_table[i].timestamp = current_time;
			strcpy(mac_table[i].interface, interface);
			return;
		}
	}

	for (int i = 0; i < TABLE_SIZE; ++i) {
		if (mac_table[i].timestamp == 0 || current_time - mac_table[i].timestamp > TIMEOUT) {
			memcpy(mac_table[i].mac, mac, ETHER_ADDR_LEN);
			mac_table[i].timestamp = current_time;
			strcpy(mac_table[i].interface, interface);
			return;
		}
	}
}

const char* lookup_mac_table(const u_char* mac) {
	time_t current_time = time(NULL);

	for (int i = 0; i < TABLE_SIZE; ++i) {
		if (memcmp(mac_table[i].mac, mac, ETHER_ADDR_LEN) == 0) {
			if (current_time - mac_table[i].timestamp <= TIMEOUT) {
				return mac_table[i].interface;
			} else {
				mac_table[i].timestamp = 0;
			}
		}
	}

	return NULL;
}

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

void modify_tcp_segment(struct tcphdr* tcp_header, const struct config* cfg) {
	if (tcp_header != NULL && cfg->modify_flag) {
		tcp_header->urg = 1;
		tcp_header->urg_ptr = htons(cfg->urg_ptr_value);
	}
}

void packet_handler(u_char* user, const struct pcap_pkthdr* header, const u_char* packet) {
	struct handler_args* args = (struct handler_args*) user;
	struct config* cfg = &args->cfg;
	char* interface = args->interface1;  // This assumes packet_handler is called for interface1 first
	struct ether_header* eth_header = (struct ether_header*) packet;
	const char* out_interface = lookup_mac_table(eth_header->ether_dhost);
	pcap_t* handle = NULL;

	update_mac_table(eth_header->ether_shost, interface);

	if (out_interface != NULL && strcmp(out_interface, interface) != 0) {
		handle = pcap_open_live(out_interface, BUFSIZ, 1, 1000, NULL);
	} else {
		if (strcmp(interface, args->interface1) == 0) {
			out_interface = args->interface2;
		} else {
			out_interface = args->interface1;
		}
		handle = pcap_open_live(out_interface, BUFSIZ, 1, 1000, NULL);
	}

	if (handle == NULL) {
		fprintf(stderr, "Could not open device %s: %s\n", out_interface, pcap_geterr(handle));
		return;
	}

	struct tcphdr* tcp_header = check_tcp_segment(packet);
	if (tcp_header != NULL) {
		modify_tcp_segment(tcp_header, cfg);
	}

	if (pcap_sendpacket(handle, packet, (int) header->len) != 0) {
		fprintf(stderr, "Error sending packet: %s\n", pcap_geterr(handle));
	}

	pcap_close(handle);
}

int main(int argc, char* argv[]) {
	char buffer[PCAP_ERRBUF_SIZE];
	pcap_t* handle1 = NULL;
	pcap_t* handle2 = NULL;

	if (argc != 4) {
		fprintf(stdout, "Usage: %s (interface1) (interface2) (init.cfg)\n", argv[0]);
		return EXIT_FAILURE;
	}

	struct handler_args args;
	strcpy(args.interface1, argv[1]);
	strcpy(args.interface2, argv[2]);

	struct config cfg;
	read_config("init.cfg", &cfg);
	args.cfg = cfg;

	handle1 = pcap_open_live(args.interface1, BUFSIZ, 1, 1000, buffer);
	if (handle1 == NULL) {
		fprintf(stderr, "Could not open device: %s\n", args.interface1);
		return EXIT_FAILURE;
	}

	handle2 = pcap_open_live(args.interface2, BUFSIZ, 1, 1000, buffer);
	if (handle2 == NULL) {
		fprintf(stderr, "Could not open device: %s\n", args.interface2);
		pcap_close(handle1);
		return EXIT_FAILURE;
	}

	if (pcap_loop(handle1, 0, packet_handler, (u_char*) &args) < 0) {
		fprintf(stderr, "Error occurred while processing packets on %s: %s\n", args.interface1, pcap_geterr(handle1));
		pcap_close(handle1);
		pcap_close(handle2);
		return EXIT_FAILURE;
	}

	if (pcap_loop(handle2, 0, packet_handler, (u_char*) &args) < 0) {
		fprintf(stderr, "Error occurred while processing packets on %s: %s\n", args.interface2, pcap_geterr(handle2));
		pcap_close(handle1);
		pcap_close(handle2);
		return EXIT_FAILURE;
	}

	pcap_close(handle1);
	pcap_close(handle2);
	return 0;
}
