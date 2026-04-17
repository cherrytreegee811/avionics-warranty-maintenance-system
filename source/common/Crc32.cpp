/**
 * @file Crc32.cpp
 * @brief Implements CRC-32 checksum calculations.
 */

#include <common/Crc32.h>

#include <cstddef>
#include <cstdint>

namespace network {

  namespace {
    constexpr uint32_t kCrc32Polynomial = 0xEDB88320U;
  }  // namespace

  Crc32::Crc32() : m_crc(0xFFFFFFFFU) {}

  void Crc32::update(std::span<const std::byte> data) {
    for (const std::byte byte_value : data) {
      m_crc ^= static_cast<uint32_t>(std::to_integer<uint8_t>(byte_value));
      for (uint32_t bit = 0U; bit < 8U; ++bit) {
        if ((m_crc & 1U) != 0U) {
          m_crc = (m_crc >> 1U) ^ kCrc32Polynomial;
        } else {
          m_crc >>= 1U;
        }
      }
    }
  }

  uint32_t Crc32::finalize() { return ~m_crc; }

  void Crc32::reset() { m_crc = 0xFFFFFFFFU; }

  uint32_t Crc32::calculate(std::span<const std::byte> data) {
    Crc32 crc;
    crc.update(data);
    return crc.finalize();
  }

}  // namespace network