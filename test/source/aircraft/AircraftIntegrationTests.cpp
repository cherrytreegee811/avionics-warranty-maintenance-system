#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/StateManager.h>
#include <doctest/doctest.h>
#include <helpers/MockMMA.h>
#include <helpers/TestHelpers.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono_literals;

// ============================================================================
// REQ-CLT-005: Client sends LANDED notification (client-only verification)
// ============================================================================
TEST_CASE("REQ-CLT-005: Client sends LANDED notification to MMA") {
  const uint16_t testPort = 8020;
  test_helpers::MockMMA mockServer(testPort);
  std::this_thread::sleep_for(100ms);

  aircraft::Aircraft client;
  client.connectToMMA("127.0.0.1", testPort);

  // Wait for the mock to receive LANDED (max 2 seconds)
  bool received = test_helpers::waitFor([&]() { return mockServer.hasReceivedLanded(); }, 2000);
  CHECK(received);
}

// ============================================================================
// REQ-CLT-054: Client logs when it sends the landed signal.
// ============================================================================
TEST_CASE("REQ-CLT-054: Client logs after sending LANDED notification") {
  const std::string clientLogFile = "test_client_landed_log.txt";
  std::remove(clientLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(clientLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("client_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  // Scope to ensure client and mockServer are destroyed before spdlog::shutdown()
  {
    const uint16_t testPort = 8021;
    test_helpers::MockMMA mockServer(testPort);
    std::this_thread::sleep_for(100ms);

    aircraft::Aircraft client;
    client.connectToMMA("127.0.0.1", testPort);

    // Wait for the log file to contain the expected message
    bool logged = test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(clientLogFile, "Landed notification sent to MMA");
        },
        3000);
    CHECK(logged);
  }  // client and mockServer destroyed here

  spdlog::shutdown();
  std::remove(clientLogFile.c_str());
}

// ============================================================================
// REQ-SYS-060 / REQ-CLT-061 / REQ-CLT-063 / REQ-CLT-071 / REQ-SRV-008:
// Integration - State transitions and data transfer
// ============================================================================

TEST_CASE(
    "REQ-SYS-060/REQ-CLT-061/REQ-CLT-063/REQ-CLT-071/REQ-SRV-008: Integration - Transition to "
    "DIAGNOSTIC and verify diagnostic/warranty data transfer") {
  const uint16_t testPort = 8022;
  test_helpers::MockMMA mockServer(testPort);
  std::this_thread::sleep_for(100ms);

  aircraft::Aircraft client;
  StateManager stateManager;
  client.setStateManager(&stateManager);
  client.syncStateManagerToCurrentState();
  client.clearFaultCodes();
  client.addFaultCode({9401, network::DiagnosticFaultSeverity::MINOR,
                       "Integration baseline minor fault", std::chrono::system_clock::now()});
  client.connectToMMA("127.0.0.1", testPort);

  const bool connected_and_verified
      = test_helpers::waitFor([&]() { return mockServer.isVerified(); }, 3000);
  REQUIRE(connected_and_verified);

  REQUIRE(mockServer.sendStateChange(network::StateId::DIAGNOSTIC));

  const bool reached_maintenance
      = test_helpers::waitFor([&]() { return client.getCurrentState() == "MAINTENANCE"; }, 4000);
  CHECK(reached_maintenance);
  CHECK(mockServer.hasConfirmationForState(network::StateId::DIAGNOSTIC));
  CHECK(mockServer.hasConfirmationForState(network::StateId::MAINTENANCE));
  CHECK(mockServer.hasReceivedDiagnosticData());
  CHECK(mockServer.receivedDiagnosticFaultCount() > 0);
  CHECK(mockServer.hasReceivedWarrantyData());
}

TEST_CASE("REQ-SYS-060/REQ-CLT-062: Integration - Invalid transition input is rejected") {
  const uint16_t testPort = 8023;
  test_helpers::MockMMA mockServer(testPort);
  std::this_thread::sleep_for(100ms);

  aircraft::Aircraft client;
  StateManager stateManager;
  client.setStateManager(&stateManager);
  client.syncStateManagerToCurrentState();
  client.clearFaultCodes();
  client.addFaultCode({9402, network::DiagnosticFaultSeverity::MINOR,
                       "Integration baseline minor fault", std::chrono::system_clock::now()});
  client.connectToMMA("127.0.0.1", testPort);

  const bool connected_and_verified
      = test_helpers::waitFor([&]() { return mockServer.isVerified(); }, 3000);
  REQUIRE(connected_and_verified);

  const size_t confirmations_before = mockServer.stateChangeConfirmationCount();
  REQUIRE(mockServer.sendStateChange(network::StateId::FAULT));

  std::this_thread::sleep_for(400ms);
  CHECK(client.getCurrentState() == "STANDBY");
  CHECK(mockServer.stateChangeConfirmationCount() == confirmations_before);
}

TEST_CASE("REQ-CLT-082: Timed connection fallback transitions to DIAGNOSTIC") {
  const std::string logFile = "test_aircraft_connect_fallback_log.txt";
  std::remove(logFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
  auto logger = std::make_shared<spdlog::logger>("aircraft_connect_fallback_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  aircraft::Aircraft client;
  StateManager stateManager;
  client.setStateManager(&stateManager);
  client.syncStateManagerToCurrentState();

  const auto start = std::chrono::steady_clock::now();
  client.connectToMMA("10.255.255.1", 65000);

  const bool transitioned
      = test_helpers::waitFor([&]() { return client.getCurrentState() == "DIAGNOSTIC"; }, 8000);
  REQUIRE(transitioned);

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();
  CHECK(elapsed_ms >= 3000);
  CHECK(test_helpers::logContains(logFile,
                                  "Connection timeout, changing to DIAGNOSTIC|Connect failed: .*"));

  spdlog::shutdown();
  std::remove(logFile.c_str());
}

TEST_CASE(
    "REQ-SYS-060/REQ-CLT-061: Integration - MAJOR fault in MAINTENANCE transitions to FAULT") {
  const uint16_t testPort = 8024;
  test_helpers::MockMMA mockServer(testPort);
  std::this_thread::sleep_for(100ms);

  aircraft::Aircraft client;
  StateManager stateManager;
  client.setStateManager(&stateManager);
  client.syncStateManagerToCurrentState();
  client.clearFaultCodes();
  client.addFaultCode({9403, network::DiagnosticFaultSeverity::MINOR,
                       "Integration baseline minor fault", std::chrono::system_clock::now()});
  client.connectToMMA("127.0.0.1", testPort);

  const bool connected_and_verified
      = test_helpers::waitFor([&]() { return mockServer.isVerified(); }, 3000);
  REQUIRE(connected_and_verified);

  REQUIRE(mockServer.sendStateChange(network::StateId::DIAGNOSTIC));
  REQUIRE(test_helpers::waitFor([&]() { return client.getCurrentState() == "MAINTENANCE"; }, 4000));

  client.addFaultCode({9501, network::DiagnosticFaultSeverity::MAJOR,
                       "Integration test major fault injection", std::chrono::system_clock::now()});

  const bool transitioned_to_fault
      = test_helpers::waitFor([&]() { return client.getCurrentState() == "FAULT"; }, 3000);
  CHECK(transitioned_to_fault);
  CHECK(test_helpers::waitFor(
      [&]() { return mockServer.hasConfirmationForState(network::StateId::FAULT); }, 2000));
}

TEST_CASE("REQ-NET-081: Integration - Aircraft reassembles SCHEMATIC_CHUNK sent by MMA") {
  const std::string clientLogFile = "test_aircraft_receive_schematic_chunk_log.txt";
  std::remove(clientLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(clientLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("aircraft_schematic_chunk_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  {
    const uint16_t testPort = 8025;
    test_helpers::MockMMA mockServer(testPort);
    std::this_thread::sleep_for(100ms);

    aircraft::Aircraft client;
    client.connectToMMA("127.0.0.1", testPort);

    REQUIRE(test_helpers::waitFor([&]() { return mockServer.isVerified(); }, 3000));

    const std::vector<uint8_t> image_data{0x01, 0x02, 0x03, 0x04};
    const auto chunk_payloads
        = network::serializeImagePayload(77, image_data, network::ImageFormat::RAW);
    REQUIRE(chunk_payloads.size() == 1);
    REQUIRE(mockServer.sendSchematicChunkPayload(chunk_payloads.front()));

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(clientLogFile,
                                           "Image 77 received and reassembled \\(4 bytes total, "
                                           "format: RAW\\)");
        },
        3000));
  }

  spdlog::shutdown();
  std::remove(clientLogFile.c_str());
}

TEST_CASE(
    "REQ-NET-081: Integration - Aircraft handles SCHEMATIC_CHUNK_RETRY_REQUEST by resending "
    "cached chunk") {
  const uint16_t testPort = 8026;
  test_helpers::MockMMA mockServer(testPort);
  std::this_thread::sleep_for(100ms);

  aircraft::Aircraft client;
  client.connectToMMA("127.0.0.1", testPort);

  REQUIRE(test_helpers::waitFor([&]() { return mockServer.isVerified(); }, 3000));

  const std::vector<uint8_t> image_data{0x10, 0x20, 0x30, 0x40};
  REQUIRE(client.sendImage(image_data, network::ImageFormat::RAW));

  REQUIRE(
      test_helpers::waitFor([&]() { return mockServer.receivedSchematicChunkCount() >= 1; }, 3000));

  const auto initial_chunks = mockServer.receivedSchematicChunkPayloads();
  REQUIRE(!initial_chunks.empty());
  const auto expected_chunk_payload = initial_chunks.front();

  REQUIRE(mockServer.sendChunkRetryRequest(1, 0));

  REQUIRE(
      test_helpers::waitFor([&]() { return mockServer.receivedSchematicChunkCount() >= 2; }, 3000));

  const auto chunks_after_retry = mockServer.receivedSchematicChunkPayloads();
  REQUIRE(chunks_after_retry.size() >= 2);
  CHECK(chunks_after_retry.back() == expected_chunk_payload);
}
