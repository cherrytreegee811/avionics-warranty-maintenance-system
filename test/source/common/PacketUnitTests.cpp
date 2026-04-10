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

TEST_CASE("REQ-NET-013: DiagnosticCodeClearRequest serialization/deserialization") {
  DiagnosticCodeClearRequest original{101};
  auto packet = serializePacket(PacketType::CLEAR_DIAGNOSTIC_CODE, original);

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(deserializePacket(packet, header, payload));
  CHECK(header.type == PacketType::CLEAR_DIAGNOSTIC_CODE);
  CHECK(header.payload_size == sizeof(DiagnosticCodeClearRequest));

  DiagnosticCodeClearRequest parsed{};
  std::memcpy(&parsed, payload.data(), sizeof(parsed));
  CHECK(parsed.code == original.code);
}

TEST_CASE("REQ-NET-013: DiagnosticCodeClearConfirmation serialization/deserialization") {
  DiagnosticCodeClearConfirmation original{203, DiagnosticCodeClearStatus::SUCCESS,
                                           StateId::MAINTENANCE};
  auto packet = serializePacket(PacketType::CLEAR_DIAGNOSTIC_CODE_CONFIRMATION, original);

  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(deserializePacket(packet, header, payload));
  CHECK(header.type == PacketType::CLEAR_DIAGNOSTIC_CODE_CONFIRMATION);
  CHECK(header.payload_size == sizeof(DiagnosticCodeClearConfirmation));

  DiagnosticCodeClearConfirmation parsed{};
  std::memcpy(&parsed, payload.data(), sizeof(parsed));
  CHECK(parsed.code == original.code);
  CHECK(parsed.status == DiagnosticCodeClearStatus::SUCCESS);
  CHECK(parsed.resulting_state == StateId::MAINTENANCE);
}

TEST_CASE("REQ-SYS-030/REQ-NET-031: Diagnostic payload roundtrip") {
  std::vector<DiagnosticFaultCode> original{
      {101, 1710000000000LL, DiagnosticFaultSeverity::MINOR, "Engine temperature sensor fault"},
      {202, 1710000001000LL, DiagnosticFaultSeverity::MAJOR, "Hydraulic pressure low"}};

  const auto payload = serializeDiagnosticDataPayload(original);

  std::vector<DiagnosticFaultCode> parsed;
  CHECK(deserializeDiagnosticDataPayload(payload, parsed));
  REQUIRE(parsed.size() == original.size());
  CHECK(parsed[0].code == original[0].code);
  CHECK(parsed[0].timestamp_epoch_ms == original[0].timestamp_epoch_ms);
  CHECK(parsed[0].severity == DiagnosticFaultSeverity::MINOR);
  CHECK(parsed[0].description == original[0].description);
  CHECK(parsed[1].code == original[1].code);
  CHECK(parsed[1].timestamp_epoch_ms == original[1].timestamp_epoch_ms);
  CHECK(parsed[1].severity == DiagnosticFaultSeverity::MAJOR);
  CHECK(parsed[1].description == original[1].description);
}

TEST_CASE("REQ-NET-012: Diagnostic payload rejects malformed buffer") {
  std::vector<uint8_t> malformed;
  const uint16_t count = 1;
  malformed.resize(sizeof(count));
  std::memcpy(malformed.data(), &count, sizeof(count));

  std::vector<DiagnosticFaultCode> parsed;
  CHECK(!deserializeDiagnosticDataPayload(malformed, parsed));

  malformed.clear();
  malformed.resize(sizeof(uint16_t) + sizeof(DiagnosticFaultCodeHeader) - 1);
  std::memcpy(malformed.data(), &count, sizeof(count));
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

// ============================================================================
// REQ-CLT-005: The airplane should tell the MMA when it has landed.
// ============================================================================
TEST_CASE("REQ-CLT-005: LANDED packet serialization/deserialization") {
  auto packet = serializePacket(PacketType::LANDED_NOTIFICATION, nullptr, 0);
  PacketHeader header;
  std::vector<uint8_t> payload;
  CHECK(deserializePacket(packet, header, payload));
  CHECK(header.type == PacketType::LANDED_NOTIFICATION);
  CHECK(header.payload_size == 0);
  CHECK(payload.empty());
}

// ============================================================================
// Image packet tests - Single chunk, multi-chunk, reassembly
// ============================================================================

TEST_CASE("Image packet: Single chunk serialization/deserialization") {
  // Create a small test image (1KB)
  std::vector<uint8_t> image_data(1024);
  for (size_t i = 0; i < image_data.size(); ++i) {
    image_data[i] = static_cast<uint8_t>(i % 256);
  }

  // Serialize
  const auto chunks = serializeImagePayload(123, image_data, ImageFormat::PNG);
  CHECK(chunks.size() == 1);  // Single chunk

  // Deserialize
  ImageChunkHeader header;
  std::vector<uint8_t> chunk_data;
  CHECK(deserializeImageChunk(chunks[0], header, chunk_data));

  CHECK(header.image_id == 123);
  CHECK(header.chunk_index == 0);
  CHECK(header.total_chunks == 1);
  CHECK(header.format == ImageFormat::PNG);
  CHECK(header.chunk_crc32 == Crc32::calculate(image_data.data(), image_data.size()));
  CHECK(header.image_crc32 == Crc32::calculate(image_data.data(), image_data.size()));
  CHECK(chunk_data == image_data);
}

TEST_CASE("Image packet: Multi-chunk image serialization") {
  // Create a large test image (2.5 MB - spans 3+ chunks)
  const size_t image_size = 2621440;  // 2.5 MB
  std::vector<uint8_t> large_image(image_size);
  for (size_t i = 0; i < large_image.size(); ++i) {
    large_image[i] = static_cast<uint8_t>(i % 256);
  }

  // Serialize
  const auto chunks = serializeImagePayload(456, large_image, ImageFormat::JPEG);
  CHECK(chunks.size() >= 3);  // Multiple chunks

  // Deserialize each chunk and verify structure
  for (size_t i = 0; i < chunks.size(); ++i) {
    ImageChunkHeader header;
    std::vector<uint8_t> chunk_data;
    CHECK(deserializeImageChunk(chunks[i], header, chunk_data));

    CHECK(header.image_id == 456);
    CHECK(header.chunk_index == static_cast<uint16_t>(i));
    CHECK(header.total_chunks == static_cast<uint16_t>(chunks.size()));
    CHECK(header.format == ImageFormat::JPEG);
    CHECK(header.chunk_data_size == chunk_data.size());
    CHECK(header.chunk_crc32 == Crc32::calculate(chunk_data.data(), chunk_data.size()));
    CHECK(header.image_crc32 == Crc32::calculate(large_image.data(), large_image.size()));
  }
}

TEST_CASE("Image packet: ImageBuffer add single chunk") {
  ImageBuffer buffer(789, ImageFormat::RAW, 1);
  std::vector<uint8_t> data{0xAA, 0xBB, 0xCC};

  CHECK(!buffer.isComplete());
  const bool complete = buffer.addChunk(0, data);
  CHECK(complete);  // Single chunk image complete after adding one chunk
  CHECK(buffer.isComplete());

  const auto reassembled = buffer.reassemble();
  CHECK(reassembled == data);

  CHECK(buffer.setExpectedImageCrc(Crc32::calculate(data.data(), data.size())));
  CHECK(buffer.validateReassembledCrc(reassembled));
}

TEST_CASE("Image packet: ImageBuffer multi-chunk reassembly") {
  const size_t total_chunks = 5;
  ImageBuffer buffer(111, ImageFormat::PNG, total_chunks);

  // Create test data chunks
  std::vector<std::vector<uint8_t>> chunks(total_chunks);
  for (size_t i = 0; i < total_chunks; ++i) {
    chunks[i].resize(1000 + i * 100);
    for (size_t j = 0; j < chunks[i].size(); ++j) {
      chunks[i][j] = static_cast<uint8_t>((i * 256 + j) % 256);
    }
  }

  // Add chunks out of order (to test robustness)
  CHECK(!buffer.addChunk(2, chunks[2]));
  CHECK(!buffer.addChunk(0, chunks[0]));
  CHECK(!buffer.addChunk(4, chunks[4]));
  CHECK(!buffer.addChunk(1, chunks[1]));
  const bool complete = buffer.addChunk(3, chunks[3]);
  CHECK(complete);  // All chunks added, should be complete

  // Verify reassembly
  const auto reassembled = buffer.reassemble();
  size_t offset = 0;
  for (size_t i = 0; i < total_chunks; ++i) {
    for (size_t j = 0; j < chunks[i].size(); ++j) {
      CHECK(reassembled[offset + j] == chunks[i][j]);
    }
    offset += chunks[i].size();
  }
}

TEST_CASE("Image packet: ImageBuffer duplicate chunk handling") {
  ImageBuffer buffer(222, ImageFormat::PNG, 3);
  std::vector<uint8_t> chunk_data{0x01, 0x02, 0x03};

  // Add chunk once
  CHECK(!buffer.addChunk(0, chunk_data));
  CHECK(buffer.received[0]);

  // Try to add same chunk again (should just update isComplete check)
  const auto chunk_data2 = std::vector<uint8_t>{0xFF, 0xFF, 0xFF};  // Different data
  CHECK(!buffer.addChunk(0, chunk_data2));  // Duplicate chunk index, returns !isComplete()

  // Original data should still be there (we don't overwrite)
  CHECK(buffer.chunks[0] == chunk_data);

  // Now add remaining chunks
  buffer.addChunk(1, chunk_data);
  const bool complete = buffer.addChunk(2, chunk_data);
  CHECK(complete);
}

TEST_CASE("Image packet: ImageBuffer rejects invalid chunk index") {
  ImageBuffer buffer(333, ImageFormat::JPEG, 3);
  std::vector<uint8_t> data{0xAA, 0xBB};

  CHECK(!buffer.addChunk(0, data));   // Valid index 0
  CHECK(!buffer.addChunk(3, data));   // Invalid: index >= total_chunks, returns false
  CHECK(!buffer.addChunk(10, data));  // Invalid: far out of range
}

TEST_CASE("Image packet: Multi-chunk full round trip") {
  // Create test image
  const size_t test_size = 5 * 1024 * 1024;  // 5 MB
  std::vector<uint8_t> original_image(test_size);
  for (size_t i = 0; i < original_image.size(); ++i) {
    original_image[i] = static_cast<uint8_t>(i % 256);
  }

  // Serialize
  const auto chunks = serializeImagePayload(999, original_image, ImageFormat::JPEG);
  CHECK(!chunks.empty());

  // Reassemble using ImageBuffer
  ImageBuffer buffer(999, ImageFormat::JPEG, chunks.size());
  for (size_t i = 0; i < chunks.size(); ++i) {
    ImageChunkHeader header;
    std::vector<uint8_t> chunk_data;
    CHECK(deserializeImageChunk(chunks[i], header, chunk_data));

    const bool complete = buffer.addChunk(i, chunk_data);
    if (i < chunks.size() - 1) {
      CHECK(!complete);
    } else {
      CHECK(complete);
    }
  }

  // Verify reassembled data matches original
  const auto reassembled = buffer.reassemble();
  CHECK(reassembled == original_image);

  CHECK(buffer.setExpectedImageCrc(Crc32::calculate(original_image.data(), original_image.size())));
  CHECK(buffer.validateReassembledCrc(reassembled));
}

TEST_CASE("Image packet: Boundary - exactly 1MB chunk size") {
  // Create image that's exactly one chunk
  const std::vector<uint8_t> image(kMaxImageChunkPayloadSize, 0xAA);

  const auto chunks = serializeImagePayload(444, image, ImageFormat::RAW);
  CHECK(chunks.size() == 1);

  ImageChunkHeader header;
  std::vector<uint8_t> chunk_data;
  CHECK(deserializeImageChunk(chunks[0], header, chunk_data));
  CHECK(chunk_data.size() == kMaxImageChunkPayloadSize);
}

TEST_CASE("Image packet: Boundary - one byte over 1MB") {
  // Create image just over one chunk
  const size_t oversized = kMaxImageChunkPayloadSize + 1;
  const std::vector<uint8_t> image(oversized, 0xBB);

  const auto chunks = serializeImagePayload(555, image, ImageFormat::PNG);
  CHECK(chunks.size() == 2);

  // Verify first chunk is full, second has one byte
  ImageChunkHeader header1;
  std::vector<uint8_t> chunk_data1;
  CHECK(deserializeImageChunk(chunks[0], header1, chunk_data1));
  CHECK(chunk_data1.size() == kMaxImageChunkPayloadSize);

  ImageChunkHeader header2;
  std::vector<uint8_t> chunk_data2;
  CHECK(deserializeImageChunk(chunks[1], header2, chunk_data2));
  CHECK(chunk_data2.size() == 1);
}

TEST_CASE("Image packet: Empty image serialization fails gracefully") {
  const std::vector<uint8_t> empty_image;
  const auto chunks = serializeImagePayload(666, empty_image, ImageFormat::PNG);
  CHECK(chunks.empty());
}

TEST_CASE("Image packet: Malformed chunk deserialization fails") {
  // Payload too short for header
  std::vector<uint8_t> malformed(10);
  ImageChunkHeader header;
  std::vector<uint8_t> chunk_data;
  CHECK(!deserializeImageChunk(malformed, header, chunk_data));

  // Payload with wrong size
  ImageChunkHeader hdr{100, 0, 1, 500, ImageFormat::PNG, 0, 0};
  std::vector<uint8_t> invalid_payload(sizeof(hdr) + 100);  // Says 500 bytes but has 100
  std::memcpy(invalid_payload.data(), &hdr, sizeof(hdr));
  CHECK(!deserializeImageChunk(invalid_payload, header, chunk_data));
}

TEST_CASE("Image packet: Corrupted chunk CRC is rejected") {
  std::vector<uint8_t> image_data(512, 0x5A);
  auto chunks = serializeImagePayload(777, image_data, ImageFormat::PNG);
  REQUIRE(chunks.size() == 1);

  // Corrupt one payload byte after the chunk header.
  chunks[0][sizeof(ImageChunkHeader)] ^= 0xFF;

  ImageChunkHeader header;
  std::vector<uint8_t> chunk_data;
  CHECK(!deserializeImageChunk(chunks[0], header, chunk_data));
}
