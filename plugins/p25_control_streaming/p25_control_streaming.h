#ifndef P25_CONTROL_STREAMING_H
#define P25_CONTROL_STREAMING_H

#include "../../trunk-recorder/plugin_manager/plugin_api.h"
#include <boost/dll/alias.hpp>
#include <boost/log/trivial.hpp>
#include <json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Streaming methods
enum class StreamingMethod {
    UDP_NETWORK,
    SHARED_LIBRARY,
    DISABLED
};

// P25 Control Channel packet structure
struct P25ControlPacket {
    uint32_t magic;              // 0x50323543 ("P25C")
    uint32_t version;            // Protocol version (1)
    uint64_t timestamp_us;       // Microsecond timestamp
    uint32_t sequence_number;    // For ordering/loss detection
    uint32_t system_id;          // System identifier (NAC)
    uint32_t site_id;            // Site identifier
    double frequency;            // Control channel frequency
    uint32_t sample_rate;        // Sample rate of data
    uint16_t data_length;        // Length of P25 data
    uint16_t checksum;           // Simple integrity check
    uint8_t control_data[];      // Variable length P25 control data
};

// Plugin configuration
struct P25ControlConfig {
    bool enabled;
    StreamingMethod method;
    
    // UDP Configuration
    std::string udp_endpoint;
    std::string udp_host;
    int udp_port;
    size_t udp_packet_size;
    size_t udp_buffer_size;
    bool enable_sequencing;
    bool enable_checksums;
    
    // Shared Library Configuration
    std::string library_path;
    size_t library_buffer_size;
    bool enable_buffering;
    bool synchronous_calls;
    
    // General Settings
    bool stream_tsbk_messages;
    bool stream_raw_bitstream;
    bool stream_system_metadata;
    int queue_size_limit;
    
    // Debug Settings
    bool verbose_debug;
    bool log_packet_details;
    bool log_performance_stats;
};

// Forward declarations
class UDPStreamer;
class LibraryStreamer;

class P25ControlStreaming : public Plugin_Api {
private:
    P25ControlConfig config;
    std::atomic<bool> running;
    std::atomic<uint32_t> sequence_counter;
    
    // Statistics tracking
    std::atomic<uint64_t> packets_sent;
    std::atomic<uint64_t> packets_dropped;
    std::atomic<uint64_t> bytes_sent;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_stats_time;
    
    // Streaming implementations
    std::unique_ptr<UDPStreamer> udp_streamer;
    std::unique_ptr<LibraryStreamer> library_streamer;
    
    // Threading for UDP mode
    std::thread streaming_thread;
    std::queue<P25ControlPacket> packet_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    
    // Helper methods
    StreamingMethod detect_streaming_method();
    uint16_t calculate_checksum(const uint8_t* data, size_t length);
    void streaming_worker();
    void log_packet_info(const P25ControlPacket& packet);
    void log_performance_stats();
    void reset_statistics();
    
public:
    P25ControlStreaming();
    ~P25ControlStreaming();
    
    // Plugin API implementation
    int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override;
    int parse_config(json config_data) override;
    int start() override;
    int stop() override;
    int p25_tsbk_data(const uint8_t* data, size_t length, uint32_t nac, double frequency, uint64_t timestamp, System *system) override;
    
    // P25 data streaming methods
    int stream_tsbk_data(const uint8_t* data, size_t length, 
                        uint32_t system_id, double frequency, uint64_t timestamp);
    int stream_control_metadata(uint32_t system_id, const std::string& metadata);
    
    // Factory method for plugin creation
    static boost::shared_ptr<P25ControlStreaming> create();
};

// UDP Streamer implementation
class UDPStreamer {
private:
    int socket_fd;
    struct sockaddr_in server_addr;
    std::string host;
    int port;
    
public:
    UDPStreamer(const std::string& endpoint);
    ~UDPStreamer();
    
    bool initialize();
    int send_packet(const P25ControlPacket& packet);
    void shutdown();
};

// Shared Library Streamer implementation  
class LibraryStreamer {
private:
    void* library_handle;
    std::string library_path;
    typedef int (*process_control_data_func_t)(const uint8_t*, size_t, uint32_t, double, uint64_t);
    process_control_data_func_t process_control_data;
    
public:
    LibraryStreamer(const std::string& library_path);
    ~LibraryStreamer();
    
    bool initialize();
    int send_data(const uint8_t* data, size_t length, 
                 uint32_t system_id, double frequency, uint64_t timestamp);
    void shutdown();
};

#endif // P25_CONTROL_STREAMING_H