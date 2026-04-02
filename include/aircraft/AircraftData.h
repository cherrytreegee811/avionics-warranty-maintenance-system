#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace aircraft {

struct FaultCode {
    int code;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
};

struct WarrantyInfo {
    bool isActive;
    std::string expiryDate;
    std::string provider;
};

struct MaintenanceInfo {
    std::chrono::system_clock::time_point lastMaintenance;
    std::string technician;
    std::string notes;
};

} // namespace aircraft