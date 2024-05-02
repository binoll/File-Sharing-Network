#include "../libs.hpp"

void modify_tcp_segment(const u_char* packet_data) {
	const struct ether_header* eth_header = (struct ether_header*) packet_data;
	int32_t ethernet_header_length = sizeof(struct ether_header);

	if (ntohs(eth_header->ether_type) == ETHERTYPE_IP) {
		const struct ip* ip_header = (struct ip*) (packet_data + ethernet_header_length);
		int32_t ip_header_length = ip_header->ip_hl * 4;

		if (ip_header->ip_p == IPPROTO_TCP) {
			auto* tcp_header = (struct tcphdr*) (packet_data + ethernet_header_length + ip_header_length);
			tcp_header->th_flags |= TH_URG;
			std::cout << "Modified TCP segment: " << tcp_header->th_flags << std::endl;
		}
	}
}

int main() {
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t* handle;
	const u_char* packet;
	struct pcap_pkthdr header { };

	handle = pcap_open_live("eth0", BUFSIZ, 1, 1000, errbuf);
	if (handle == nullptr) {
		std::cerr << "Could not open device: " << errbuf << std::endl;
		return 1;
	}

	while (true) {
		packet = pcap_next(handle, &header);

		if (packet == NULL) {
			continue;
		}
		modify_tcp_segment(packet);
	}
	pcap_close(handle);
	return 0;
}