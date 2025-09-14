# Trunk-Recorder Development Todo

## ✅ Configuration Status
- [x] **trunk_decoder plugin** - Fully implemented with all config options
- [x] **HTTP API support** - Complete with auth, SSL, compression, timeout
- [x] **All config.api.detroit.json items** - Already supported

## P25 Control Channel Streaming Plugin 
- [ ] **Create P25 control channel streaming output plugin architecture**
  - Design plugin structure and interfaces
  - Support dual streaming methods: UDP + shared library
  - Focus on control channel data only (not voice)
  - Plugin-based architecture for modularity

- [ ] **Design dual streaming methods in plugin: UDP + shared library**
  - UDP streaming for network/distributed deployments
  - Shared library calls for local/same-machine deployments
  - Auto-detection of optimal method based on config
  - Packet structure and metadata design

- [ ] **Identify P25 control channel data capture points for plugin integration**
  - Hook into P25 parser before/during TSBK decoding
  - Capture raw control channel bitstream with metadata
  - Determine optimal capture points for minimal overhead
  - Map to existing plugin API hooks

- [ ] **Implement UDP control channel bitstream streaming in plugin**
  - UDP packet structure with sequencing and checksums
  - Non-blocking transmission with queue management
  - Error handling and connection management
  - Performance optimization for real-time streaming

- [ ] **Implement shared library control channel streaming in plugin**
  - Direct function calls to trunk-decoder library
  - Memory buffer management and zero-copy where possible
  - Synchronous/asynchronous call modes
  - Error handling and fallback mechanisms

- [ ] **Add plugin configuration and testing for control channel streaming**
  - Plugin configuration schema and validation
  - Unit tests and integration testing
  - Performance benchmarking and optimization
  - Documentation and example configurations

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