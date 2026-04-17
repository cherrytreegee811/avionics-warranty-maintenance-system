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
#include <string_view>
#include <vector>

namespace network {

  /** @brief Magic constant used to validate packet framing. */
  constexpr uint32_t PACKET_MAGIC = 0xABCD1234U;

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
   * @param type Type: @ref network::PacketType. Packet type discriminator.
   * @param payload Type: const void*. Pointer to payload bytes.
   * @param payload_size Type: size_t. Number of payload bytes.
   * @return Type: std::vector<uint8_t>. Serialized packet bytes including header.
   */
  std::vector<uint8_t> serializePacket(PacketType type, const void* payload, size_t payload_size);
  /**
   * @brief Convenience wrapper that serializes a POD payload object.
   * @param type Type: @ref network::PacketType. Packet type discriminator.
   * @param payload Type: const T&. POD payload object.
   * @return Type: std::vector<uint8_t>. Serialized packet bytes including header.
   */
  template <typename T> std::vector<uint8_t> serializePacket(PacketType type, const T& payload) {
    return serializePacket(type, &payload, sizeof(T));
  }

  /**
   * @brief Serializes fault records into a diagnostic payload buffer.
   * @param faults Type: const std::vector<@ref network::DiagnosticFaultCode>&. Fault records to
   * serialize.
   * @return Type: std::vector<uint8_t>. Serialized diagnostic payload bytes.
   */
  std::vector<uint8_t> serializeDiagnosticDataPayload(
      const std::vector<DiagnosticFaultCode>& faults);
  /**
   * @brief Parses a diagnostic payload buffer into fault records.
   * @param payload Type: const std::vector<uint8_t>&. Serialized diagnostic payload bytes.
   * @param faults Type: std::vector<@ref network::DiagnosticFaultCode>&. Output vector populated
   * with parsed fault records.
   * @return Type: bool. True if payload is valid and parsed successfully.
   */
  bool deserializeDiagnosticDataPayload(const std::vector<uint8_t>& payload,
                                        std::vector<DiagnosticFaultCode>& faults);

  /**
   * @brief Serializes warranty data into payload bytes.
   * @param warranty Type: const @ref common::WarrantyInfo&. Warranty information to encode.
   * @return Type: std::vector<uint8_t>. Serialized warranty payload bytes.
   */
  std::vector<uint8_t> serializeWarrantyDataPayload(const common::WarrantyInfo& warranty);
  /**
   * @brief Parses payload bytes into a warranty structure.
   * @param payload Type: const std::vector<uint8_t>&. Serialized warranty payload bytes.
   * @param warranty Type: @ref common::WarrantyInfo&. Output warranty structure.
   * @return Type: bool. True if payload is valid and parsed successfully.
   */
  bool deserializeWarrantyDataPayload(const std::vector<uint8_t>& payload,
                                      common::WarrantyInfo& warranty);

  /**
   * @brief Splits an image into serialized chunk payloads.
   * @param image_id Type: uint32_t. Identifier assigned to this image transfer.
   * @param image_data Type: const std::vector<uint8_t>&. Raw encoded image bytes.
   * @param format Type: @ref network::ImageFormat. Encoding format of image_data.
   * @return Type: std::vector<std::vector<uint8_t>>. One serialized payload per chunk.
   */
  std::vector<std::vector<uint8_t>> serializeImagePayload(uint32_t image_id,
                                                          const std::vector<uint8_t>& image_data,
                                                          ImageFormat format);

  /**
   * @brief Extracts image chunk header and data from a chunk payload.
   * @param payload Type: const std::vector<uint8_t>&. Serialized chunk payload bytes.
   * @param header_out Type: @ref network::ImageChunkHeader&. Output chunk header.
   * @param chunk_data_out Type: std::vector<uint8_t>&. Output raw chunk bytes.
   * @return Type: bool. True if payload is valid and parsed successfully.
   */
  bool deserializeImageChunk(const std::vector<uint8_t>& payload, ImageChunkHeader& header_out,
                             std::vector<uint8_t>& chunk_data_out);

  /**
   * @brief Validates and deserializes packet bytes into header and payload.
   * @param data Type: const std::vector<uint8_t>&. Serialized packet bytes.
   * @param header Type: @ref network::PacketHeader&. Output parsed packet header.
   * @param payload Type: std::vector<uint8_t>&. Output parsed packet payload.
   * @return Type: bool. True if packet framing and checksum are valid.
   */
  bool deserializePacket(const std::vector<uint8_t>& data, PacketHeader& header,
                         std::vector<uint8_t>& payload);

  /**
   * @brief Computes packet checksum for test verification and diagnostics.
   * @param header Type: const @ref network::PacketHeader&. Packet header with checksum field
   * ignored.
   * @param payload Type: const void*. Pointer to payload bytes.
   * @param payload_size Type: size_t. Number of payload bytes.
   * @return Type: uint32_t. CRC-32 checksum for packet header and payload.
   */
  uint32_t computePacketChecksum(const PacketHeader& header, const void* payload,
                                 size_t payload_size);

  /**
   * @brief Converts a state identifier to a human-readable string.
   * @param state Type: @ref network::StateId. State enum value.
   * @return Type: std::string_view. String representation of state.
   */
  inline constexpr std::string_view stateIdToString(StateId state) {
    std::string_view result = "UNKNOWN";
    switch (state) {
      case StateId::STANDBY:
        result = "STANDBY";
        break;
      case StateId::DIAGNOSTIC:
        result = "DIAGNOSTIC";
        break;
      case StateId::MAINTENANCE:
        result = "MAINTENANCE";
        break;
      case StateId::FAULT:
        result = "FAULT";
        break;
      default:
        // Unknown state id.
        break;
    }

    return result;
  }

  /**
   * @brief Converts diagnostic fault severity to a human-readable string.
   * @param severity Type: @ref network::DiagnosticFaultSeverity. Diagnostic severity value.
   * @return Type: std::string_view. String representation of severity.
   */
  inline constexpr std::string_view diagnosticFaultSeverityToString(
      DiagnosticFaultSeverity severity) {
    std::string_view result = "UNKNOWN";
    switch (severity) {
      case DiagnosticFaultSeverity::MINOR:
        result = "MINOR";
        break;
      case DiagnosticFaultSeverity::MAJOR:
        result = "MAJOR";
        break;
      default:
        // Unknown severity.
        break;
    }

    return result;
  }

  /**
   * @brief Converts image format enum to a human-readable string.
   * @param format Type: @ref network::ImageFormat. Image format value.
   * @return Type: std::string_view. String representation of image format.
   */
  inline constexpr std::string_view imageFormatToString(ImageFormat format) {
    std::string_view result = "UNKNOWN";
    switch (format) {
      case ImageFormat::RAW:
        result = "RAW";
        break;
      case ImageFormat::PNG:
        result = "PNG";
        break;
      case ImageFormat::JPEG:
        result = "JPEG";
        break;
      default:
        // Unknown image format.
        break;
    }

    return result;
  }

  /**
   * @brief Converts diagnostic code clear status to a human-readable string.
   * @param status Type: @ref network::DiagnosticCodeClearStatus. Diagnostic clear result value.
   * @return Type: std::string_view. String representation of status.
   */
  inline constexpr std::string_view diagnosticCodeClearStatusToString(
      DiagnosticCodeClearStatus status) {
    std::string_view result = "UNKNOWN";
    switch (status) {
      case DiagnosticCodeClearStatus::SUCCESS:
        result = "SUCCESS";
        break;
      case DiagnosticCodeClearStatus::REJECTED_NOT_IN_CLEARABLE_STATE:
        result = "REJECTED_NOT_IN_CLEARABLE_STATE";
        break;
      case DiagnosticCodeClearStatus::CODE_NOT_FOUND:
        result = "CODE_NOT_FOUND";
        break;
      case DiagnosticCodeClearStatus::MALFORMED_REQUEST:
        result = "MALFORMED_REQUEST";
        break;
      default:
        // Unknown status.
        break;
    }

    return result;
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

    /**
     * @brief Creates an empty reassembly buffer for a specific image transfer.
     * @param id Type: uint32_t. Identifier for this image transfer.
     * @param fmt Type: @ref network::ImageFormat. Encoding format for this image.
     * @param total Type: uint16_t. Total number of expected chunks.
     */
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

    /**
     * @brief Sets expected full-image CRC, validating consistency on repeated calls.
     * @param crc32 Type: uint32_t. Expected CRC-32 of full image.
     * @return Type: bool. True if value was set or matches the existing expectation.
     */
    bool setExpectedImageCrc(uint32_t crc32) {
      bool result = false;

      if (!expected_image_crc32_set) {
        expected_image_crc32 = crc32;
        expected_image_crc32_set = true;
        result = true;
      } else {
        result = (expected_image_crc32 == crc32);
      }

      return result;
    }

    /**
     * @brief Adds a chunk and returns true once all chunks are present.
     * @param chunk_index Type: uint16_t. Zero-based chunk index.
     * @param data Type: const std::vector<uint8_t>&. Raw chunk bytes.
     * @return Type: bool. True when the image is complete after this insert.
     */
    bool addChunk(uint16_t chunk_index, const std::vector<uint8_t>& data) {
      bool result = false;

      if (chunk_index < total_chunks) {
        if (received[chunk_index]) {
          result = isComplete();
        } else {
          chunks[chunk_index] = data;
          received[chunk_index] = true;
          total_size_bytes += data.size();
          result = isComplete();
        }
      } else {
        result = false;
      }

      return result;
    }

    /**
     * @brief Returns true when every chunk index has been received.
     * @return Type: bool. True when no chunk is missing.
     */
    bool isComplete() const {
      bool complete = true;

      for (bool r : received) {
        if (!r) {
          complete = false;
          break;
        }
      }

      return complete;
    }

    /**
     * @brief Concatenates chunk buffers into one complete image byte vector.
     * @return Type: std::vector<uint8_t>. Reassembled image bytes in chunk order.
     */
    std::vector<uint8_t> reassemble() const {
      std::vector<uint8_t> result;
      result.reserve(total_size_bytes);
      for (const auto& chunk : chunks) {
        (void)result.insert(result.end(), chunk.begin(), chunk.end());
      }
      return result;
    }

    /**
     * @brief Validates CRC of reassembled image data against expected checksum.
     * @param reassembled Type: const std::vector<uint8_t>&. Reassembled image bytes.
     * @return Type: bool. True if CRC matches the expected image checksum.
     */
    bool validateReassembledCrc(const std::vector<uint8_t>& reassembled) const {
      bool result = false;

      if (expected_image_crc32_set) {
        result = (Crc32::calculate(reassembled.data(), reassembled.size()) == expected_image_crc32);
      }

      return result;
    }
  };
}  // namespace network
