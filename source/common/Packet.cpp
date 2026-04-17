/**
 * @file Packet.cpp
 * @brief Implements packet serialization and deserialization helpers.
 */

#include <common/Packet.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace network {

  namespace {

    std::string bytesToString(const uint8_t* data, size_t length) {
      std::string result;
      result.resize(length);
      for (size_t i = 0; i < length; ++i) {
        result[i] = static_cast<char>(data[i]);
      }
      return result;
    }

  }  // namespace

  std::vector<uint8_t> serializePacket(PacketType type, const void* payload, size_t payload_size) {
    PacketHeader header;
    header.magic = PACKET_MAGIC;
    header.type = type;
    header.payload_size = static_cast<uint32_t>(payload_size);
    header.sequence = 0;  // will be incremented per connection
    header.checksum = 0;  // temporary

    // Compute CRC over header (with checksum zero) + payload
    header.checksum = computePacketChecksum(header, payload, payload_size);

    std::vector<uint8_t> result(sizeof(PacketHeader) + payload_size);
    std::memcpy(result.data(), &header, sizeof(PacketHeader));
    if (payload && payload_size) {
      std::memcpy(result.data() + sizeof(PacketHeader), payload, payload_size);
    }
    return result;
  }

  uint32_t computePacketChecksum(const PacketHeader& header, const void* payload,
                                 size_t payload_size) {
    Crc32 crc;
    // Hash header (excluding checksum field)
    PacketHeader header_copy = header;
    header_copy.checksum = 0;
    crc.update(&header_copy, sizeof(PacketHeader));
    if (payload && payload_size) {
      crc.update(payload, payload_size);
    }
    return crc.finalize();
  }

  bool deserializePacket(const std::vector<uint8_t>& data, PacketHeader& header,
                         std::vector<uint8_t>& payload) {
    if (data.size() < sizeof(PacketHeader)) {
      spdlog::warn("Packet too short: {} bytes", data.size());
      return false;
    }
    std::memcpy(&header, data.data(), sizeof(PacketHeader));

    // Verify magic
    if (header.magic != PACKET_MAGIC) {
      spdlog::warn("Invalid packet magic: 0x{:08X}", header.magic);
      return false;
    }

    // Verify payload size matches
    if (data.size() != sizeof(PacketHeader) + header.payload_size) {
      spdlog::warn("Packet size mismatch: expected {}, got {}",
                   sizeof(PacketHeader) + header.payload_size, data.size());
      return false;
    }

    // Extract payload
    payload.resize(header.payload_size);
    if (header.payload_size > 0) {
      std::memcpy(payload.data(), data.data() + sizeof(PacketHeader), header.payload_size);
    }

    // Verify CRC
    uint32_t expected_crc = header.checksum;
    uint32_t computed_crc = computePacketChecksum(header, payload.data(), payload.size());
    if (expected_crc != computed_crc) {
      spdlog::warn("CRC mismatch: expected 0x{:08X}, computed 0x{:08X}", expected_crc,
                   computed_crc);
      return false;
    }

    return true;
  }

  std::vector<uint8_t> serializeDiagnosticDataPayload(
      const std::vector<DiagnosticFaultCode>& faults) {
    constexpr size_t kCountSize = sizeof(uint16_t);
    std::vector<uint8_t> payload;
    payload.reserve(kCountSize + faults.size() * sizeof(DiagnosticFaultCodeHeader));

    const auto capped_count = static_cast<uint16_t>(
        std::min<size_t>(faults.size(), std::numeric_limits<uint16_t>::max()));
    payload.resize(kCountSize);
    std::memcpy(payload.data(), &capped_count, sizeof(capped_count));

    for (size_t i = 0; i < capped_count; ++i) {
      const auto& fault = faults[i];
      const auto desc_size = static_cast<uint16_t>(
          std::min<size_t>(fault.description.size(), std::numeric_limits<uint16_t>::max()));

      DiagnosticFaultCodeHeader header{
          fault.code,
          fault.timestamp_epoch_ms,
          fault.severity,
          desc_size,
      };

      const auto start = payload.size();
      payload.resize(start + sizeof(header) + desc_size);
      std::memcpy(payload.data() + start, &header, sizeof(header));
      if (desc_size > 0) {
        std::memcpy(payload.data() + start + sizeof(header), fault.description.data(), desc_size);
      }
    }

    return payload;
  }

  bool deserializeDiagnosticDataPayload(const std::vector<uint8_t>& payload,
                                        std::vector<DiagnosticFaultCode>& faults) {
    faults.clear();
    constexpr size_t kCountSize = sizeof(uint16_t);
    if (payload.size() < kCountSize) {
      return false;
    }

    uint16_t count = 0;
    std::memcpy(&count, payload.data(), sizeof(count));

    size_t offset = kCountSize;
    faults.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
      if (offset + sizeof(DiagnosticFaultCodeHeader) > payload.size()) {
        return false;
      }

      DiagnosticFaultCodeHeader wire{};
      std::memcpy(&wire, payload.data() + offset, sizeof(wire));
      offset += sizeof(wire);

      if (offset + wire.description_size > payload.size()) {
        return false;
      }

      DiagnosticFaultCode parsed{};
      parsed.code = wire.code;
      parsed.timestamp_epoch_ms = wire.timestamp_epoch_ms;
      parsed.severity = wire.severity;
      parsed.description
          = bytesToString(payload.data() + offset, static_cast<size_t>(wire.description_size));
      offset += wire.description_size;

      faults.push_back(std::move(parsed));
    }

    return offset == payload.size();
  }

  std::vector<uint8_t> serializeWarrantyDataPayload(const common::WarrantyInfo& warranty) {
    const auto expiry_size = static_cast<uint16_t>(
        std::min<size_t>(warranty.expiryDate.size(), std::numeric_limits<uint16_t>::max()));
    const auto provider_size = static_cast<uint16_t>(
        std::min<size_t>(warranty.provider.size(), std::numeric_limits<uint16_t>::max()));

    std::vector<uint8_t> payload;
    payload.reserve(sizeof(uint8_t) + sizeof(uint16_t) + expiry_size + sizeof(uint16_t)
                    + provider_size);

    const uint8_t is_active = warranty.isActive ? 1U : 0U;
    payload.push_back(is_active);

    payload.resize(payload.size() + sizeof(expiry_size));
    std::memcpy(payload.data() + 1, &expiry_size, sizeof(expiry_size));

    if (expiry_size > 0) {
      const auto start = payload.size();
      payload.resize(start + expiry_size);
      std::memcpy(payload.data() + start, warranty.expiryDate.data(), expiry_size);
    }

    const auto provider_size_offset = payload.size();
    payload.resize(payload.size() + sizeof(provider_size));
    std::memcpy(payload.data() + provider_size_offset, &provider_size, sizeof(provider_size));

    if (provider_size > 0) {
      const auto start = payload.size();
      payload.resize(start + provider_size);
      std::memcpy(payload.data() + start, warranty.provider.data(), provider_size);
    }

    return payload;
  }

  bool deserializeWarrantyDataPayload(const std::vector<uint8_t>& payload,
                                      common::WarrantyInfo& warranty) {
    if (payload.size() < sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t)) {
      return false;
    }

    size_t offset = 0;
    warranty.isActive = payload[offset] != 0;
    offset += sizeof(uint8_t);

    uint16_t expiry_size = 0;
    std::memcpy(&expiry_size, payload.data() + offset, sizeof(expiry_size));
    offset += sizeof(expiry_size);

    if (offset + expiry_size > payload.size()) {
      return false;
    }
    warranty.expiryDate = bytesToString(payload.data() + offset, static_cast<size_t>(expiry_size));
    offset += expiry_size;

    if (offset + sizeof(uint16_t) > payload.size()) {
      return false;
    }

    uint16_t provider_size = 0;
    std::memcpy(&provider_size, payload.data() + offset, sizeof(provider_size));
    offset += sizeof(provider_size);

    if (offset + provider_size > payload.size()) {
      return false;
    }
    warranty.provider = bytesToString(payload.data() + offset, static_cast<size_t>(provider_size));
    offset += provider_size;

    return offset == payload.size();
  }

  std::vector<std::vector<uint8_t>> serializeImagePayload(uint32_t image_id,
                                                          const std::vector<uint8_t>& image_data,
                                                          ImageFormat format) {
    std::vector<std::vector<uint8_t>> result;

    if (image_data.empty()) {
      spdlog::warn("Cannot serialize empty image");
      return result;
    }

    // Calculate chunk count and validate
    const size_t kMaxDataPerChunk = kMaxImageChunkPayloadSize;
    const size_t total_chunks = (image_data.size() + kMaxDataPerChunk - 1) / kMaxDataPerChunk;

    if (total_chunks > std::numeric_limits<uint16_t>::max()) {
      spdlog::error("Image too large: requires {} chunks, max is {}", total_chunks,
                    std::numeric_limits<uint16_t>::max());
      return result;
    }

    const auto total_chunks_u16 = static_cast<uint16_t>(total_chunks);
    const uint32_t image_crc32 = Crc32::calculate(image_data.data(), image_data.size());

    // Serialize each chunk
    size_t bytes_serialized = 0;
    for (uint16_t chunk_index = 0; chunk_index < total_chunks_u16; ++chunk_index) {
      const size_t chunk_start = bytes_serialized;
      const size_t chunk_end = std::min(bytes_serialized + kMaxDataPerChunk, image_data.size());
      const size_t chunk_size = chunk_end - chunk_start;
      const uint32_t chunk_crc32 = Crc32::calculate(image_data.data() + chunk_start, chunk_size);

      ImageChunkHeader header{
          image_id, chunk_index, total_chunks_u16, static_cast<uint32_t>(chunk_size),
          format,   chunk_crc32, image_crc32,
      };

      std::vector<uint8_t> chunk_payload(sizeof(header) + chunk_size);
      std::memcpy(chunk_payload.data(), &header, sizeof(header));
      if (chunk_size > 0) {
        std::memcpy(chunk_payload.data() + sizeof(header), image_data.data() + chunk_start,
                    chunk_size);
      }

      result.push_back(std::move(chunk_payload));
      bytes_serialized = chunk_end;
    }

    spdlog::info("Serialized image {} into {} chunks ({} bytes total)", image_id, total_chunks_u16,
                 image_data.size());
    return result;
  }

  bool deserializeImageChunk(const std::vector<uint8_t>& payload, ImageChunkHeader& header_out,
                             std::vector<uint8_t>& chunk_data_out) {
    chunk_data_out.clear();

    if (payload.size() < sizeof(ImageChunkHeader)) {
      spdlog::warn("Image chunk payload too short: {} bytes", payload.size());
      return false;
    }

    std::memcpy(&header_out, payload.data(), sizeof(ImageChunkHeader));

    // Validate header
    if (header_out.chunk_index >= header_out.total_chunks) {
      spdlog::warn("Invalid chunk index: {} >= {}", header_out.chunk_index,
                   header_out.total_chunks);
      return false;
    }

    const size_t expected_chunk_size = sizeof(ImageChunkHeader) + header_out.chunk_data_size;
    if (payload.size() != expected_chunk_size) {
      spdlog::warn("Chunk payload size mismatch: expected {}, got {}", expected_chunk_size,
                   payload.size());
      return false;
    }

    // Extract chunk data
    chunk_data_out.resize(header_out.chunk_data_size);
    if (header_out.chunk_data_size > 0) {
      std::memcpy(chunk_data_out.data(), payload.data() + sizeof(ImageChunkHeader),
                  header_out.chunk_data_size);
    }

    const uint32_t computed_chunk_crc
        = Crc32::calculate(chunk_data_out.data(), chunk_data_out.size());
    if (computed_chunk_crc != header_out.chunk_crc32) {
      spdlog::warn("Chunk CRC mismatch for image {} chunk {}: expected 0x{:08X}, computed 0x{:08X}",
                   header_out.image_id, header_out.chunk_index, header_out.chunk_crc32,
                   computed_chunk_crc);
      return false;
    }

    return true;
  }

}  // namespace network
