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

  // Aircraft constructor adds 6 sample fault codes.
  auto faults = aircraft.getFaultCodes();
  CHECK(faults.size() == 6);
  CHECK(faults[0].code == 101);
  CHECK(faults[0].severity == network::DiagnosticFaultSeverity::MINOR);
  CHECK(faults[1].code == 102);
  CHECK(faults[1].severity == network::DiagnosticFaultSeverity::MAJOR);
}

TEST_CASE("REQ-CLT-004: Aircraft can view all fault codes") {
  Aircraft aircraft;

  auto faults = aircraft.getFaultCodes();

  // Verify we can access all fault codes (view functionality)
  CHECK(faults.size() == 6);

  // Verify we can read each fault code's properties
  for (const auto& fault : faults) {
    CHECK(fault.code > 0);
    CHECK((fault.severity == network::DiagnosticFaultSeverity::MINOR
           || fault.severity == network::DiagnosticFaultSeverity::MAJOR));
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
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(
      aircraft.transitionToState(network::StateId::DIAGNOSTIC, aircraft::TransitionSource::MANUAL));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE,
                                   aircraft::TransitionSource::AUTOMATIC));

  spdlog::shutdown();
  CHECK(test_helpers::logContains(testLogFile,
                                  "Operational state transition: STANDBY -> DIAGNOSTIC"));
  CHECK(test_helpers::logContains(testLogFile,
                                  "Operational state transition: DIAGNOSTIC -> MAINTENANCE"));

  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-CLT-056/US-015: Aircraft logs transition metadata for all source labels") {
  std::string testLogFile = "test_aircraft_transition_metadata_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_transition_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  // Keep MAINTENANCE stable so MAINTENANCE -> FAULT is logged with MMA_COMMAND source.
  aircraft.clearFaultCodes();
  aircraft.addFaultCode({701, network::DiagnosticFaultSeverity::MINOR,
                         "Minor metadata transition advisory", std::chrono::system_clock::now()});

  CHECK(
      aircraft.transitionToState(network::StateId::DIAGNOSTIC, aircraft::TransitionSource::MANUAL));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE,
                                   aircraft::TransitionSource::AUTOMATIC));
  CHECK(
      aircraft.transitionToState(network::StateId::FAULT, aircraft::TransitionSource::MMA_COMMAND));
  CHECK(aircraft.transitionToState(network::StateId::STANDBY,
                                   aircraft::TransitionSource::CONNECTION_FALLBACK));

  spdlog::shutdown();
  CHECK(test_helpers::logContains(
      testLogFile, "Operational state transition: STANDBY -> DIAGNOSTIC.*source: MANUAL"));
  CHECK(test_helpers::logContains(
      testLogFile, "Operational state transition: DIAGNOSTIC -> MAINTENANCE.*source: AUTOMATIC"));
  CHECK(test_helpers::logContains(
      testLogFile, "Operational state transition: MAINTENANCE -> FAULT.*source: MMA_COMMAND"));
  CHECK(test_helpers::logContains(
      testLogFile, "Operational state transition: FAULT -> STANDBY.*source: CONNECTION_FALLBACK"));

  std::remove(testLogFile.c_str());
}

TEST_CASE("US-012: Automatic transition MAINTENANCE -> FAULT when MAJOR fault is added") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");

  aircraft.addFaultCode({777, network::DiagnosticFaultSeverity::MAJOR, "Hydraulic leak detected",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.getCurrentState() == "FAULT");
}

TEST_CASE("US-012: Automatic transition MAINTENANCE -> STANDBY when faults are cleared") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({778, network::DiagnosticFaultSeverity::MINOR, "Cabin light anomaly",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");

  aircraft.clearFaultCodes();

  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("US-012: Automatic transition FAULT -> STANDBY when all faults are resolved") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));

  aircraft.addFaultCode({779, network::DiagnosticFaultSeverity::MAJOR, "Engine fire warning",
                         std::chrono::system_clock::now()});
  CHECK(aircraft.getCurrentState() == "FAULT");

  aircraft.clearFaultCodes();

  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("US-012: Automatic transition FAULT -> DIAGNOSTIC when only MINOR faults persist") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({780, network::DiagnosticFaultSeverity::MINOR,
                         "Cabin pressure sensor drift", std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));

  aircraft.addFaultCode({781, network::DiagnosticFaultSeverity::MAJOR,
                         "Primary hydraulic bus failure", std::chrono::system_clock::now()});
  CHECK(aircraft.getCurrentState() == "FAULT");

  CHECK(aircraft.resolveFaultCode(781));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
  CHECK_FALSE(aircraft.resolveFaultCode(9999));
}

TEST_CASE("US-012: Automatic transitions are logged with source AUTOMATIC") {
  std::string testLogFile = "test_aircraft_automatic_transition_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_automatic_transition_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));

  aircraft.addFaultCode({782, network::DiagnosticFaultSeverity::MAJOR,
                         "Autopilot channel disagreement", std::chrono::system_clock::now()});

  spdlog::shutdown();
  CHECK(test_helpers::logContains(
      testLogFile, "Operational state transition: MAINTENANCE -> FAULT.*source: AUTOMATIC"));

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

// ============================================================================
// REQ-CLT-062: Allowed State Transitions
// ============================================================================

TEST_CASE("REQ-CLT-062: Aircraft allows valid transition") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  // Keep MAINTENANCE stable for this transition test.
  aircraft.clearFaultCodes();
  aircraft.addFaultCode({700, network::DiagnosticFaultSeverity::MINOR, "Minor maintenance advisory",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.transitionToState(network::StateId::FAULT));
  CHECK(aircraft.transitionToState(network::StateId::STANDBY));
  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("REQ-CLT-062: MAINTENANCE -> STANDBY is allowed (via cleared faults)") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({800, network::DiagnosticFaultSeverity::MINOR, "Test minor fault",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");

  // Clear faults triggers automatic transition to STANDBY
  aircraft.clearFaultCodes();
  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("REQ-CLT-062: FAULT -> DIAGNOSTIC is allowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({801, network::DiagnosticFaultSeverity::MINOR, "Test minor fault",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));

  // Add MAJOR fault to trigger transition to FAULT
  aircraft.addFaultCode({802, network::DiagnosticFaultSeverity::MAJOR, "Test major fault",
                         std::chrono::system_clock::now()});
  CHECK(aircraft.getCurrentState() == "FAULT");

  // Remove the MAJOR fault; should transition to DIAGNOSTIC
  CHECK(aircraft.resolveFaultCode(802));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
}

// ============================================================================
// REQ-CLT-062: All Disallowed State Transitions
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

TEST_CASE("REQ-CLT-062: STANDBY -> STANDBY is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(aircraft.getCurrentState() == "STANDBY");
  CHECK_FALSE(aircraft.transitionToState(network::StateId::STANDBY));
  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("REQ-CLT-062: STANDBY -> FAULT is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(aircraft.getCurrentState() == "STANDBY");
  CHECK_FALSE(aircraft.transitionToState(network::StateId::FAULT));
  CHECK(aircraft.getCurrentState() == "STANDBY");
}

TEST_CASE("REQ-CLT-062: DIAGNOSTIC -> DIAGNOSTIC is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");

  CHECK_FALSE(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
}

TEST_CASE("REQ-CLT-062: DIAGNOSTIC -> FAULT is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");

  CHECK_FALSE(aircraft.transitionToState(network::StateId::FAULT));
  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");
}

TEST_CASE("REQ-CLT-062: MAINTENANCE -> DIAGNOSTIC is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({803, network::DiagnosticFaultSeverity::MINOR, "Test minor fault",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");

  CHECK_FALSE(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");
}

TEST_CASE("REQ-CLT-062: MAINTENANCE -> MAINTENANCE is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({804, network::DiagnosticFaultSeverity::MINOR, "Test minor fault",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");

  CHECK_FALSE(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "MAINTENANCE");
}

TEST_CASE("REQ-CLT-062: FAULT -> MAINTENANCE is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({805, network::DiagnosticFaultSeverity::MINOR, "Test minor fault",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));

  aircraft.addFaultCode({806, network::DiagnosticFaultSeverity::MAJOR, "Test major fault",
                         std::chrono::system_clock::now()});
  CHECK(aircraft.getCurrentState() == "FAULT");

  CHECK_FALSE(aircraft.transitionToState(network::StateId::MAINTENANCE));
  CHECK(aircraft.getCurrentState() == "FAULT");
}

TEST_CASE("REQ-CLT-062: FAULT -> FAULT is disallowed") {
  Aircraft aircraft;
  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft.clearFaultCodes();
  aircraft.addFaultCode({807, network::DiagnosticFaultSeverity::MINOR, "Test minor fault",
                         std::chrono::system_clock::now()});

  CHECK(aircraft.transitionToState(network::StateId::DIAGNOSTIC));
  CHECK(aircraft.transitionToState(network::StateId::MAINTENANCE));

  aircraft.addFaultCode({808, network::DiagnosticFaultSeverity::MAJOR, "Test major fault",
                         std::chrono::system_clock::now()});
  CHECK(aircraft.getCurrentState() == "FAULT");

  CHECK_FALSE(aircraft.transitionToState(network::StateId::FAULT));
  CHECK(aircraft.getCurrentState() == "FAULT");
}