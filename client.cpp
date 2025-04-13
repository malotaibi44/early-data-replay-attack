#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// Simple QUIC client implementation using a basic socket-based approach
// This demonstrates the concept without requiring complex libraries

class QuicClient {
private:
    int sock_fd;
    struct sockaddr_in server_addr;
    std::vector<uint8_t> session_ticket;
    bool has_ticket;

public:
    QuicClient() : sock_fd(-1), has_ticket(false) {}

    ~QuicClient() {
        if (sock_fd >= 0) {
            close(sock_fd);
        }
    }

    bool init() {
        // Create UDP socket
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Make socket non-blocking
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

        // Set server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(4433);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        return true;
    }

    // Save session ticket to a file
    bool saveSessionTicket(const std::string& filename) {
        if (session_ticket.empty()) {
            std::cerr << "No session ticket to save" << std::endl;
            return false;
        }
        
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(session_ticket.data()), session_ticket.size());
        std::cout << "Session ticket saved to " << filename << std::endl;
        return true;
    }

    // Load session ticket from a file
    bool loadSessionTicket(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Failed to open file for reading: " << filename << std::endl;
            return false;
        }
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        session_ticket.resize(size);
        file.read(reinterpret_cast<char*>(session_ticket.data()), size);
        
        has_ticket = true;
        std::cout << "Session ticket loaded from " << filename << std::endl;
        return true;
    }

    // Connect with full handshake to get session ticket
    bool connectWithFullHandshake() {
        std::cout << "Initiating full handshake with server..." << std::endl;
        
        // Send initial handshake packet
        uint8_t packet[1500];
        packet[0] = 0x01;  // Initial handshake packet type
        
        // Simply send client identifier for demo purposes
        std::string client_id = "client1";
        memcpy(packet + 1, client_id.c_str(), client_id.length());
        
        ssize_t sent = sendto(sock_fd, packet, 1 + client_id.length(), 0,
                           (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        if (sent < 0) {
            std::cerr << "Failed to send handshake packet: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Wait for handshake response with session ticket
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            uint8_t buf[1500];
            struct sockaddr_in peer_addr;
            socklen_t peer_addr_len = sizeof(peer_addr);
            
            ssize_t recv_len = recvfrom(sock_fd, buf, sizeof(buf), 0,
                                    (struct sockaddr *)&peer_addr, &peer_addr_len);
            
            if (recv_len < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // No data available, check timeout
                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        current_time - start_time).count();
                    
                    if (elapsed > 5) {
                        std::cerr << "Handshake timeout" << std::endl;
                        return false;
                    }
                    
                    // Wait a bit before trying again
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                continue;
            }
            
            // Check if this is a handshake response
            if (recv_len >= 1 && buf[0] == 0x02) {
                if (recv_len < 3) {
                    std::cerr << "Invalid handshake response" << std::endl;
                    return false;
                }
                
                // Extract session ticket
                uint16_t ticket_len = (buf[1] << 8) | buf[2];
                if (recv_len < 3 + ticket_len) {
                    std::cerr << "Invalid handshake response (truncated ticket)" << std::endl;
                    return false;
                }
                
                session_ticket.assign(buf + 3, buf + 3 + ticket_len);
                has_ticket = true;
                
                std::cout << "Handshake completed, received session ticket of " 
                         << ticket_len << " bytes" << std::endl;
                
                // Send a regular data packet
                sendRegularData("Hello after full handshake!");
                
                return true;
            }
        }
        
        return false;
    }

    // Send regular data after handshake
    bool sendRegularData(const std::string& data) {
        std::cout << "Sending regular data: " << data << std::endl;
        
        uint8_t packet[1500];
        packet[0] = 0x06;  // Regular data packet type
        
        memcpy(packet + 1, data.c_str(), data.length());
        
        ssize_t sent = sendto(sock_fd, packet, 1 + data.length(), 0,
                           (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        if (sent < 0) {
            std::cerr << "Failed to send data packet: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Wait for response
        return receiveResponse();
    }

    // Connect with 0-RTT using session ticket
    bool connectWith0RTT(const std::string& early_data) {
        if (!has_ticket) {
            std::cerr << "No session ticket available for 0-RTT" << std::endl;
            return false;
        }
        
        std::cout << "Attempting 0-RTT connection with early data: " << early_data << std::endl;
        
        // Create 0-RTT packet with session ticket and early data
        uint8_t packet[1500];
        packet[0] = 0x03;  // 0-RTT packet type
        
        // Add ticket length and ticket
        uint16_t ticket_len = session_ticket.size();
        packet[1] = (ticket_len >> 8) & 0xFF;
        packet[2] = ticket_len & 0xFF;
        
        memcpy(packet + 3, session_ticket.data(), ticket_len);
        
        // Add early data
        memcpy(packet + 3 + ticket_len, early_data.c_str(), early_data.length());
        
        ssize_t sent = sendto(sock_fd, packet, 3 + ticket_len + early_data.length(), 0,
                           (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        if (sent < 0) {
            std::cerr << "Failed to send 0-RTT packet: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Wait for response
        return receiveResponse();
    }

private:
    // Wait for and process server response
    bool receiveResponse() {
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            uint8_t buf[1500];
            struct sockaddr_in peer_addr;
            socklen_t peer_addr_len = sizeof(peer_addr);
            
            ssize_t recv_len = recvfrom(sock_fd, buf, sizeof(buf), 0,
                                    (struct sockaddr *)&peer_addr, &peer_addr_len);
            
            if (recv_len < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // No data available, check timeout
                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        current_time - start_time).count();
                    
                    if (elapsed > 5) {
                        std::cerr << "Response timeout" << std::endl;
                        return false;
                    }
                    
                    // Wait a bit before trying again
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                continue;
            }
            
            // Process response based on type
            if (recv_len >= 1) {
                uint8_t response_type = buf[0];
                
                switch (response_type) {
                    case 0x04: {  // 0-RTT response
                        std::string response(reinterpret_cast<char*>(buf + 1), recv_len - 1);
                        std::cout << "Received 0-RTT response: " << response << std::endl;
                        return true;
                    }
                    
                    case 0x05: {  // 0-RTT rejection
                        std::cout << "0-RTT data rejected by server" << std::endl;
                        return false;
                    }
                    
                    case 0x07: {  // Regular data response
                        std::string response(reinterpret_cast<char*>(buf + 1), recv_len - 1);
                        std::cout << "Received regular response: " << response << std::endl;
                        return true;
                    }
                    
                    default:
                        std::cerr << "Unknown response type: " << (int)response_type << std::endl;
                        return false;
                }
            }
        }
        
        return false;
    }
};

int main() {
    QuicClient client;
    
    if (!client.init()) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }
    
    // First connect with full handshake to get session ticket
    std::cout << "Starting full handshake connection..." << std::endl;
    if (!client.connectWithFullHandshake()) {
        std::cerr << "Failed to connect with full handshake" << std::endl;
        return 1;
    }
    
    // Save session ticket for future use
    client.saveSessionTicket("session_ticket.bin");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "\n--------------------------------------\n" << std::endl;
    
    // Wait a bit before trying 0-RTT

    std::cout << "wait for 10 seconds before sending the 0-RTT early data" << std::endl;
    // Load the ticket (in a real scenario, this would be done in a separate client instance)
    if (!client.loadSessionTicket("session_ticket.bin")) {
        std::cerr << "Failed to load session ticket" << std::endl;
        return 1;
    }
    
    // Now try 0-RTT connection with early data
    std::cout << "Starting 0-RTT connection..." << std::endl;
    std::string early_data = "This is early data sent in 0-RTT!";
    if (!client.connectWith0RTT(early_data)) {
        std::cerr << "Failed to connect with 0-RTT" << std::endl;
        return 1;
    }
    
    std::cout << "0-RTT demonstration completed successfully!" << std::endl;
    
    return 0;
}