#include <common/Packet.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace network {

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

}  // namespace network