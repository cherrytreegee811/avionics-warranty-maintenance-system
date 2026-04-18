/**
 * @file mma_main.cpp
 * @brief Entry point for the MMA application executable.
 */

#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <thread>

namespace {

  static std::string getCurrentDate() {
    const auto now = std::chrono::system_clock::now();
    const auto year_month_day = std::chrono::floor<std::chrono::days>(now);
    return std::format("{:%Y%m%d}", year_month_day);
  }

}  // namespace

int main() {
  // Create logs directory if it doesn't exist
  std::error_code dir_ec;
  const bool created_logs_dir = std::filesystem::create_directories("logs", dir_ec);
  std::error_code exists_ec;
  const bool logs_dir_exists = std::filesystem::exists("logs", exists_ec);
  if (dir_ec || exists_ec || (!created_logs_dir && !logs_dir_exists)) {
    std::cerr << "Warning: failed to ensure logs directory exists\n";
  }

  // Build log filename with date
  std::string logFileName = std::format("logs/mma_{}.log", getCurrentDate());

  // File sink (truncate each run, or use false to append)
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true);
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>("mma_logger", sinks.begin(), sinks.end());
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);  // flush on every info message

  spdlog::info("MMA server starting");

  mma::MMA mma;
  mma.initialize();
  mma.startServer(8000);
  mma.runMenu();

  spdlog::info("MMA server shutting down");
  spdlog::shutdown();
  return 0;
}