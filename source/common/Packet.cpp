#include <common/Packet.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <limits>
#include <utility>

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
      parsed.description.assign(reinterpret_cast<const char*>(payload.data() + offset),
                                wire.description_size);
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

    WarrantyDataHeader header{
        static_cast<uint8_t>(warranty.isActive ? 1 : 0),
        expiry_size,
        provider_size,
    };

    std::vector<uint8_t> payload(sizeof(header) + expiry_size + provider_size);
    std::memcpy(payload.data(), &header, sizeof(header));

    size_t offset = sizeof(header);
    if (expiry_size > 0) {
      std::memcpy(payload.data() + offset, warranty.expiryDate.data(), expiry_size);
      offset += expiry_size;
    }
    if (provider_size > 0) {
      std::memcpy(payload.data() + offset, warranty.provider.data(), provider_size);
    }

    return payload;
  }

  bool deserializeWarrantyDataPayload(const std::vector<uint8_t>& payload,
                                      common::WarrantyInfo& warranty) {
    if (payload.size() < sizeof(WarrantyDataHeader)) {
      return false;
    }

    WarrantyDataHeader header{};
    std::memcpy(&header, payload.data(), sizeof(header));

    const size_t required_size = sizeof(header) + static_cast<size_t>(header.expiry_date_size)
                                 + static_cast<size_t>(header.provider_size);
    if (payload.size() != required_size) {
      return false;
    }

    size_t offset = sizeof(header);
    warranty.isActive = (header.is_active == 1);
    warranty.expiryDate.assign(reinterpret_cast<const char*>(payload.data() + offset),
                               header.expiry_date_size);
    offset += header.expiry_date_size;
    warranty.provider.assign(reinterpret_cast<const char*>(payload.data() + offset),
                             header.provider_size);
    return true;
  }

}  // namespace network