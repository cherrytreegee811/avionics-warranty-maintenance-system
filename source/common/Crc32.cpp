/**
 * @file Crc32.cpp
 * @brief Implements CRC-32 checksum calculations.
 */

#include <common/Crc32.h>

#include <cstdint>

namespace network {

  namespace {
    constexpr uint32_t kCrc32Polynomial = 0xEDB88320U;
  }  // namespace

  Crc32::Crc32() : m_crc(0xFFFFFFFFU) {}

  void Crc32::update(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
      m_crc ^= static_cast<uint32_t>(bytes[i]);
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

  uint32_t Crc32::calculate(const void* data, size_t len) {
    Crc32 crc;
    crc.update(data, len);
    return crc.finalize();
  }

}  // namespace network