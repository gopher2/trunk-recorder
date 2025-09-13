#include <curl/curl.h>
#include <iomanip>
#include <time.h>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <map>

#include "../../trunk-recorder/call_concluder/call_concluder.h"
#include "../../trunk-recorder/plugin_manager/plugin_api.h"
#include <boost/dll/alias.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

struct Sender_System {
  std::string short_name;
  bool enabled;
  std::string server_url;
  std::string auth_token;  // Authentication token
  bool verify_ssl;         // Verify SSL certificates (default: true)
  bool compress_data;      // Enable data compression for transfer (default: false)
};

struct Call_P25_Buffer {
  std::vector<uint8_t> p25_data;
  Call_Data_t call_info;
  bool call_active;
  
  Call_P25_Buffer() : call_active(false) {}
};

struct Sender_Data {
  std::vector<Sender_System> systems;
  std::string default_server_url;
  std::string default_auth_token;
  bool enabled;
  int timeout_seconds;
  bool verify_ssl;      // Global SSL verification setting
  bool compress_data;   // Global data compression setting
  std::map<long, Call_P25_Buffer> active_calls;  // Map call_num to P25 buffer
};

class Sender : public Plugin_Api {
private:
  Sender_Data data;
  CURLSH *curl_share;
  static bool curl_initialized;
  
public:
  Sender_System *get_system_config(std::string short_name) {
    for (auto &sys : data.systems) {
      if (sys.short_name == short_name) {
        return &sys;
      }
    }
    return nullptr;
  }

  static size_t write_callback(void *contents, size_t size, size_t nmemb, std::string *response) {
    response->append((char*)contents, size * nmemb);
    return size * nmemb;
  }
  
  std::vector<uint8_t> read_file_to_memory(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
      return {};
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file into memory
    std::vector<uint8_t> data(size);
    file.read((char*)data.data(), size);
    
    return data;
  }

  int init(Config *config, std::vector<Source *> sources, std::vector<System *> systems) override {
    // Initialize CURL globally if not already done
    if (!curl_initialized) {
      CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
      if (result != CURLE_OK) {
        BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] Failed to initialize CURL: " << curl_easy_strerror(result);
        return -1;
      }
      curl_initialized = true;
    }
    
    // Create shared CURL handle for connection reuse
    curl_share = curl_share_init();
    if (!curl_share) {
      BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] Failed to create CURL share handle";
      return -1;
    }
    
    curl_share_setopt(curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(curl_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    
    // Force P25 saving when trunk_decoder plugin is enabled
    if (config && !config->save_p25) {
      BOOST_LOG_TRIVIAL(info) << "[trunk_decoder] Enabling P25 saving (required for trunk_decoder plugin)";
      config->save_p25 = true;
    }
    
    BOOST_LOG_TRIVIAL(info) << "[trunk_decoder] Plugin initialized";
    return 0;
  }

  int parse_config(json config_data) override {
    data.enabled = config_data.value("enabled", false);
    data.default_server_url = config_data.value("server", "http://localhost:3000/api/v1/decode");
    data.default_auth_token = config_data.value("auth_token", "");
    data.timeout_seconds = config_data.value("timeout", 30);
    data.verify_ssl = config_data.value("verify_ssl", true);
    data.compress_data = config_data.value("compress_data", false);

    if (config_data.find("systems") != config_data.end()) {
      for (auto &system_data : config_data["systems"]) {
        Sender_System sys;
        sys.short_name = system_data.value("short_name", "");
        sys.enabled = system_data.value("enabled", true);
        sys.server_url = system_data.value("server", data.default_server_url);
        sys.auth_token = system_data.value("auth_token", data.default_auth_token);
        sys.verify_ssl = system_data.value("verify_ssl", data.verify_ssl);
        sys.compress_data = system_data.value("compress_data", data.compress_data);
        
        data.systems.push_back(sys);
      }
    }

    BOOST_LOG_TRIVIAL(info) << "[trunk_decoder] Plugin enabled: " << (data.enabled ? "true" : "false");
    if (data.enabled) {
      BOOST_LOG_TRIVIAL(info) << "[trunk_decoder] Server: " << data.default_server_url;
    }
    return 0;
  }

  int call_start(Call *call) override {
    if (!data.enabled || !call) {
      return 0;
    }
    
    long call_num = call->get_call_num();
    
    // Initialize P25 buffer for this call
    Call_P25_Buffer& buffer = data.active_calls[call_num];
    buffer.call_active = true;
    buffer.p25_data.clear();
    
    return 0;
  }

  int send_decode_request(Call_Data_t call_info) {
    if (!data.enabled) {
      return 0;
    }

    // Check if P25 file exists
    struct stat file_stat;
    if (stat(call_info.p25_filename, &file_stat) != 0) {
      BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] P25 file not found: " << call_info.p25_filename;
      return -1;
    }
    
    // Get system-specific configuration
    Sender_System *sys_config = get_system_config(call_info.short_name);
    std::string server_url = sys_config ? sys_config->server_url : data.default_server_url;
    std::string auth_token = sys_config ? sys_config->auth_token : data.default_auth_token;
    bool verify_ssl = sys_config ? sys_config->verify_ssl : data.verify_ssl;
    
    // Initialize CURL handle
    CURL *curl = curl_easy_init();
    if (!curl) {
      BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] Failed to initialize CURL handle";
      return -1;
    }
    
    // Read P25 file
    std::vector<uint8_t> file_data = read_file_to_memory(call_info.p25_filename);
    if (file_data.empty()) {
      BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] Failed to read P25 file: " << call_info.p25_filename;
      curl_easy_cleanup(curl);
      return -1;
    }
    
    // Create multipart form data
    curl_mime *form = curl_mime_init(curl);
    curl_mimepart *field;
    
    // Add P25 file field
    field = curl_mime_addpart(form);
    curl_mime_name(field, "p25_file");
    curl_mime_filename(field, boost::filesystem::path(call_info.p25_filename).filename().string().c_str());
    curl_mime_type(field, "application/octet-stream");
    curl_mime_data(field, (const char*)file_data.data(), file_data.size());
    
    // Read and add the corresponding JSON file
    std::string json_filename = call_info.p25_filename;
    if (json_filename.size() > 4 && json_filename.substr(json_filename.size() - 4) == ".p25") {
        json_filename = json_filename.substr(0, json_filename.size() - 4) + ".json";
    }
    
    std::vector<uint8_t> json_data = read_file_to_memory(json_filename);
    if (!json_data.empty()) {
        field = curl_mime_addpart(form);
        curl_mime_name(field, "metadata");
        curl_mime_type(field, "application/json");
        curl_mime_data(field, (const char*)json_data.data(), json_data.size());
    }
    
    // Set CURL options
    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_URL, server_url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)data.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "trunk-recorder-plugin/1.0");
    
    if (!verify_ssl) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    if (curl_share) {
      curl_easy_setopt(curl, CURLOPT_SHARE, curl_share);
    }
    
    // Add authorization header if token provided
    struct curl_slist *headers = nullptr;
    if (!auth_token.empty()) {
      std::string auth_header = "Authorization: Bearer " + auth_token;
      headers = curl_slist_append(headers, auth_header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    // Clean up
    if (headers) {
      curl_slist_free_all(headers);
    }
    curl_mime_free(form);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
      BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] HTTP request failed for call " << call_info.call_num 
                               << ": " << curl_easy_strerror(res);
      return -1;
    }
    
    if (response_code >= 200 && response_code < 300) {
      BOOST_LOG_TRIVIAL(info) << "[trunk_decoder] Successfully sent call " << call_info.call_num 
                              << " (TG " << call_info.talkgroup << ") to decoder service";
    } else {
      BOOST_LOG_TRIVIAL(error) << "[trunk_decoder] Server error for call " << call_info.call_num 
                               << ": HTTP " << response_code;
      return -1;
    }
    
    return 0;
  }

  int call_end(Call_Data_t call_info) override {
    // Clean up the active call buffer
    auto it = data.active_calls.find(call_info.call_num);
    if (it != data.active_calls.end()) {
      data.active_calls.erase(it);
    }
    
    return send_decode_request(call_info);
  }

  int stop() override {
    // Clean up CURL resources
    if (curl_share) {
      curl_share_cleanup(curl_share);
      curl_share = nullptr;
    }
    
    // Clean up active calls
    data.active_calls.clear();
    
    BOOST_LOG_TRIVIAL(info) << "[trunk_decoder] Plugin stopped";
    return 0;
  }

  static boost::shared_ptr<Sender> create() {
    return boost::shared_ptr<Sender>(
        new Sender());
  }
};

// Define the static variable
bool Sender::curl_initialized = false;

BOOST_DLL_ALIAS(
    Sender::create,
    create_plugin
)