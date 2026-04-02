#pragma once

#include <string>

namespace common {

  struct WarrantyInfo {
    bool isActive;
    std::string expiryDate;  // YYYY-MM-DD
    std::string provider;
  };

}  // namespace common