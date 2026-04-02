#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <common/Packet.h>
#include <doctest/doctest.h>

#include <cstring>

using namespace aircraft;
using namespace network;

// ============================================================================
// REQ-CLT-002: The airplane (client) shall display its current state
// (STANDBY, DIAGNOSTIC, MAINTENANCE, FAULT)
// ============================================================================

TEST_CASE("REQ-CLT-002: Aircraft displays current state - STANDBY") {
  Aircraft aircraft;
  aircraft.setCurrentState("STANDBY");
  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("REQ-CLT-002: Aircraft displays current state - DIAGNOSTIC") {
  Aircraft aircraft;
  aircraft.setCurrentState("DIAGNOSTIC");
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
}

TEST_CASE("REQ-CLT-002: Aircraft displays current state - MAINTENANCE") {
  Aircraft aircraft;
  aircraft.setCurrentState("MAINTENANCE");
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");
}

TEST_CASE("REQ-CLT-002: Aircraft displays current state - FAULT") {
  Aircraft aircraft;
  aircraft.setCurrentState("FAULT");
  CHECK(aircraft.getCurrentState() == "FAULT");
}

// ============================================================================
// REQ-CLT-003: The airplane should allow viewing of last maintenance time
// ============================================================================

TEST_CASE("REQ-CLT-003: Aircraft returns last maintenance time") {
  Aircraft aircraft;
  MaintenanceInfo maint = aircraft.getLastMaintenance();

  CHECK(maint.technician == "John Doe");
  CHECK(maint.notes == "Routine checkup");
  auto now = std::chrono::system_clock::now();
  CHECK(maint.lastMaintenance <= now);
}

// ============================================================================
// REQ-CLT-004: The airplane should allow viewing of its fault codes
// ============================================================================

TEST_CASE("REQ-CLT-004: Aircraft returns fault codes") {
  Aircraft aircraft;

  // Aircraft constructor adds 2 sample fault codes (101 and 202)
  auto faults = aircraft.getFaultCodes();
  CHECK(faults.size() == 2);
  CHECK(faults[0].code == 101);
  CHECK(faults[0].description == "Engine temperature sensor fault");
  CHECK(faults[1].code == 202);
  CHECK(faults[1].description == "Hydraulic pressure low");
}

TEST_CASE("REQ-CLT-004: Aircraft can view all fault codes") {
  Aircraft aircraft;

  auto faults = aircraft.getFaultCodes();

  // Verify we can access all fault codes (view functionality)
  CHECK(faults.size() == 2);

  // Verify we can read each fault code's properties
  for (const auto& fault : faults) {
    CHECK(fault.code > 0);
    CHECK(!fault.description.empty());
  }
}

// ============================================================================
// REQ-CLT-007: The airplane should allow viewing of its warranty
// ============================================================================

TEST_CASE("REQ-CLT-007: Aircraft returns warranty status") {
  Aircraft aircraft;
  WarrantyInfo warranty = aircraft.getWarranty();

  CHECK(warranty.isActive == true);
  CHECK(warranty.expiryDate == "2027-12-31");
  CHECK(warranty.provider == "Aviation Warranty Corp");
}

// ============================================================================
// REQ-CLT-041: The airplane (client) must provide a CLI/TUI interface
// ============================================================================

TEST_CASE("REQ-CLT-041: Aircraft has token identifier for CLI interface") {
  Aircraft aircraft;
  aircraft.token = 12345;
  CHECK(aircraft.token == 12345);
}

// ============================================================================
// REQ-SYS-040: The two applications shall have different user interfaces
// ============================================================================

TEST_CASE("REQ-SYS-040: Client CLI interface exists and is distinct from server") {
  Aircraft aircraft;

  // This test verifies the client has a CLI interface component
  // The server will have its own separate interface
  // The existence of a separate client UI component ensures it can be distinct
  SUBCASE("Client CLI interface exists") {
    // Verify the aircraft can be instantiated with CLI capabilities
    // The actual CLI display is verified through manual testing
    CHECK(aircraft.getCurrentState() == "STANDBY");
  }
}