#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <doctest/doctest.h>
#include <helpers/MockMMA.h>
#include <helpers/TestHelpers.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
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