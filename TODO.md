# Trunk-Recorder Development Todo

## ✅ Configuration Status
- [x] **trunk_decoder plugin** - Fully implemented with all config options
- [x] **HTTP API support** - Complete with auth, SSL, compression, timeout
- [x] **All config.api.detroit.json items** - Already supported

## ✅ P25 Control Channel Streaming Plugin - COMPLETED
- [x] **P25 control channel streaming output plugin architecture**
  - ✅ Complete dual streaming system (UDP + shared library)
  - ✅ Plugin structure with auto-detection of streaming method
  - ✅ Focus on control channel data only (not voice)
  - ✅ Plugin-based architecture for modularity

- [x] **Dual streaming methods: UDP + shared library**
  - ✅ UDP streaming for network/distributed deployments  
  - ✅ Shared library calls for local/same-machine deployments
  - ✅ Auto-detection based on library availability
  - ✅ P25C packet structure with metadata

- [x] **P25 control channel data capture integration**
  - ✅ Hooked into P25 parser at TSBK processing point (p25_parser.cc:1140)
  - ✅ Added p25_tsbk_data() plugin API method
  - ✅ Captures raw TSBK bitstream with NAC, frequency, timestamp
  - ✅ Integrated with plugin manager for all plugins

- [x] **UDP control channel bitstream streaming**
  - ✅ P25C packet format with sequencing and checksums
  - ✅ Thread-safe background UDP transmission with queuing
  - ✅ Error handling and drop rate monitoring
  - ✅ Performance optimized for real-time streaming

- [x] **Shared library control channel streaming**
  - ✅ Direct function calls to trunk-decoder library
  - ✅ Dynamic library loading with dlopen/dlsym
  - ✅ Synchronous call mode with error handling
  - ✅ Fallback to UDP if library unavailable

- [x] **Plugin configuration and testing**
  - ✅ Complete configuration schema in config.api.detroit.json
  - ✅ Verbose debugging and performance statistics
  - ✅ Successfully tested with live P25 system
  - ✅ Real-time TSBK data streaming verified working

**Status**: 🎉 **COMPLETE** - Plugin successfully streaming live P25 TSBK data to UDP port 9999

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
**Status**: ✅ **TRUNK-RECORDER COMPLETE** - P25 control streaming plugin fully implemented and working
**Next Project**: 🎯 **trunk-decoder** - Implement P25 TSBK packet receiver and protocol analysis engine

## Next Phase: trunk-decoder Development
- [ ] **Implement P25 TSBK packet receiver** - UDP listener for P25C packets from trunk-recorder  
- [ ] **Design P25 TSBK packet parsing engine** - Comprehensive protocol analysis and decoding
- [ ] **Implement real-time P25 event detection** - Emergency activations, unit movements, alerts
- [ ] **Add P25 protocol logging and analysis** - Detailed logging and forensic capabilities  
- [ ] **Test end-to-end streaming pipeline** - Verify trunk-recorder → trunk-decoder integration