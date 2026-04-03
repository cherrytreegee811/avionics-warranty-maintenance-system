#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <common/Packet.h>
#include <doctest/doctest.h>

#include <cstring>

using namespace network;

// ============================================================================
// REQ-SYS-010: All data transferred between Client and Server shall use a pre‑defined structure.
// REQ-SYS-030: The data packet structure shall contain at least 1 dynamically allocated element.
// REQ-NET-031: The packet must dynamically allocate memory for the list of diagnostic codes.
// REQ-NET-013: The client and the server shall support internal processing logic for reading
//              received information, and serialize information for transfer.
// ============================================================================

TEST_CASE(
    "REQ-SYS-010/REQ-SYS-030/REQ-NET-031/REQ-NET-013: VerificationRequest "
    "serialization/deserialization") {
  VerificationRequest original{0x12345678, 0xABCDEF1234567890ULL};
  auto packet = serializePacket(PacketType::VERIFICATION_REQUEST, original);

  CHECK(packet.size() == sizeof(PacketHeader) + sizeof(VerificationRequest));

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(deserializePacket(packet, header, payload));

  CHECK(header.magic == PACKET_MAGIC);
  CHECK(header.type == PacketType::VERIFICATION_REQUEST);
  CHECK(header.payload_size == sizeof(VerificationRequest));
  // CRC should be non-zero
  CHECK(header.checksum != 0);

  VerificationRequest deserialized;
  std::memcpy(&deserialized, payload.data(), sizeof(deserialized));
  CHECK(deserialized.challenge == original.challenge);
  CHECK(deserialized.timestamp == original.timestamp);
}

TEST_CASE("REQ-SYS-010/REQ-NET-013: VerificationResponse serialization/deserialization") {
  VerificationResponse original{0xDEADBEEF, 12345ULL};
  auto packet = serializePacket(PacketType::VERIFICATION_RESPONSE, original);

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(deserializePacket(packet, header, payload));

  CHECK(header.type == PacketType::VERIFICATION_RESPONSE);
  CHECK(header.payload_size == sizeof(VerificationResponse));

  VerificationResponse deserialized;
  std::memcpy(&deserialized, payload.data(), sizeof(deserialized));
  CHECK(deserialized.challenge_response == original.challenge_response);
  CHECK(deserialized.client_id == original.client_id);
}

TEST_CASE("REQ-NET-013: StateChangeRequest serialization/deserialization") {
  StateChangeRequest original{StateId::DIAGNOSTIC};
  auto packet = serializePacket(PacketType::STATE_CHANGE, original);

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(deserializePacket(packet, header, payload));
  CHECK(header.type == PacketType::STATE_CHANGE);
  CHECK(header.payload_size == sizeof(StateChangeRequest));

  StateChangeRequest parsed{};
  std::memcpy(&parsed, payload.data(), sizeof(parsed));
  CHECK(parsed.target_state == StateId::DIAGNOSTIC);
}

TEST_CASE("REQ-SYS-030/REQ-NET-031: Diagnostic payload roundtrip") {
  std::vector<DiagnosticFaultCode> original{
      {101, 1710000000000LL, "Engine temperature sensor fault"},
      {202, 1710000001000LL, "Hydraulic pressure low"}};

  const auto payload = serializeDiagnosticDataPayload(original);

  std::vector<DiagnosticFaultCode> parsed;
  CHECK(deserializeDiagnosticDataPayload(payload, parsed));
  REQUIRE(parsed.size() == original.size());
  CHECK(parsed[0].code == original[0].code);
  CHECK(parsed[0].timestamp_epoch_ms == original[0].timestamp_epoch_ms);
  CHECK(parsed[0].description == original[0].description);
  CHECK(parsed[1].code == original[1].code);
  CHECK(parsed[1].timestamp_epoch_ms == original[1].timestamp_epoch_ms);
  CHECK(parsed[1].description == original[1].description);
}

TEST_CASE("REQ-NET-012: Diagnostic payload rejects malformed buffer") {
  std::vector<uint8_t> malformed;
  const uint16_t count = 1;
  malformed.resize(sizeof(count));
  std::memcpy(malformed.data(), &count, sizeof(count));

  std::vector<DiagnosticFaultCode> parsed;
  CHECK(!deserializeDiagnosticDataPayload(malformed, parsed));
}

// ============================================================================
// REQ-NET-012: The packet shall contain packet integrity checks to ensure validity
//              and authenticity of the information.
// ============================================================================

TEST_CASE("REQ-NET-012: Packet with wrong magic fails deserialization") {
  VerificationRequest req{0x123, 0x456};
  auto packet = serializePacket(PacketType::VERIFICATION_REQUEST, req);
  // corrupt magic
  PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet.data());
  hdr->magic = 0xFFFFFFFF;

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(!deserializePacket(packet, header, payload));
}

TEST_CASE("REQ-NET-012: Packet with corrupted CRC fails deserialization") {
  VerificationRequest req{0x123, 0x456};
  auto packet = serializePacket(PacketType::VERIFICATION_REQUEST, req);
  // corrupt a byte in payload
  packet[sizeof(PacketHeader)] ^= 0xFF;

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(!deserializePacket(packet, header, payload));
}

TEST_CASE("REQ-NET-012: Packet with wrong size fails deserialization") {
  VerificationRequest req{0x123, 0x456};
  auto packet = serializePacket(PacketType::VERIFICATION_REQUEST, req);
  packet.pop_back();  // truncate

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(!deserializePacket(packet, header, payload));
}