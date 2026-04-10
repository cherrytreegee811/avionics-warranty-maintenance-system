#pragma once

#include <common/Crc32.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace network {

  // Magic number for packet validation (REQ-SYS-010)
  constexpr uint32_t PACKET_MAGIC = 0xABCD1234;

  enum class PacketType : uint8_t {
    VERIFICATION_REQUEST = 1,
    VERIFICATION_RESPONSE = 2,
    STATE_CHANGE = 3,
    LANDED_NOTIFICATION = 4,
    DIAGNOSTIC_DATA = 5,
    SCHEMATIC_CHUNK = 6,
    STATE_CHANGE_CONFIRMATION = 7
  };

  enum class StateId : uint8_t { STANDBY = 0, DIAGNOSTIC = 1, MAINTENANCE = 2, FAULT = 3 };

  enum class DiagnosticFaultSeverity : uint8_t {
    MINOR = 0,
    MAJOR = 1,
  };

  enum class ImageFormat : uint8_t {
    RAW = 0,   // Uncompressed pixel data
    PNG = 1,   // PNG compressed image
    JPEG = 2,  // JPEG compressed image
  };

#pragma pack(push, 1)
  struct PacketHeader {
    uint32_t magic;
    PacketType type;
    uint32_t payload_size;
    uint32_t sequence;
    uint32_t checksum;  // CRC32 of header + payload (excluding this field)
  };

  // Verification request (server -> client)
  struct VerificationRequest {
    uint32_t challenge;
    uint64_t timestamp;
  };

  // Verification response (client -> server)
  struct VerificationResponse {
    uint32_t challenge_response;
    uint64_t client_id;
  };

  struct StateChangeRequest {
    StateId target_state;
  };

  struct StateChangeConfirmation {
    StateId applied_state;
  };

  struct DiagnosticFaultCodeHeader {
    int32_t code;
    int64_t timestamp_epoch_ms;
    DiagnosticFaultSeverity severity;
    uint16_t description_size;
  };

  struct ImageChunkHeader {
    uint32_t image_id;         // Unique identifier for image sequence
    uint16_t chunk_index;      // 0-based chunk number
    uint16_t total_chunks;     // Total chunks for this image
    uint32_t chunk_data_size;  // Bytes of image data in this chunk
    ImageFormat format;        // Image format (RAW, PNG, JPEG, etc.)
  };
#pragma pack(pop)

  struct DiagnosticFaultCode {
    int32_t code;
    int64_t timestamp_epoch_ms;
    DiagnosticFaultSeverity severity;
    std::string description;
  };

  // Image transfer constants
  constexpr size_t kMaxImageChunkPayloadSize
      = 1024 * 1024 - sizeof(ImageChunkHeader);  // ~1 MB minus header
  constexpr uint32_t kMaxImageId = 0xFFFFFFFFU;

  // Serialization helpers
  std::vector<uint8_t> serializePacket(PacketType type, const void* payload, size_t payload_size);
  template <typename T> std::vector<uint8_t> serializePacket(PacketType type, const T& payload) {
    return serializePacket(type, &payload, sizeof(T));
  }

  std::vector<uint8_t> serializeDiagnosticDataPayload(
      const std::vector<DiagnosticFaultCode>& faults);
  bool deserializeDiagnosticDataPayload(const std::vector<uint8_t>& payload,
                                        std::vector<DiagnosticFaultCode>& faults);

  // Image chunk serialization (handles multi-chunk images)
  // Returns vector of serialized chunk payloads (one per packet to send)
  std::vector<std::vector<uint8_t>> serializeImagePayload(uint32_t image_id,
                                                          const std::vector<uint8_t>& image_data,
                                                          ImageFormat format);

  // Extracts chunk metadata from a SCHEMATIC_CHUNK packet payload
  bool deserializeImageChunk(const std::vector<uint8_t>& payload, ImageChunkHeader& header_out,
                             std::vector<uint8_t>& chunk_data_out);

  // Deserialization: returns true if valid (magic + CRC), extracts header and payload
  bool deserializePacket(const std::vector<uint8_t>& data, PacketHeader& header,
                         std::vector<uint8_t>& payload);

  // For testing
  uint32_t computePacketChecksum(const PacketHeader& header, const void* payload,
                                 size_t payload_size);

  // For logging: converts StateId to string
  inline constexpr std::string_view stateIdToString(StateId state) {
    switch (state) {
      case StateId::STANDBY:
        return "STANDBY";
      case StateId::DIAGNOSTIC:
        return "DIAGNOSTIC";
      case StateId::MAINTENANCE:
        return "MAINTENANCE";
      case StateId::FAULT:
        return "FAULT";
      default:
        return "UNKNOWN";
    }
  }

  inline constexpr std::string_view diagnosticFaultSeverityToString(
      DiagnosticFaultSeverity severity) {
    switch (severity) {
      case DiagnosticFaultSeverity::MINOR:
        return "MINOR";
      case DiagnosticFaultSeverity::MAJOR:
        return "MAJOR";
      default:
        return "UNKNOWN";
    }
  }

  inline constexpr std::string_view imageFormatToString(ImageFormat format) {
    switch (format) {
      case ImageFormat::RAW:
        return "RAW";
      case ImageFormat::PNG:
        return "PNG";
      case ImageFormat::JPEG:
        return "JPEG";
      default:
        return "UNKNOWN";
    }
  }

  // Image reassembly buffer: holds partial/complete images keyed by image_id
  struct ImageBuffer {
    uint32_t image_id;
    ImageFormat format;
    uint16_t total_chunks;
    size_t total_size_bytes;
    std::vector<std::vector<uint8_t>>
        chunks;                  // chunks[i] = data for chunk i, empty if not received
    std::vector<bool> received;  // received[i] = true if chunk i has been received

    ImageBuffer(uint32_t id, ImageFormat fmt, uint16_t total)
        : image_id(id), format(fmt), total_chunks(total), total_size_bytes(0) {
      chunks.resize(total);
      received.resize(total, false);
    }

    // Add a chunk to the buffer. Returns true if image is now complete.
    bool addChunk(uint16_t chunk_index, const std::vector<uint8_t>& data) {
      if (chunk_index >= total_chunks) {
        return false;  // Invalid chunk index
      }
      if (received[chunk_index]) {
        return isComplete();  // Already have this chunk
      }

      chunks[chunk_index] = data;
      received[chunk_index] = true;
      total_size_bytes += data.size();
      return isComplete();
    }

    bool isComplete() const {
      for (bool r : received) {
        if (!r) return false;
      }
      return true;
    }

    // Reassemble all chunks into a single vector
    std::vector<uint8_t> reassemble() const {
      std::vector<uint8_t> result;
      result.reserve(total_size_bytes);
      for (const auto& chunk : chunks) {
        result.insert(result.end(), chunk.begin(), chunk.end());
      }
      return result;
    }
  };
}  // namespace network
