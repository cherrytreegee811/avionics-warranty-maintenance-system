#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <doctest/doctest.h>
#include <helpers/TestHelpers.h>
#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace aircraft;

TEST_CASE("Connection timeout changes aircraft state to DIAGNOSTIC") {
  std::string testLogFile = "test_timeout_integration.log";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_timeout_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  Aircraft aircraft;
  aircraft.connectToMMA("127.0.0.1", 9999);
  std::this_thread::sleep_for(std::chrono::seconds(6));

  CHECK(aircraft.getCurrentState() == "DIAGNOSTIC");

  bool logged
      = test_helpers::logContains(testLogFile, "Connect failed: Connection refused")
        || test_helpers::logContains(testLogFile, "Connection timeout, changing to DIAGNOSTIC");
  CHECK(logged);

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}