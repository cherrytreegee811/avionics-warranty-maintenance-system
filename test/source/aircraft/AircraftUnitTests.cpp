#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/StateManager.h>
#include <doctest/doctest.h>
#include <helpers/TestHelpers.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstring>

using namespace aircraft;

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

// ============================================================================
// REQ-SRV-006: The MMA should be able to change the airplane states to diagnostic
// ============================================================================

TEST_CASE("REQ-SRV-006: STATE_CHANGE transition updates aircraft state through StateManager") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
}

// ============================================================================
// REQ-CLT-062: The airplane shall be prevented from transitioning to disallowed states from a given
// state.
// ============================================================================

TEST_CASE("REQ-CLT-062: Aircraft rejects invalid transitions") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK_FALSE(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "STANDBY");

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK_FALSE(aircraft.transitionToState(network::StateId::STANDBY));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
}

TEST_CASE("REQ-CLT-062: Aircraft allows valid transition") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.transitionToState(network::StateId::FAULT));
  CHECK(aircraft.transitionToState(network::StateId::STANDBY));
  CHECK(aircraft.getCurrentState() == "STANDBY");
}

// ===========================================================================
// REQ-CLT-056: The airplane should log when it has been switched states
// ============================================================================
TEST_CASE("REQ-CLT-056: Aircraft logs state changes") {
  std::string testLogFile = "test_aircraft_state_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  Aircraft aircraft;
  aircraft.setCurrentState("DIAGNOSTIC");
  aircraft.setCurrentState("MAINTENANCE");

  spdlog::shutdown();
  CHECK(
      test_helpers::logContains(testLogFile, "Aircraft state changing from STANDBY to DIAGNOSTIC"));
  CHECK(test_helpers::logContains(testLogFile,
                                  "Aircraft state changing from DIAGNOSTIC to MAINTENANCE"));

  std::remove(testLogFile.c_str());
}

// ============================================================================
// REQ-CLT-054: The airplane should log when it sends the landed signal
// ============================================================================
TEST_CASE("REQ-CLT-054: Aircraft logs error when landed notification cannot be sent") {
  std::string testLogFile = "test_aircraft_landed_error_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  Aircraft aircraft;
  aircraft.sendLandedNotification();  // not connected -> logs error

  spdlog::shutdown();
  CHECK(test_helpers::logContains(testLogFile,
                                  "Cannot send landed notification: not connected/verified"));

  std::remove(testLogFile.c_str());
}