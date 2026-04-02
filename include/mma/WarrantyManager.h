#pragma once
#include <common/WarrantyData.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

class WarrantyManager {
public:
  explicit WarrantyManager(const std::string& storageFile = "mma_warranty_data.csv");

  bool load();
  bool save();

  std::optional<common::WarrantyInfo> getWarranty(uint64_t aircraftId) const;
  void setWarranty(uint64_t aircraftId, const common::WarrantyInfo& info);

private:
  std::string storageFile_;
  std::unordered_map<uint64_t, common::WarrantyInfo> warranties_;
};