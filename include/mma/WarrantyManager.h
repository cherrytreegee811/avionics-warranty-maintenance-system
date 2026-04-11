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
  /** @brief Creates manager bound to a CSV storage file path. */
  explicit WarrantyManager(const std::string& storageFile = "mma_warranty_data.csv");

  /** @brief Loads warranty records from persistent storage. */
  bool load();
  /** @brief Saves warranty records to persistent storage. */
  bool save();

  /** @brief Retrieves warranty information for an aircraft id when available. */
  std::optional<common::WarrantyInfo> getWarranty(uint64_t aircraftId) const;
  /** @brief Inserts or updates warranty information for an aircraft id. */
  void setWarranty(uint64_t aircraftId, const common::WarrantyInfo& info);

private:
  std::string storageFile_;
  std::unordered_map<uint64_t, common::WarrantyInfo> warranties_;
};