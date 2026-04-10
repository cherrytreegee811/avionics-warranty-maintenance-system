#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <helpers/MockAircraft.h>
#include <helpers/TestHelpers.h>
#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

// ============================================================================
// REQ-SRV-053: The MMA logs when it receives a landed notification from an aircraft.
// ============================================================================

TEST_CASE("REQ-SRV-053: MMA server logs landed notification from client") {
  const std::string mmaLogFile = "test_mma_server_landed.log";
  std::remove(mmaLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(mmaLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("mma_logger_test", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  const uint16_t testPort = 8040;
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  {
    test_helpers::MockAircraft client("127.0.0.1", testPort, 12345);
    client.runVerificationAndSendLanded();
    std::this_thread::sleep_for(200ms);
  }

  server.stopServer();

  bool found = test_helpers::waitFor(
      [&]() { return test_helpers::logContains(mmaLogFile, "Aircraft 12345 landed"); }, 2000);

  CHECK(found);

  spdlog::shutdown();
  std::remove(mmaLogFile.c_str());
}
