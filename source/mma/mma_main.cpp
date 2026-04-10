#include <mma/mma.h>
#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

std::string getCurrentDate() {
  auto now = std::chrono::system_clock::now();
  auto year_month_day = std::chrono::floor<std::chrono::days>(now);
  return fmt::format("{:%Y%m%d}", year_month_day);
}

int main() {
  // Create logs directory if it doesn't exist
  std::filesystem::create_directories("logs");

  // Build log filename with date
  std::string logFileName = fmt::format("logs/mma_{}.log", getCurrentDate());

  // File sink (truncate each run, or use false to append)
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true);
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>("mma_logger", sinks.begin(), sinks.end());
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);  // flush on every info message

  spdlog::info("MMA server starting");

  MMA mma;
  mma.initialize();
  mma.startServer(8000);
  mma.runMenu();

  spdlog::info("MMA server shutting down");
  spdlog::shutdown();
  return 0;
}