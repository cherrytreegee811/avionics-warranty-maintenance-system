#pragma once
/**
 * @file WarrantyManager.h
 * @brief Declares persistence and lookup logic for aircraft warranty records.
 */

#include <common/WarrantyData.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * @brief Loads, stores, and queries warranty records keyed by aircraft id.
 */
class WarrantyManager {
public:
  /**
   * @brief Creates manager bound to a CSV storage file path.
  * @param storageFile Type: const std::string&. CSV path used for load/save operations.
   */
  explicit WarrantyManager(const std::string& storageFile = "mma_warranty_data.csv");

  /**
   * @brief Loads warranty records from persistent storage.
  * @return Type: bool. True if load succeeded.
   */
  bool load();
  /**
   * @brief Saves warranty records to persistent storage.
  * @return Type: bool. True if save succeeded.
   */
  bool save();

  /**
   * @brief Retrieves warranty information for an aircraft id when available.
  * @param aircraftId Type: uint64_t. Aircraft identifier key.
  * @return Type: std::optional<@ref common::WarrantyInfo>. Warranty information when present; empty otherwise.
   */
  std::optional<common::WarrantyInfo> getWarranty(uint64_t aircraftId) const;
  /**
   * @brief Inserts or updates warranty information for an aircraft id.
  * @param aircraftId Type: uint64_t. Aircraft identifier key.
  * @param info Type: const @ref common::WarrantyInfo&. Warranty data to store.
   */
  void setWarranty(uint64_t aircraftId, const common::WarrantyInfo& info);

private:
  std::string storageFile_;
  std::unordered_map<uint64_t, common::WarrantyInfo> warranties_;
};