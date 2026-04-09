#pragma once

#include <common/Packet.h>

#include <chrono>
#include <string>
#include <vector>

namespace aircraft {

  struct FaultCode {
    int code;
    network::DiagnosticFaultSeverity severity;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
  };

  struct MaintenanceInfo {
    std::chrono::system_clock::time_point lastMaintenance;
    std::string technician;
    std::string notes;
  };

}  // namespace aircraft