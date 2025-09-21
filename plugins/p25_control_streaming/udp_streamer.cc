#include "p25_control_streaming.h"
#include <unistd.h>
#include <cstring>

UDPStreamer::UDPStreamer(const std::string& endpoint) 
    : socket_fd(-1) {
    // Parse endpoint
    size_t colon_pos = endpoint.find(':');
    if (colon_pos != std::string::npos) {
        host = endpoint.substr(0, colon_pos);
        port = std::stoi(endpoint.substr(colon_pos + 1));
    } else {
        host = "localhost";
        port = 9999;
    }
}

UDPStreamer::~UDPStreamer() {
    shutdown();
}

bool UDPStreamer::initialize() {
    // Create UDP socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        BOOST_LOG_TRIVIAL(error) << "[udp_streamer] Failed to create UDP socket: " << strerror(errno);
        return false;
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        BOOST_LOG_TRIVIAL(error) << "[udp_streamer] Invalid IP address: " << host;
        close(socket_fd);
        socket_fd = -1;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "[udp_streamer] UDP socket initialized for " << host << ":" << port;
    return true;
}

int UDPStreamer::send_packet(const P25ControlPacket& packet) {
    if (socket_fd < 0) {
        return -1;
    }

    // Create buffer with fixed header + data
    size_t header_size = sizeof(uint32_t) * 6 + sizeof(uint64_t) + sizeof(double) + sizeof(uint16_t) * 2;
    size_t total_size = header_size + packet.data_length;
    std::vector<uint8_t> buffer(total_size);

    size_t offset = 0;

    // Serialize packet header manually to avoid padding issues
    memcpy(buffer.data() + offset, &packet.magic, sizeof(uint32_t)); offset += sizeof(uint32_t);
    memcpy(buffer.data() + offset, &packet.version, sizeof(uint32_t)); offset += sizeof(uint32_t);
    memcpy(buffer.data() + offset, &packet.timestamp_us, sizeof(uint64_t)); offset += sizeof(uint64_t);
    memcpy(buffer.data() + offset, &packet.sequence_number, sizeof(uint32_t)); offset += sizeof(uint32_t);
    memcpy(buffer.data() + offset, &packet.system_id, sizeof(uint32_t)); offset += sizeof(uint32_t);
    memcpy(buffer.data() + offset, &packet.site_id, sizeof(uint32_t)); offset += sizeof(uint32_t);
    memcpy(buffer.data() + offset, &packet.frequency, sizeof(double)); offset += sizeof(double);
    memcpy(buffer.data() + offset, &packet.sample_rate, sizeof(uint32_t)); offset += sizeof(uint32_t);
    memcpy(buffer.data() + offset, &packet.data_length, sizeof(uint16_t)); offset += sizeof(uint16_t);
    memcpy(buffer.data() + offset, &packet.checksum, sizeof(uint16_t)); offset += sizeof(uint16_t);

    // Copy packet data from vector
    if (packet.data_length > 0 && !packet.control_data.empty()) {
        memcpy(buffer.data() + offset, packet.control_data.data(), packet.data_length);
    }
    
    // Send UDP packet
    ssize_t bytes_sent = sendto(socket_fd, buffer.data(), total_size, 0,
                               (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (bytes_sent < 0) {
        BOOST_LOG_TRIVIAL(error) << "[udp_streamer] Failed to send UDP packet: " << strerror(errno);
        return -1;
    }
    
    if (bytes_sent != static_cast<ssize_t>(total_size)) {
        BOOST_LOG_TRIVIAL(warning) << "[udp_streamer] Partial UDP packet sent: " 
                                   << bytes_sent << "/" << total_size << " bytes";
        return -1;
    }
    
    return 0;
}

void UDPStreamer::shutdown() {
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
        BOOST_LOG_TRIVIAL(info) << "[udp_streamer] UDP socket closed";
    }
}