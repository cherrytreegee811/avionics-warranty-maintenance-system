#pragma once
/**
 * @file Crc32.h
 * @brief Declares CRC-32 checksum utilities used by packet validation.
 */

#include <cstddef>
#include <cstdint>
#include <span>

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
      * @param data Type: std::span<const std::byte>. Bytes to incorporate.
     */
        void update(std::span<const std::byte> data);
        /**
         * @brief Adds a byte range to the checksum state.
         * @param data Type: std::span<const uint8_t>. Bytes to incorporate.
         */
        void update(std::span<const uint8_t> data) { update(std::as_bytes(data)); }
    /**
     * @brief Finalizes and returns current checksum value.
     * @return Type: uint32_t. Final CRC-32 checksum.
     */
    uint32_t finalize();
    /** @brief Resets checksum state to initial value. */
    void reset();
    /**
     * @brief Computes CRC-32 for a contiguous buffer in one call.
      * @param data Type: std::span<const std::byte>. Bytes to checksum.
     * @return Type: uint32_t. CRC-32 checksum for the provided bytes.
     */
        static uint32_t calculate(std::span<const std::byte> data);
        /**
         * @brief Computes CRC-32 for a contiguous buffer in one call.
         * @param data Type: std::span<const uint8_t>. Bytes to checksum.
         * @return Type: uint32_t. CRC-32 checksum for the provided bytes.
         */
        static uint32_t calculate(std::span<const uint8_t> data) { return calculate(std::as_bytes(data)); }

  private:
    uint32_t m_crc;
    static const uint32_t s_table[256];
  };

}  // namespace network