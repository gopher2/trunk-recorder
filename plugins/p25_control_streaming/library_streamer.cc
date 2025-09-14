#include "p25_control_streaming.h"

LibraryStreamer::LibraryStreamer(const std::string& library_path) 
    : library_handle(nullptr), process_control_data(nullptr) {
    this->library_path = library_path;
}

LibraryStreamer::~LibraryStreamer() {
    shutdown();
}

bool LibraryStreamer::initialize() {
    // Load the trunk-decoder shared library
    library_handle = dlopen(library_path.c_str(), RTLD_LAZY);
    if (!library_handle) {
        BOOST_LOG_TRIVIAL(error) << "[library_streamer] Failed to load library: " << dlerror();
        return false;
    }
    
    // Get the function pointer for processing control data
    process_control_data = (process_control_data_func_t)dlsym(library_handle, "process_p25_control_data");
    if (!process_control_data) {
        BOOST_LOG_TRIVIAL(error) << "[library_streamer] Failed to find function 'process_p25_control_data': " << dlerror();
        dlclose(library_handle);
        library_handle = nullptr;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "[library_streamer] Shared library initialized: " << library_path;
    return true;
}

int LibraryStreamer::send_data(const uint8_t* data, size_t length, 
                              uint32_t system_id, double frequency, uint64_t timestamp) {
    if (!library_handle || !process_control_data) {
        return -1;
    }
    
    // Call the trunk-decoder function directly
    int result = process_control_data(data, length, system_id, frequency, timestamp);
    
    if (result != 0) {
        BOOST_LOG_TRIVIAL(warning) << "[library_streamer] Library function returned error: " << result;
    } else {
        BOOST_LOG_TRIVIAL(trace) << "[library_streamer] Sent " << length << " bytes to library, system=" << system_id;
    }
    
    return result;
}

void LibraryStreamer::shutdown() {
    if (library_handle) {
        dlclose(library_handle);
        library_handle = nullptr;
        process_control_data = nullptr;
        BOOST_LOG_TRIVIAL(info) << "[library_streamer] Shared library closed";
    }
}