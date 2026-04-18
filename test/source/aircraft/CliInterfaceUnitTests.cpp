#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/CliInterface.h>
#include <doctest/doctest.h>

#include <sstream>
#include <string>

namespace {
  struct UiCapture {
    std::istringstream in;
    std::ostringstream out;

    explicit UiCapture(std::string scriptedInput = "") : in(std::move(scriptedInput)) {}
  };
}  // namespace

// ============================================================================
// REQ-CLT-002: The airplane (client) shall display its current state
// ============================================================================

TEST_CASE("REQ-CLT-002: CLI displays state-specific descriptions") {
  aircraft::Aircraft aircraft;

  SUBCASE("STANDBY") {
    UiCapture cap;
    aircraft.setCurrentState("STANDBY");
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayCurrentState();
    const auto s = cap.out.str();
    CHECK(s.find("Operational State: STANDBY") != std::string::npos);
    CHECK(s.find("Aircraft is on standby") != std::string::npos);
  }

  SUBCASE("DIAGNOSTIC") {
    UiCapture cap;
    aircraft.setCurrentState("DIAGNOSTIC");
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayCurrentState();
    const auto s = cap.out.str();
    CHECK(s.find("Operational State: DIAGNOSTIC") != std::string::npos);
    CHECK(s.find("running diagnostics") != std::string::npos);
  }

  SUBCASE("MAINTENANCE") {
    UiCapture cap;
    aircraft.setCurrentState("MAINTENANCE");
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayCurrentState();
    const auto s = cap.out.str();
    CHECK(s.find("Operational State: MAINTENANCE") != std::string::npos);
    CHECK(s.find("maintenance mode") != std::string::npos);
  }

  SUBCASE("FAULT") {
    UiCapture cap;
    aircraft.setCurrentState("FAULT");
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayCurrentState();
    const auto s = cap.out.str();
    CHECK(s.find("Operational State: FAULT") != std::string::npos);
    CHECK(s.find("Action Required") != std::string::npos);
  }
}

// ============================================================================
// REQ-CLT-003: The airplane should allow viewing of last maintenance time
// ============================================================================

TEST_CASE("REQ-CLT-003: CLI renders last maintenance record") {
  aircraft::Aircraft aircraft;
  UiCapture cap;
  aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);

  ui.displayLastMaintenance();
  const auto s = cap.out.str();

  CHECK(s.find("LAST MAINTENANCE RECORD") != std::string::npos);
  CHECK(s.find("Technician:") != std::string::npos);
  CHECK(s.find("Notes:") != std::string::npos);
}

// ============================================================================
// REQ-CLT-004: The airplane should allow viewing of its fault codes
// ============================================================================

TEST_CASE("REQ-CLT-004: CLI renders fault codes (empty and non-empty)") {
  aircraft::Aircraft aircraft;

  SUBCASE("Empty") {
    UiCapture cap;
    aircraft.clearFaultCodes();
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayFaultCodes();
    const auto s = cap.out.str();
    CHECK(s.find("No active fault codes") != std::string::npos);
    CHECK(s.find("All systems operational") != std::string::npos);
  }

  SUBCASE("Non-empty") {
    UiCapture cap;
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayFaultCodes();
    const auto s = cap.out.str();
    CHECK(s.find("Active Fault Codes") != std::string::npos);
    CHECK(s.find("[CODE") != std::string::npos);
    CHECK(s.find("Severity:") != std::string::npos);
    CHECK(s.find("Description:") != std::string::npos);
  }
}

// ============================================================================
// REQ-CLT-007 / REQ-SRV-008: The airplane should allow viewing of its warranty
// ============================================================================

TEST_CASE("REQ-CLT-007/REQ-SRV-008: CLI renders warranty status (active and expired)") {
  aircraft::Aircraft aircraft;

  SUBCASE("Active") {
    UiCapture cap;
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayWarrantyStatus();
    const auto s = cap.out.str();
    CHECK(s.find("WARRANTY STATUS") != std::string::npos);
    CHECK(s.find("ACTIVE") != std::string::npos);
    CHECK(s.find("Provider:") != std::string::npos);
  }

  SUBCASE("Expired") {
    UiCapture cap;
    aircraft::WarrantyInfo expired{false, "2000-01-01", "OldProvider"};
    aircraft.setWarranty(expired);
    aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);
    ui.displayWarrantyStatus();
    const auto s = cap.out.str();
    CHECK(s.find("EXPIRED") != std::string::npos);
    CHECK(s.find("OldProvider") == std::string::npos);  // provider not printed in expired branch
  }
}

TEST_CASE("REQ-CLT-002/REQ-CLT-003/REQ-CLT-004/REQ-CLT-007: CLI renders complete status") {
  aircraft::Aircraft aircraft;
  UiCapture cap;
  aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);

  ui.displayAllInfo();
  const auto s = cap.out.str();

  CHECK(s.find("COMPLETE AIRCRAFT STATUS") != std::string::npos);
  CHECK(s.find("[STATE]") != std::string::npos);
  CHECK(s.find("[MAINTENANCE]") != std::string::npos);
  CHECK(s.find("[WARRANTY]") != std::string::npos);
  CHECK(s.find("[FAULT CODES]") != std::string::npos);
}
