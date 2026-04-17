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

    explicit UiCapture(std::string scriptedInput) : in(std::move(scriptedInput)) {}
  };
}  // namespace

// ============================================================================
// REQ-CLT-041: The airplane (client) must provide a CLI/TUI interface for interaction
// REQ-TST-024: Test results are marked pass or fail and linked to the requirements
// ============================================================================

TEST_CASE("REQ-CLT-041: CLI main menu supports happy-path navigation") {
  aircraft::Aircraft aircraft;
  aircraft.setCurrentState("STANDBY");

  UiCapture cap{"1\n2\n3\n4\n5\n6\n"};
  aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);

  ui.showMainMenu();

  const auto s = cap.out.str();
  CHECK(s.find("AIRCRAFT MAINTENANCE MANAGEMENT") != std::string::npos);
  CHECK(s.find("Enter choice (1-6):") != std::string::npos);

  CHECK(s.find("CURRENT AIRCRAFT STATE") != std::string::npos);
  CHECK(s.find("LAST MAINTENANCE RECORD") != std::string::npos);
  CHECK(s.find("FAULT CODES") != std::string::npos);
  CHECK(s.find("WARRANTY STATUS") != std::string::npos);
  CHECK(s.find("COMPLETE AIRCRAFT STATUS") != std::string::npos);

  CHECK(s.find("Exiting application") != std::string::npos);
}

TEST_CASE("REQ-CLT-041: CLI main menu recovers from non-numeric input") {
  aircraft::Aircraft aircraft;
  aircraft.setCurrentState("STANDBY");

  UiCapture cap{"abc\n6\n"};
  aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);

  ui.showMainMenu();

  const auto s = cap.out.str();
  CHECK(s.find("Invalid input. Please enter a number.") != std::string::npos);
  CHECK(s.find("Exiting application") != std::string::npos);
}

TEST_CASE("REQ-CLT-041: CLI main menu rejects out-of-range selections") {
  aircraft::Aircraft aircraft;
  aircraft.setCurrentState("STANDBY");

  UiCapture cap{"99\n6\n"};
  aircraft::CliInterface ui(aircraft, cap.in, cap.out, /*enable_screen_control=*/false);

  ui.showMainMenu();

  const auto s = cap.out.str();
  CHECK(s.find("Invalid choice. Please select 1-6.") != std::string::npos);
  CHECK(s.find("Exiting application") != std::string::npos);
}
