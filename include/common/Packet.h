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
    SCHEMATIC_CHUNK = 6
  };

  enum class StateId : uint8_t { STANDBY = 0, DIAGNOSTIC = 1, MAINTENANCE = 2, FAULT = 3 };

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

  struct DiagnosticFaultCodeHeader {
    int32_t code;
    int64_t timestamp_epoch_ms;
    uint16_t description_size;
  };
#pragma pack(pop)

  struct DiagnosticFaultCode {
    int32_t code;
    int64_t timestamp_epoch_ms;
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

  // Deserialization: returns true if valid (magic + CRC), extracts header and payload
  bool deserializePacket(const std::vector<uint8_t>& data, PacketHeader& header,
                         std::vector<uint8_t>& payload);

  // For testing
  uint32_t computePacketChecksum(const PacketHeader& header, const void* payload,
                                 size_t payload_size);

}  // namespace network