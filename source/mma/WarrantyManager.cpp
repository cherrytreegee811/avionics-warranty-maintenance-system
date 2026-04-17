/**
 * @file WarrantyManager.cpp
 * @brief Implements warranty persistence and lookup services.
 */

#include <mma/WarrantyManager.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>

namespace mma {

  WarrantyManager::WarrantyManager(const std::string& storageFile) : storageFile_(storageFile) {}

  bool WarrantyManager::load() {
    std::ifstream file(storageFile_);
    if (!file.is_open()) {
      spdlog::warn("Warranty file not found: {}", storageFile_);
      return false;
    }
    std::string line;
    while (std::getline(file, line)) {
      if (line.empty()) continue;
      std::istringstream ss(line);
      std::string idStr, activeStr, expiry, provider;
      if (std::getline(ss, idStr, ',') && std::getline(ss, activeStr, ',')
          && std::getline(ss, expiry, ',') && std::getline(ss, provider)) {
        uint64_t id = std::stoull(idStr);
        bool active = (activeStr == "1");
        warranties_[id] = {active, expiry, provider};
      }
    }
    spdlog::info("Loaded {} warranty records", warranties_.size());
    return true;
  }

  bool WarrantyManager::save() {
    std::ofstream file(storageFile_);
    if (!file.is_open()) {
      spdlog::error("Cannot write warranty file: {}", storageFile_);
      return false;
    }
    for (const auto& [id, info] : warranties_) {
      file << id << ',' << (info.isActive ? "1" : "0") << ',' << info.expiryDate << ','
           << info.provider << '\n';
    }
    return true;
  }

  std::optional<common::WarrantyInfo> WarrantyManager::getWarranty(uint64_t aircraftId) const {
    auto it = warranties_.find(aircraftId);
    if (it != warranties_.end()) return it->second;
    return std::nullopt;
  }

  void WarrantyManager::setWarranty(uint64_t aircraftId, const common::WarrantyInfo& info) {
    warranties_[aircraftId] = info;
    save();
  }

}  // namespace mma