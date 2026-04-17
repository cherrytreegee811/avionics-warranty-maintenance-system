#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <doctest/doctest.h>
#include <helpers/TestHelpers.h>
#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace std::chrono_literals;

// ============================================================================
// REQ-TST-023: We will create system tests for our client and server.
// REQ-TST-024: Test results are marked pass or fail and linked to the requirements.
// ============================================================================

TEST_CASE(
    "REQ-CLT-007/REQ-SRV-008: System - Warranty data persists to CSV (real MMA + real Aircraft)") {
  test_helpers::ScopedTempWorkingDir env("awms_sys_warranty_persistence");
  REQUIRE(env.ok());

  const std::string testLogFile = "system_warranty_persistence.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("sys_warranty_persistence_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  mma::MMA server;
  const uint16_t testPort = 8050;
  server.startServer(testPort);
  test_helpers::ScopedMmaStopper stop_server(server);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client;
    client.connectToMMA("127.0.0.1", testPort);
    const uint64_t aircraft_id = client.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(aircraft_id) + " verified");
        },
        6000ms));

    aircraft::WarrantyInfo warranty{};
    warranty.isActive = true;
    warranty.expiryDate = "2030-01-02";
    warranty.provider = "SystemTest Provider";
    client.setWarranty(warranty);

    CHECK(client.sendWarrantyData());

    const std::string warranty_file = "mma_warranty_data.csv";
    const std::string expected_line
        = std::to_string(aircraft_id) + ",1,2030-01-02,SystemTest Provider";

    REQUIRE(test_helpers::waitFor(
        [&]() {
          if (!std::filesystem::exists(warranty_file)) {
            return false;
          }

          std::ifstream file(warranty_file);
          if (!file.is_open()) {
            return false;
          }

          std::string line;
          while (std::getline(file, line)) {
            if (line.find(expected_line) != std::string::npos) {
              return true;
            }
          }

          return false;
        },
        6000ms));
  }

  server.stopServer();
}

// ============================================================================
// REQ-SRV-006: The MMA should be able to change the airplane states to diagnostic.
// REQ-NET-081: Our applications shall use TCP/IP to communicate.
// REQ-SYS-080: Both client and server shall require connection verification.
// ============================================================================

TEST_CASE(
    "REQ-SRV-006/REQ-NET-081/REQ-SYS-080: System - Multi-aircraft command routing (real MMA + 2x "
    "real Aircraft)") {
  test_helpers::ScopedTempWorkingDir env("awms_sys_multi_aircraft_routing");
  REQUIRE(env.ok());

  const std::string testLogFile = "system_multi_aircraft_routing.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("sys_multi_aircraft_routing_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  mma::MMA server;
  const uint16_t testPort = 8051;
  server.startServer(testPort);
  test_helpers::ScopedMmaStopper stop_server(server);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client1;
    client1.connectToMMA("127.0.0.1", testPort);
    const uint64_t id1 = client1.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(id1) + " verified")
                 && test_helpers::logContains(testLogFile,
                                              "Aircraft " + std::to_string(id1) + " landed");
        },
        8000ms));

    aircraft::Aircraft client2;
    client2.connectToMMA("127.0.0.1", testPort);
    const uint64_t id2 = client2.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(id2) + " verified")
                 && test_helpers::logContains(testLogFile,
                                              "Aircraft " + std::to_string(id2) + " landed");
        },
        8000ms));

    server.sendDiagnosticStateChange(id1);
    server.sendDiagnosticStateChange(id2);

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
                     testLogFile,
                     "Sent DIAGNOSTIC state change command to aircraft " + std::to_string(id1))
                 && test_helpers::logContains(
                     testLogFile,
                     "Sent DIAGNOSTIC state change command to aircraft " + std::to_string(id2))
                 && test_helpers::logContains(
                     testLogFile, "State change confirmation received from aircraft "
                                      + std::to_string(id1) + ": state change to DIAGNOSTIC")
                 && test_helpers::logContains(
                     testLogFile, "State change confirmation received from aircraft "
                                      + std::to_string(id2) + ": state change to DIAGNOSTIC");
        },
        8000ms));
  }

  server.stopServer();
}

// ============================================================================
// REQ-SYS-060: The client or server (or both) shall contain an operational state machine.
// REQ-CLT-061: The airplane shall be able to transition to STANDBY/DIAGNOSTIC/MAINTENANCE/FAULT.
// ============================================================================

TEST_CASE("REQ-SYS-060/REQ-CLT-061: System - MAJOR fault escalates to FAULT state") {
  test_helpers::ScopedTempWorkingDir env("awms_sys_fault_escalation");
  REQUIRE(env.ok());

  const std::string testLogFile = "system_fault_escalation.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("sys_fault_escalation_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  mma::MMA server;
  const uint16_t testPort = 8052;
  server.startServer(testPort);
  test_helpers::ScopedMmaStopper stop_server(server);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client;
    client.connectToMMA("127.0.0.1", testPort);
    const uint64_t aircraft_id = client.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(aircraft_id) + " verified");
        },
        6000ms));

    client.addFaultCode(aircraft::FaultCode{
        9001,
        network::DiagnosticFaultSeverity::MAJOR,
        "System test MAJOR fault",
        std::chrono::system_clock::now(),
    });

    server.sendDiagnosticStateChange(aircraft_id);

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
              testLogFile, "State change confirmation received from aircraft "
                               + std::to_string(aircraft_id) + ": state change to DIAGNOSTIC");
        },
        8000ms));

    // Move from DIAGNOSTIC into MAINTENANCE explicitly; major faults should immediately escalate
    // to FAULT via automatic transition rules.
    REQUIRE(client.transitionToState(network::StateId::MAINTENANCE,
                                     aircraft::TransitionSource::MANUAL));

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
                     testLogFile, "Operational state transition: DIAGNOSTIC -> MAINTENANCE")
                 && test_helpers::logContains(testLogFile,
                                              "Aircraft " + std::to_string(aircraft_id)
                                                  + " transitioned to MAINTENANCE state")
                 && test_helpers::logContains(testLogFile,
                                              "Operational state transition: MAINTENANCE -> FAULT")
                 && test_helpers::logContains(testLogFile, "Aircraft " + std::to_string(aircraft_id)
                                                               + " transitioned to FAULT state");
        },
        8000ms));

    CHECK(client.getCurrentState() == "FAULT");

    client.clearFaultCodes();

    CHECK(test_helpers::waitFor([&]() { return client.getCurrentState() == "STANDBY"; }, 6000ms));
    CHECK(test_helpers::logContains(testLogFile, "Operational state transition: FAULT -> STANDBY"));
  }

  server.stopServer();
}
