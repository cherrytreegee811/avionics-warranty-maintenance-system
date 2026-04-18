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
  bool ok = true;
  std::ifstream file(storageFile_);
  if (!file.is_open()) {
    spdlog::warn("Warranty file not found: {}", storageFile_);
    ok = false;
  }

  if (ok) {
    std::string line;
    while (std::getline(file, line)) {
      if (line.empty()) {
        continue;
      }
      std::istringstream ss(line);
      std::string idStr;
      std::string activeStr;
      std::string expiry;
      std::string provider;
      if (std::getline(ss, idStr, ',') && std::getline(ss, activeStr, ',')
          && std::getline(ss, expiry, ',') && std::getline(ss, provider)) {
        const uint64_t id = std::stoull(idStr);
        const bool active = (activeStr == "1");
        warranties_[id] = {active, expiry, provider};
      }
    }

    spdlog::info("Loaded {} warranty records", warranties_.size());
  }

  return ok;
}

bool WarrantyManager::save() {
  bool ok = true;
  std::ofstream file(storageFile_);
  if (!file.is_open()) {
    spdlog::error("Cannot write warranty file: {}", storageFile_);
    ok = false;
  }

  if (ok) {
    for (const auto& [id, info] : warranties_) {
      std::ostringstream line;
      line << id << ',' << (info.isActive ? "1" : "0") << ',' << info.expiryDate << ','
           << info.provider << '\n';
      const bool write_ok = static_cast<bool>(file << line.str());
      if (!write_ok) {
        ok = false;
      }
    }
  }

  if (ok && !file.good()) {
    ok = false;
  }

  return ok;
}

std::optional<common::WarrantyInfo> WarrantyManager::getWarranty(uint64_t aircraftId) const {
  std::optional<common::WarrantyInfo> result;

  const auto it = warranties_.find(aircraftId);
  if (it != warranties_.end()) {
    result = it->second;
  } else {
    result = std::nullopt;
  }

  return result;
}

void WarrantyManager::setWarranty(uint64_t aircraftId, const common::WarrantyInfo& info) {
  warranties_[aircraftId] = info;
  const bool saved = save();
  if (!saved) {
    spdlog::error("Failed to persist warranty data for aircraft {} to {}", aircraftId,
                  storageFile_);
  }
}

}  // namespace mma