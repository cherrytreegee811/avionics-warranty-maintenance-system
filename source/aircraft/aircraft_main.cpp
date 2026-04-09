#include <aircraft/Aircraft.h>
#include <aircraft/CliInterface.h>
#include <aircraft/StateManager.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <memory>

std::string getCurrentDate() {
  auto now = std::chrono::system_clock::now();
  auto year_month_day = std::chrono::floor<std::chrono::days>(now);
  return std::format("{:%Y%m%d}", year_month_day);
}

int main() {
  std::filesystem::create_directories("logs");
  std::string logFileName = std::format("logs/aircraft_{}.log", getCurrentDate());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true);
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);
  file_sink->set_level(spdlog::level::info);
  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

  auto logger = std::make_shared<spdlog::logger>("aircraft_logger", sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::info);
  spdlog::set_default_logger(logger);
  spdlog::flush_on(spdlog::level::info);
  spdlog::info("Aircraft application started");

  aircraft::Aircraft aircraft;
  aircraft.initialize();

  // Start connection to MMA on localhost port 8000
  aircraft.connectToMMA("127.0.0.1", 8000);

  StateManager stateManager;
  aircraft.setStateManager(&stateManager);
  aircraft.syncStateManagerToCurrentState();

  aircraft::CliInterface cli(aircraft);
  cli.showMainMenu();

  spdlog::info("Aircraft application exiting");
  spdlog::shutdown();
  return 0;
}