#include <aircraft/Aircraft.h>

#include <chrono>
#include <iomanip>
#include <sstream>

using namespace aircraft;

Aircraft::Aircraft() : m_currentState("STANDBY") {
  // Initialize with sample data for demonstration
  // In production, this would come from server/ persistence

  // Sample maintenance data
  m_lastMaintenance.lastMaintenance = std::chrono::system_clock::now();
  m_lastMaintenance.technician = "John Doe";
  m_lastMaintenance.notes = "Routine checkup";

  // Sample fault codes
  m_faultCodes.push_back(
      {101, "Engine temperature sensor fault", std::chrono::system_clock::now()});
  m_faultCodes.push_back({202, "Hydraulic pressure low", std::chrono::system_clock::now()});

  // Sample warranty data
  m_warranty.isActive = true;
  m_warranty.expiryDate = "2027-12-31";
  m_warranty.provider = "Aviation Warranty Corp";
}

void Aircraft::initialize() {
  // TODO: Implement network connection initialization
  // TODO: Load persisted data
}

std::string Aircraft::getCurrentState() const { return m_currentState; }

void Aircraft::setCurrentState(const std::string& state) { m_currentState = state; }

MaintenanceInfo Aircraft::getLastMaintenance() const { return m_lastMaintenance; }

std::vector<FaultCode> Aircraft::getFaultCodes() const { return m_faultCodes; }

WarrantyInfo Aircraft::getWarranty() const { return m_warranty; }

void Aircraft::setLastMaintenance(const MaintenanceInfo& info) { m_lastMaintenance = info; }

void Aircraft::addFaultCode(const FaultCode& code) { m_faultCodes.push_back(code); }

void Aircraft::clearFaultCodes() { m_faultCodes.clear(); }

void Aircraft::setWarranty(const WarrantyInfo& info) { m_warranty = info; }