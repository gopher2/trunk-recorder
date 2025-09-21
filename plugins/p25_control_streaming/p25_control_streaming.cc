#include "p25_control_streaming.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <chrono>
#include <iomanip>
#include <boost/algorithm/string.hpp>

P25ControlStreaming::P25ControlStreaming() 
    : running(false), sequence_counter(0), packets_sent(0), packets_dropped(0), bytes_sent(0) {
    start_time = std::chrono::steady_clock::now();
    last_stats_time = start_time;
}

P25ControlStreaming::~P25ControlStreaming() {
    stop();
}

int P25ControlStreaming::init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) {
    BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Plugin initializing...";
    return 0;
}

int P25ControlStreaming::parse_config(json config_data) {
    config.enabled = config_data.value("enabled", false);
    
    if (!config.enabled) {
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Plugin disabled";
        return 0;
    }
    
    // Parse streaming method
    std::string method_str = config_data.value("streaming_method", "auto");
    if (method_str == "udp") {
        config.method = StreamingMethod::UDP_NETWORK;
    } else if (method_str == "shared_library") {
        config.method = StreamingMethod::SHARED_LIBRARY;
    } else if (method_str == "auto") {
        config.method = detect_streaming_method();
    } else {
        config.method = StreamingMethod::DISABLED;
    }
    
    // UDP Configuration
    config.udp_endpoint = config_data.value("udp_endpoint", "localhost:9999");
    config.udp_packet_size = config_data.value("udp_packet_size", 1400);
    config.udp_buffer_size = config_data.value("udp_buffer_size", 8192);
    config.enable_sequencing = config_data.value("enable_sequencing", true);
    config.enable_checksums = config_data.value("enable_checksums", true);
    
    // Parse UDP endpoint
    std::vector<std::string> endpoint_parts;
    boost::split(endpoint_parts, config.udp_endpoint, boost::is_any_of(":"));
    if (endpoint_parts.size() == 2) {
        config.udp_host = endpoint_parts[0];
        config.udp_port = std::stoi(endpoint_parts[1]);
    } else {
        config.udp_host = "localhost";
        config.udp_port = 9999;
    }
    
    // Shared Library Configuration
    config.library_path = config_data.value("library_path", "/usr/local/lib/libtrunk-decoder.so");
    config.library_buffer_size = config_data.value("library_buffer_size", 8192);
    config.enable_buffering = config_data.value("enable_buffering", true);
    config.synchronous_calls = config_data.value("synchronous_calls", true);
    
    // General Settings
    config.stream_tsbk_messages = config_data.value("stream_tsbk_messages", true);
    config.stream_raw_bitstream = config_data.value("stream_raw_bitstream", false);
    config.stream_system_metadata = config_data.value("stream_system_metadata", true);
    config.queue_size_limit = config_data.value("queue_size_limit", 1000);
    
    // Debug Settings
    config.verbose_debug = config_data.value("verbose_debug", false);
    config.log_packet_details = config_data.value("log_packet_details", false);
    config.log_performance_stats = config_data.value("log_performance_stats", false);
    
    BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Configuration loaded:";
    BOOST_LOG_TRIVIAL(info) << "  Method: " << (config.method == StreamingMethod::UDP_NETWORK ? "UDP" : 
                                                 config.method == StreamingMethod::SHARED_LIBRARY ? "Library" : "Disabled");
    if (config.method == StreamingMethod::UDP_NETWORK) {
        BOOST_LOG_TRIVIAL(info) << "  UDP Endpoint: " << config.udp_host << ":" << config.udp_port;
    } else if (config.method == StreamingMethod::SHARED_LIBRARY) {
        BOOST_LOG_TRIVIAL(info) << "  Library Path: " << config.library_path;
    }
    
    return 0;
}

int P25ControlStreaming::start() {
    if (!config.enabled || config.method == StreamingMethod::DISABLED) {
        return 0;
    }
    
    running = true;
    
    if (config.method == StreamingMethod::UDP_NETWORK) {
        // Initialize UDP streamer
        udp_streamer = std::make_unique<UDPStreamer>(config.udp_endpoint);
        if (!udp_streamer->initialize()) {
            BOOST_LOG_TRIVIAL(error) << "[p25_control_streaming] Failed to initialize UDP streamer";
            return -1;
        }
        
        // Start background streaming thread
        streaming_thread = std::thread(&P25ControlStreaming::streaming_worker, this);
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] UDP streaming started";
        
    } else if (config.method == StreamingMethod::SHARED_LIBRARY) {
        // Initialize shared library streamer
        library_streamer = std::make_unique<LibraryStreamer>(config.library_path);
        if (!library_streamer->initialize()) {
            BOOST_LOG_TRIVIAL(error) << "[p25_control_streaming] Failed to initialize library streamer";
            return -1;
        }
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Shared library streaming started";
    }
    
    return 0;
}

int P25ControlStreaming::stop() {
    if (!running.load()) {
        return 0;
    }
    
    running = false;
    
    // Stop background thread
    if (streaming_thread.joinable()) {
        queue_cv.notify_all();
        streaming_thread.join();
    }
    
    // Cleanup streamers
    if (udp_streamer) {
        udp_streamer->shutdown();
        udp_streamer.reset();
    }
    
    if (library_streamer) {
        library_streamer->shutdown();
        library_streamer.reset();
    }
    
    BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Plugin stopped";
    return 0;
}

void P25ControlStreaming::log_packet_info(const P25ControlPacket& packet) {
    if (config.verbose_debug || config.log_packet_details) {
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Packet Info:";
        BOOST_LOG_TRIVIAL(info) << "  Magic: 0x" << std::hex << packet.magic;
        BOOST_LOG_TRIVIAL(info) << "  Version: " << std::dec << packet.version;
        BOOST_LOG_TRIVIAL(info) << "  Sequence: " << packet.sequence_number;
        BOOST_LOG_TRIVIAL(info) << "  System ID: " << packet.system_id;
        BOOST_LOG_TRIVIAL(info) << "  Site ID: " << packet.site_id;
        BOOST_LOG_TRIVIAL(info) << "  Frequency: " << packet.frequency << " Hz";
        BOOST_LOG_TRIVIAL(info) << "  Data Length: " << packet.data_length;
        BOOST_LOG_TRIVIAL(info) << "  Checksum: 0x" << std::hex << packet.checksum;
        BOOST_LOG_TRIVIAL(info) << "  Timestamp: " << std::dec << packet.timestamp_us << " μs";
    }
}

void P25ControlStreaming::log_performance_stats() {
    if (!config.log_performance_stats) {
        return;
    }
    
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
    auto stats_interval = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_stats_time);
    
    if (stats_interval.count() >= 60) { // Log stats every minute
        uint64_t sent = packets_sent.load();
        uint64_t dropped = packets_dropped.load();
        uint64_t bytes = bytes_sent.load();
        
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Performance Stats:";
        BOOST_LOG_TRIVIAL(info) << "  Runtime: " << elapsed.count() << " seconds";
        BOOST_LOG_TRIVIAL(info) << "  Packets Sent: " << sent;
        BOOST_LOG_TRIVIAL(info) << "  Packets Dropped: " << dropped;
        BOOST_LOG_TRIVIAL(info) << "  Bytes Sent: " << bytes;
        BOOST_LOG_TRIVIAL(info) << "  Drop Rate: " << (sent > 0 ? (double)dropped / sent * 100.0 : 0.0) << "%";
        BOOST_LOG_TRIVIAL(info) << "  Throughput: " << (elapsed.count() > 0 ? bytes / elapsed.count() : 0) << " bytes/sec";
        
        last_stats_time = current_time;
    }
}

void P25ControlStreaming::reset_statistics() {
    packets_sent = 0;
    packets_dropped = 0;
    bytes_sent = 0;
    start_time = std::chrono::steady_clock::now();
    last_stats_time = start_time;
    
    if (config.verbose_debug) {
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] Statistics reset";
    }
}

int P25ControlStreaming::stream_tsbk_data(const uint8_t* data, size_t length, 
                                         uint32_t system_id, double frequency, uint64_t timestamp) {
    if (!config.enabled || !running.load() || !config.stream_tsbk_messages) {
        return 0;
    }
    
    if (config.verbose_debug) {
        BOOST_LOG_TRIVIAL(debug) << "[p25_control_streaming] Streaming TSBK data: " 
                                 << length << " bytes, system=0x" << std::hex << system_id << ", freq=" << std::dec << frequency;
    }
    
    if (config.method == StreamingMethod::SHARED_LIBRARY && library_streamer) {
        // Direct library call
        int result = library_streamer->send_data(data, length, system_id, frequency, timestamp);
        if (result == 0) {
            packets_sent++;
            bytes_sent += length;
        }
        return result;
        
    } else if (config.method == StreamingMethod::UDP_NETWORK) {
        // Queue for UDP transmission
        P25ControlPacket packet;
        packet.magic = 0x50323543;  // "P25C"
        packet.version = 1;
        packet.timestamp_us = timestamp;
        packet.sequence_number = config.enable_sequencing ? sequence_counter.fetch_add(1) : 0;
        packet.system_id = system_id;
        packet.site_id = 0;  // TODO: Extract from system info
        packet.frequency = frequency;
        packet.sample_rate = 0;  // Not applicable for TSBK data
        packet.data_length = static_cast<uint16_t>(length);
        packet.checksum = config.enable_checksums ? calculate_checksum(data, length) : 0;

        // Copy the actual data into the vector
        packet.control_data.assign(data, data + length);
        
        if (config.log_packet_details) {
            log_packet_info(packet);
        }
        
        // Queue the packet for background transmission
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (packet_queue.size() >= config.queue_size_limit) {
                packets_dropped++;
                if (config.verbose_debug) {
                    BOOST_LOG_TRIVIAL(warning) << "[p25_control_streaming] Queue full, dropping packet (dropped: " 
                                               << packets_dropped.load() << ")";
                }
                return -1;
            }
            packet_queue.push(packet);
        }
        queue_cv.notify_one();
        
        // Update statistics
        packets_sent++;
        bytes_sent += sizeof(P25ControlPacket) + length;
        
        // Periodic performance logging
        log_performance_stats();
    }
    
    return 0;
}

int P25ControlStreaming::p25_tsbk_data(const uint8_t* data, size_t length, uint32_t nac, double frequency, uint64_t timestamp, System *system) {
    if (!config.enabled || !running.load()) {
        return 0;
    }
    
    if (config.verbose_debug) {
        BOOST_LOG_TRIVIAL(debug) << "[p25_control_streaming] Received P25 TSBK data: " 
                                 << length << " bytes, NAC=0x" << std::hex << nac 
                                 << ", freq=" << std::dec << frequency;
    }
    
    // Convert NAC to system ID and call our internal streaming method
    return stream_tsbk_data(data, length, nac, frequency, timestamp);
}

int P25ControlStreaming::stream_control_metadata(uint32_t system_id, const std::string& metadata) {
    if (!config.enabled || !running.load() || !config.stream_system_metadata) {
        return 0;
    }
    
    if (config.verbose_debug) {
        BOOST_LOG_TRIVIAL(debug) << "[p25_control_streaming] Streaming metadata for system " 
                                 << system_id << ": " << metadata;
    }
    
    // For now, just log the metadata. Could be extended to send structured data
    return 0;
}

StreamingMethod P25ControlStreaming::detect_streaming_method() {
    // Check if shared library exists and is accessible
    if (!config.library_path.empty()) {
        void* handle = dlopen(config.library_path.c_str(), RTLD_LAZY);
        if (handle) {
            dlclose(handle);
            return StreamingMethod::SHARED_LIBRARY;
        }
    }
    
    // Default to UDP if library not available
    return StreamingMethod::UDP_NETWORK;
}

uint16_t P25ControlStreaming::calculate_checksum(const uint8_t* data, size_t length) {
    uint16_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

void P25ControlStreaming::streaming_worker() {
    if (config.verbose_debug) {
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] UDP streaming worker started";
    }
    
    while (running.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        
        // Wait for packets or shutdown signal
        queue_cv.wait(lock, [this] { return !packet_queue.empty() || !running.load(); });
        
        // Process all queued packets
        while (!packet_queue.empty() && running.load()) {
            P25ControlPacket packet = packet_queue.front();
            packet_queue.pop();
            lock.unlock();
            
            // Send the packet via UDP
            if (udp_streamer) {
                int result = udp_streamer->send_packet(packet);
                if (result != 0 && config.verbose_debug) {
                    BOOST_LOG_TRIVIAL(error) << "[p25_control_streaming] Failed to send UDP packet, seq=" 
                                             << packet.sequence_number;
                }
            }
            
            lock.lock();
        }
    }
    
    if (config.verbose_debug) {
        BOOST_LOG_TRIVIAL(info) << "[p25_control_streaming] UDP streaming worker stopped";
    }
}

boost::shared_ptr<P25ControlStreaming> P25ControlStreaming::create() {
    return boost::shared_ptr<P25ControlStreaming>(new P25ControlStreaming());
}

// Plugin factory export
BOOST_DLL_ALIAS(P25ControlStreaming::create, create_plugin)