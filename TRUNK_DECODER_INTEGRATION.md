# Raw P25 Bitstream Integration with Trunk-Decoder

## Project Overview
Implementation of raw P25 control channel bitstream streaming from trunk-recorder to trunk-decoder for comprehensive protocol decoding and event extraction.

## Architecture - Raw Bitstream Approach
```
trunk-recorder → trunk-decoder → trunk-player
  (raw P25)      (decode + events)  (alerts + storage)
```

## Benefits of Raw Bitstream Streaming
- **Single Source of Truth**: trunk-decoder becomes authoritative P25 parser
- **No Code Duplication**: Eliminate redundant parsing logic
- **Better Accuracy**: Comprehensive P25 implementation in one place
- **Easier Maintenance**: Protocol updates only in trunk-decoder
- **More Event Coverage**: Full sdrtrunk-level parsing capabilities
- **Clean Architecture**: trunk-recorder focuses on RF, trunk-decoder on protocol

## Implementation Tasks

### Phase 1: Raw Bitstream Interface Design
- [ ] Define P25 bitstream data format (bits, symbols, frames)
- [ ] Design streaming protocol (HTTP API vs shared library)
- [ ] Specify metadata requirements (frequency, timestamp, system info)
- [ ] Create bitstream packet structure for transmission

### Phase 2: Trunk-Recorder Integration
- [ ] Add configuration for raw P25 control channel streaming
- [ ] Identify P25 control channel data capture points
- [ ] Implement raw bitstream capture before decoding
- [ ] Create P25 bitstream streaming client
- [ ] Add dual integration modes (API + shared library)
- [ ] Test raw bitstream streaming to trunk-decoder

### Phase 3: Error Handling & Performance
- [ ] Add connection management and retries
- [ ] Implement backpressure handling for high data rates
- [ ] Add bitstream validation and integrity checks
- [ ] Performance optimization for real-time streaming

## Raw Bitstream Data Format
```cpp
struct P25BitstreamPacket {
    uint64_t timestamp_us;           // Microsecond timestamp
    uint32_t system_id;              // System identifier
    uint32_t site_id;                // Site identifier  
    double frequency;                // Control channel frequency
    uint32_t sample_rate;            // Sample rate of data
    uint32_t data_length;            // Length of bitstream data
    uint8_t* bitstream_data;         // Raw P25 bitstream
    std::string metadata;            // Additional context (JSON)
};
```

## Configuration Schema
```json
{
  "enableP25BitstreamStreaming": true,
  "p25BitstreamIntegrationMethod": "api",  // or "shared_library"
  "p25BitstreamEndpoint": "http://localhost:3000/api/v1/p25-bitstream",
  "p25BitstreamApiKey": "your-api-key-here",
  "trunkDecoderLibraryPath": "/usr/local/lib/libtrunk-decoder.so",
  "p25BitstreamConfig": {
    "bufferSize": 8192,
    "maxQueueSize": 1000,
    "compressionEnabled": false,
    "validateIntegrity": true
  }
}
```

## Integration Methods
1. **HTTP API**: POST raw bitstream to trunk-decoder REST endpoint
2. **Shared Library**: Direct function calls with bitstream buffers
3. **Message Queue**: High-performance streaming via ZeroMQ (future)

## Development Priority
1. **Interface design** (protocol specification)
2. **Configuration support** (enable/disable streaming)
3. **Raw data capture** (hook into P25 processing)
4. **Streaming implementation** (API + library modes)
5. **Testing & validation** (end-to-end verification)

---
**Status**: Planning Phase - Raw Bitstream Architecture  
**Next Project**: trunk-decoder (P25 protocol parsing + event extraction)  
**Priority**: High - Clean architecture for P25 ecosystem  
**Estimated Effort**: 2-3 weeks development time