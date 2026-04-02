#pragma once

#include <chrono>
#include <string>
#include <vector>

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

  class Aircraft {
  public:
    Aircraft();
    void initialize();

    // State getters/setters
    std::string getCurrentState() const;
    void setCurrentState(const std::string& state);

    // Data getters
    MaintenanceInfo getLastMaintenance() const;
    std::vector<FaultCode> getFaultCodes() const;
    WarrantyInfo getWarranty() const;

    // Data setters
    void setLastMaintenance(const MaintenanceInfo& info);
    void addFaultCode(const FaultCode& code);
    void clearFaultCodes();
    void setWarranty(const WarrantyInfo& info);

    int token = 0;

  private:
    std::string m_currentState;
    MaintenanceInfo m_lastMaintenance;
    std::vector<FaultCode> m_faultCodes;
    WarrantyInfo m_warranty;
  };

}  // namespace aircraft