#pragma once
/**
 * @file WarrantyData.h
 * @brief Defines warranty data transfer object shared between components.
 */

#include <string>

namespace common {

  /**
   * @brief Warranty metadata for one aircraft.
   */
  struct WarrantyInfo {
    bool isActive;
    std::string expiryDate;  // YYYY-MM-DD
    std::string provider;
  };

}  // namespace common