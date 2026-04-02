#pragma once

#include <cstddef>
#include <cstdint>

namespace network {

  // Standard CRC-32 (IEEE 802.3) polynomial: 0xEDB88320
  class Crc32 {
  public:
    Crc32();
    void update(const void* data, size_t len);
    uint32_t finalize();
    void reset();
    static uint32_t calculate(const void* data, size_t len);

  private:
    uint32_t m_crc;
    static const uint32_t s_table[256];
  };

}  // namespace network