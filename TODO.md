# Trunk-Recorder Development Todo

## Unimplemented Configuration Features
- [ ] **Implement trunk_decoder plugin with HTTP API support**
  - Plugin exists in config.api.detroit.json but implementation missing
  - Add HTTP client for sending P25 data to trunk-decoder API
  - Support server, auth_token, verify_ssl, compress_data, timeout settings

- [ ] **Add plugin configuration support for verify_ssl, compress_data, timeout**
  - Extend plugin system to handle these HTTP client options
  - Add SSL certificate verification toggle
  - Implement data compression for API requests
  - Add configurable HTTP timeout handling

## Raw P25 Bitstream Integration
- [ ] **Design raw P25 bitstream streaming interface**
  - Define bitstream data format (bits, symbols, frames)
  - Design streaming protocol (HTTP API vs shared library)
  - Specify metadata requirements (frequency, timestamp, system info)
  - Create bitstream packet structure for transmission

- [ ] **Add configuration for raw P25 control channel streaming**
  - Add enableP25BitstreamStreaming boolean config
  - Add p25BitstreamIntegrationMethod (api/shared_library)
  - Add p25BitstreamEndpoint URL configuration
  - Add authentication and performance settings

- [ ] **Identify P25 control channel data capture points in trunk-recorder**
  - Find where raw P25 bitstream is available before decoding
  - Locate control channel processing pipeline
  - Determine best capture point for minimal overhead
  - Map bitstream to system/site metadata

- [ ] **Implement raw P25 bitstream capture from control channel**
  - Hook into P25 processing before TSBK decoding
  - Capture raw bitstream with timing and metadata
  - Buffer and format data for streaming
  - Add capture enable/disable controls

- [ ] **Create P25 bitstream streaming client (API + shared library modes)**
  - HTTP API client for POST requests to trunk-decoder
  - Shared library interface for direct function calls
  - Queue management and backpressure handling
  - Error handling and retry logic

- [ ] **Test raw P25 bitstream streaming to trunk-decoder**
  - End-to-end testing with live P25 signals
  - Validate bitstream integrity and timing
  - Performance testing under high load
  - Integration testing with trunk-decoder API

## Plugin System Enhancements
- [ ] **Improve plugin configuration validation**
  - Validate required plugin parameters
  - Add better error messages for missing config
  - Support plugin-specific configuration schemas

- [ ] **Add plugin performance monitoring**
  - Track plugin execution times
  - Monitor plugin memory usage
  - Add plugin health checks and status reporting

## Architecture Notes
**Current Approach**: Parse P25 events in trunk-recorder and forward to trunk-decoder
**New Approach**: Stream raw P25 bitstream to trunk-decoder for comprehensive protocol parsing

**Benefits of Raw Bitstream**:
- Single source of truth for P25 protocol implementation
- Better event coverage and accuracy in trunk-decoder
- Cleaner separation of RF capture vs protocol parsing
- Easier maintenance with centralized P25 expertise

---
**Priority**: Complete trunk_decoder plugin first, then implement raw bitstream streaming
**Next Project**: trunk-decoder (comprehensive P25 protocol processing)