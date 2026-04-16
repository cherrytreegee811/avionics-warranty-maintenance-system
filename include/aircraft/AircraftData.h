#pragma once
/**
 * @file AircraftData.h
 * @brief Shared aircraft DTO structures for faults and maintenance state.
 */

#include <common/Packet.h>

#include <chrono>
#include <string>
#include <vector>

namespace aircraft {

  /**
   * @brief Represents one diagnostic fault recorded on aircraft.
   */
  struct FaultCode {
    int code;
    network::DiagnosticFaultSeverity severity;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
  };

  /**
   * @brief Represents latest completed maintenance details.
   */
  struct MaintenanceInfo {
    std::chrono::system_clock::time_point lastMaintenance;
    std::string technician;
    std::string notes;
  };

}  // namespace aircraft