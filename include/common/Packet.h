#pragma once
/**
 * @file Packet.h
 * @brief Defines network packet schema, payload records, and serialization helpers.
 */

#include <common/Crc32.h>
#include <common/WarrantyData.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace network {

  /** @brief Magic constant used to validate packet framing. */
  constexpr uint32_t PACKET_MAGIC = 0xABCD1234;

  /** @brief Supported message types for client-server protocol packets. */
  enum class PacketType : uint8_t {
    VERIFICATION_REQUEST = 1,
    VERIFICATION_RESPONSE = 2,
    STATE_CHANGE = 3,
    LANDED_NOTIFICATION = 4,
    DIAGNOSTIC_DATA = 5,
    SCHEMATIC_CHUNK = 6,
    STATE_CHANGE_CONFIRMATION = 7,
    CLEAR_DIAGNOSTIC_CODE = 8,
    CLEAR_DIAGNOSTIC_CODE_CONFIRMATION = 9,
    SCHEMATIC_CHUNK_RETRY_REQUEST = 10,
    WARRANTY_DATA = 11
  };

  /** @brief Operational aircraft state identifiers used in state messages. */
  enum class StateId : uint8_t { STANDBY = 0, DIAGNOSTIC = 1, MAINTENANCE = 2, FAULT = 3 };

  /** @brief Severity levels for diagnostic fault records. */
  enum class DiagnosticFaultSeverity : uint8_t {
    MINOR = 0,
    MAJOR = 1,
  };

  /** @brief Image encoding used for chunked schematic transfer. */
  enum class ImageFormat : uint8_t {
    RAW = 0,   // Uncompressed pixel data
    PNG = 1,   // PNG compressed image
    JPEG = 2,  // JPEG compressed image
  };
  /** @brief Result of a diagnostic code clear request. */
  enum class DiagnosticCodeClearStatus : uint8_t {
    SUCCESS = 0,
    REJECTED_NOT_IN_CLEARABLE_STATE = 1,
    CODE_NOT_FOUND = 2,
    MALFORMED_REQUEST = 3,
  };

#pragma pack(push, 1)
  /** @brief Fixed packet framing header prepended to all payloads. */
  struct PacketHeader {
    uint32_t magic;
    PacketType type;
    uint32_t payload_size;
    uint32_t sequence;
    uint32_t checksum;  // CRC32 of header + payload (excluding this field)
  };

  /** @brief Challenge packet sent by MMA to aircraft. */
  struct VerificationRequest {
    uint32_t challenge;
    uint64_t timestamp;
  };

  /** @brief Challenge response sent by aircraft to MMA. */
  struct VerificationResponse {
    uint32_t challenge_response;
    uint64_t client_id;
  };

  /** @brief Request payload for a target state transition. */
  struct StateChangeRequest {
    StateId target_state;
  };

  /** @brief Confirmation payload for applied state transitions. */
  struct StateChangeConfirmation {
    StateId applied_state;
  };

  /** @brief Request payload for clearing a specific diagnostic code. */
  struct DiagnosticCodeClearRequest {
    int32_t code;
  };

  /** @brief Confirmation payload for a diagnostic code clear request. */
  struct DiagnosticCodeClearConfirmation {
    int32_t code;
    DiagnosticCodeClearStatus status;
    StateId resulting_state;
  };

  /** @brief Request payload for re-sending a missing image chunk. */
  struct SchematicChunkRetryRequest {
    uint32_t image_id;
    uint16_t chunk_index;
  };

  /** @brief Header used for one fault record inside diagnostic payloads. */
  struct DiagnosticFaultCodeHeader {
    int32_t code;
    int64_t timestamp_epoch_ms;
    DiagnosticFaultSeverity severity;
    uint16_t description_size;
  };

  /** @brief Header prepended to each image chunk payload. */
  struct ImageChunkHeader {
    uint32_t image_id;         // Unique identifier for image sequence
    uint16_t chunk_index;      // 0-based chunk number
    uint16_t total_chunks;     // Total chunks for this image
    uint32_t chunk_data_size;  // Bytes of image data in this chunk
    ImageFormat format;        // Image format (RAW, PNG, JPEG, etc.)
    uint32_t chunk_crc32;      // CRC32 of this chunk's raw data bytes
    uint32_t image_crc32;      // CRC32 of the full original image bytes
  };
#pragma pack(pop)

  /** @brief In-memory diagnostic fault record with string description. */
  struct DiagnosticFaultCode {
    int32_t code;
    int64_t timestamp_epoch_ms;
    DiagnosticFaultSeverity severity;
    std::string description;
  };

  /** @brief Maximum per-packet image payload size after chunk header bytes. */
  constexpr size_t kMaxImageChunkPayloadSize
      = 1024 * 1024 - sizeof(ImageChunkHeader);  // ~1 MB minus header
  /** @brief Upper bound for rolling image identifier counter. */
  constexpr uint32_t kMaxImageId = 0xFFFFFFFFU;

  /**
   * @brief Serializes a packet from raw payload bytes.
   * @param type Packet type discriminator.
   * @param payload Pointer to payload bytes.
   * @param payload_size Number of payload bytes.
   * @return Serialized packet bytes including header.
   */
  std::vector<uint8_t> serializePacket(PacketType type, const void* payload, size_t payload_size);
  /**
   * @brief Convenience wrapper that serializes a POD payload object.
   */
  template <typename T> std::vector<uint8_t> serializePacket(PacketType type, const T& payload) {
    return serializePacket(type, &payload, sizeof(T));
  }

  /** @brief Serializes fault records into a diagnostic payload buffer. */
  std::vector<uint8_t> serializeDiagnosticDataPayload(
      const std::vector<DiagnosticFaultCode>& faults);
  /** @brief Parses a diagnostic payload buffer into fault records. */
  bool deserializeDiagnosticDataPayload(const std::vector<uint8_t>& payload,
                                        std::vector<DiagnosticFaultCode>& faults);

  /** @brief Serializes warranty data into payload bytes. */
  std::vector<uint8_t> serializeWarrantyDataPayload(const common::WarrantyInfo& warranty);
  /** @brief Parses payload bytes into a warranty structure. */
  bool deserializeWarrantyDataPayload(const std::vector<uint8_t>& payload,
                                      common::WarrantyInfo& warranty);

  /**
   * @brief Splits an image into serialized chunk payloads.
   * @return One serialized payload per chunk.
   */
  std::vector<std::vector<uint8_t>> serializeImagePayload(uint32_t image_id,
                                                          const std::vector<uint8_t>& image_data,
                                                          ImageFormat format);

  /** @brief Extracts image chunk header and data from a chunk payload. */
  bool deserializeImageChunk(const std::vector<uint8_t>& payload, ImageChunkHeader& header_out,
                             std::vector<uint8_t>& chunk_data_out);

  /** @brief Validates and deserializes packet bytes into header and payload. */
  bool deserializePacket(const std::vector<uint8_t>& data, PacketHeader& header,
                         std::vector<uint8_t>& payload);

  /** @brief Computes packet checksum for test verification and diagnostics. */
  uint32_t computePacketChecksum(const PacketHeader& header, const void* payload,
                                 size_t payload_size);

  /** @brief Converts a state identifier to a human-readable string. */
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

  /** @brief Converts diagnostic fault severity to a human-readable string. */
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

  /** @brief Converts image format enum to a human-readable string. */
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

  /** @brief Converts diagnostic code clear status to a human-readable string. */
  inline constexpr std::string_view diagnosticCodeClearStatusToString(
      DiagnosticCodeClearStatus status) {
    switch (status) {
      case DiagnosticCodeClearStatus::SUCCESS:
        return "SUCCESS";
      case DiagnosticCodeClearStatus::REJECTED_NOT_IN_CLEARABLE_STATE:
        return "REJECTED_NOT_IN_CLEARABLE_STATE";
      case DiagnosticCodeClearStatus::CODE_NOT_FOUND:
        return "CODE_NOT_FOUND";
      case DiagnosticCodeClearStatus::MALFORMED_REQUEST:
        return "MALFORMED_REQUEST";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * @brief Stores chunked image reassembly state for one image identifier.
   */
  struct ImageBuffer {
    uint32_t image_id;
    ImageFormat format;
    uint16_t total_chunks;
    size_t total_size_bytes;
    uint32_t expected_image_crc32;
    bool expected_image_crc32_set;
    std::vector<std::vector<uint8_t>>
        chunks;                  // chunks[i] = data for chunk i, empty if not received
    std::vector<bool> received;  // received[i] = true if chunk i has been received

    /** @brief Creates an empty reassembly buffer for a specific image transfer. */
    ImageBuffer(uint32_t id, ImageFormat fmt, uint16_t total)
        : image_id(id),
          format(fmt),
          total_chunks(total),
          total_size_bytes(0),
          expected_image_crc32(0),
          expected_image_crc32_set(false) {
      chunks.resize(total);
      received.resize(total, false);
    }

    /** @brief Sets expected full-image CRC, validating consistency on repeated calls. */
    bool setExpectedImageCrc(uint32_t crc32) {
      if (!expected_image_crc32_set) {
        expected_image_crc32 = crc32;
        expected_image_crc32_set = true;
        return true;
      }
      return expected_image_crc32 == crc32;
    }

    /** @brief Adds a chunk and returns true once all chunks are present. */
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

    /** @brief Returns true when every chunk index has been received. */
    bool isComplete() const {
      for (bool r : received) {
        if (!r) return false;
      }
      return true;
    }

    /** @brief Concatenates chunk buffers into one complete image byte vector. */
    std::vector<uint8_t> reassemble() const {
      std::vector<uint8_t> result;
      result.reserve(total_size_bytes);
      for (const auto& chunk : chunks) {
        result.insert(result.end(), chunk.begin(), chunk.end());
      }
      return result;
    }

    /** @brief Validates CRC of reassembled image data against expected checksum. */
    bool validateReassembledCrc(const std::vector<uint8_t>& reassembled) const {
      if (!expected_image_crc32_set) {
        return false;
      }
      return Crc32::calculate(reassembled.data(), reassembled.size()) == expected_image_crc32;
    }
  };
}  // namespace network
