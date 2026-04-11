#pragma once
/**
 * @file Crc32.h
 * @brief Declares CRC-32 checksum utilities used by packet validation.
 */

#include <cstddef>
#include <cstdint>

namespace network {

  /**
   * @brief Incremental CRC-32 calculator using IEEE 802.3 polynomial.
   */
  class Crc32 {
  public:
    /** @brief Constructs a calculator in reset state. */
    Crc32();
    /** @brief Adds a byte range to the checksum state. */
    void update(const void* data, size_t len);
    /** @brief Finalizes and returns current checksum value. */
    uint32_t finalize();
    /** @brief Resets checksum state to initial value. */
    void reset();
    /** @brief Computes CRC-32 for a contiguous buffer in one call. */
    static uint32_t calculate(const void* data, size_t len);

  private:
    uint32_t m_crc;
    static const uint32_t s_table[256];
  };

}  // namespace network