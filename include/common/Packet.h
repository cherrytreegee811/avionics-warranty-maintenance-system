#pragma once

#include <common/WarrantyData.h>
#include <common/Crc32.h>

#include <algorithm>
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
    STATE_CHANGE_CONFIRMATION = 7,
    WARRANTY_DATA = 8,
    CLEAR_DIAGNOSTIC_CODE = 9,
    CLEAR_DIAGNOSTIC_CODE_CONFIRMATION = 10
  };

  enum class StateId : uint8_t { STANDBY = 0, DIAGNOSTIC = 1, MAINTENANCE = 2, FAULT = 3 };

  enum class DiagnosticFaultSeverity : uint8_t {
    MINOR = 0,
    MAJOR = 1,
  };

  enum class DiagnosticCodeClearStatus : uint8_t {
    SUCCESS = 0,
    CODE_NOT_FOUND = 1,
    REJECTED_NOT_IN_CLEARABLE_STATE = 2,
    MALFORMED_REQUEST = 3,
  };

  enum class ImageFormat : uint8_t {
    PNG = 0,
    JPEG = 1,
    RAW = 2,
  };

  constexpr size_t kMaxImageChunkPayloadSize = 1024 * 1024;

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

  struct DiagnosticCodeClearRequest {
    int32_t code;
  };

  struct DiagnosticCodeClearConfirmation {
    int32_t code;
    DiagnosticCodeClearStatus status;
    StateId resulting_state;
  };

  struct WarrantyDataHeader {
    uint8_t is_active;
    uint16_t expiry_date_size;
    uint16_t provider_size;
  };

  struct ImageChunkHeader {
    uint32_t image_id;
    uint16_t chunk_index;
    uint16_t total_chunks;
    uint32_t chunk_data_size;
    ImageFormat format;
  };
#pragma pack(pop)

  struct DiagnosticFaultCode {
    int32_t code;
    int64_t timestamp_epoch_ms;
    DiagnosticFaultSeverity severity;
    std::string description;
  };

  // Serialization helpers
  std::vector<uint8_t> serializePacket(PacketType type, const void* payload, size_t payload_size);
  template <typename T> std::vector<uint8_t> serializePacket(PacketType type, const T& payload) {
    return serializePacket(type, &payload, sizeof(T));
  }

  std::vector<uint8_t> serializeDiagnosticDataPayload(
      const std::vector<DiagnosticFaultCode>& faults);
  bool deserializeDiagnosticDataPayload(const std::vector<uint8_t>& payload,
                                        std::vector<DiagnosticFaultCode>& faults);

  std::vector<uint8_t> serializeWarrantyDataPayload(const common::WarrantyInfo& warranty);
  bool deserializeWarrantyDataPayload(const std::vector<uint8_t>& payload,
                                      common::WarrantyInfo& warranty);

  std::vector<std::vector<uint8_t>> serializeImagePayload(uint32_t image_id,
                                                          const std::vector<uint8_t>& image_data,
                                                          ImageFormat format);
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

  inline constexpr std::string_view diagnosticCodeClearStatusToString(
      DiagnosticCodeClearStatus status) {
    switch (status) {
      case DiagnosticCodeClearStatus::SUCCESS:
        return "SUCCESS";
      case DiagnosticCodeClearStatus::CODE_NOT_FOUND:
        return "CODE_NOT_FOUND";
      case DiagnosticCodeClearStatus::REJECTED_NOT_IN_CLEARABLE_STATE:
        return "REJECTED_NOT_IN_CLEARABLE_STATE";
      case DiagnosticCodeClearStatus::MALFORMED_REQUEST:
        return "MALFORMED_REQUEST";
      default:
        return "UNKNOWN";
    }
  }

  inline constexpr std::string_view imageFormatToString(ImageFormat format) {
    switch (format) {
      case ImageFormat::PNG:
        return "PNG";
      case ImageFormat::JPEG:
        return "JPEG";
      case ImageFormat::RAW:
        return "RAW";
      default:
        return "UNKNOWN";
    }
  }

  struct ImageBuffer {
    uint32_t image_id;
    ImageFormat format;
    size_t total_chunks;
    std::vector<std::vector<uint8_t>> chunks;
    std::vector<bool> received;

    ImageBuffer(uint32_t id, ImageFormat image_format, size_t chunk_count)
        : image_id(id),
          format(image_format),
          total_chunks(chunk_count),
          chunks(chunk_count),
          received(chunk_count, false) {}

    bool addChunk(size_t index, const std::vector<uint8_t>& chunk_data) {
      if (index >= total_chunks) {
        return false;
      }

      if (!received[index]) {
        chunks[index] = chunk_data;
        received[index] = true;
      }

      return isComplete();
    }

    bool isComplete() const {
      return std::all_of(received.begin(), received.end(), [](bool r) { return r; });
    }

    std::vector<uint8_t> reassemble() const {
      std::vector<uint8_t> image_data;
      size_t total_size = 0;
      for (const auto& chunk : chunks) {
        total_size += chunk.size();
      }
      image_data.reserve(total_size);

      for (const auto& chunk : chunks) {
        image_data.insert(image_data.end(), chunk.begin(), chunk.end());
      }
      return image_data;
    }
  };

}  // namespace network