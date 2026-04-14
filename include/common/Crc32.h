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
    /**
     * @brief Adds a byte range to the checksum state.
    * @param data Type: const void*. Pointer to bytes to incorporate.
    * @param len Type: size_t. Number of bytes at data.
     */
    void update(const void* data, size_t len);
    /**
     * @brief Finalizes and returns current checksum value.
    * @return Type: uint32_t. Final CRC-32 checksum.
     */
    uint32_t finalize();
    /** @brief Resets checksum state to initial value. */
    void reset();
    /**
     * @brief Computes CRC-32 for a contiguous buffer in one call.
    * @param data Type: const void*. Pointer to bytes to checksum.
    * @param len Type: size_t. Number of bytes at data.
    * @return Type: uint32_t. CRC-32 checksum for the provided bytes.
     */
    static uint32_t calculate(const void* data, size_t len);

  private:
    uint32_t m_crc;
    static const uint32_t s_table[256];
  };

}  // namespace network