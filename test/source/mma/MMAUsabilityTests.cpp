#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>
#include <helpers/TestHelpers.h>
#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
  class ScopedConsoleRedirect {
  public:
    ScopedConsoleRedirect(std::istream& input, std::ostream& output)
        : input_(input), output_(output), cinBuffer_(std::cin.rdbuf(input_.rdbuf())),
          coutBuffer_(std::cout.rdbuf(output_.rdbuf())) {}

    ScopedConsoleRedirect(const ScopedConsoleRedirect&) = delete;
    ScopedConsoleRedirect& operator=(const ScopedConsoleRedirect&) = delete;

    ~ScopedConsoleRedirect() {
      std::cout.rdbuf(coutBuffer_);
      std::cin.rdbuf(cinBuffer_);
    }

  private:
    std::istream& input_;
    std::ostream& output_;
    std::streambuf* cinBuffer_;
    std::streambuf* coutBuffer_;
  };

  void writeWarrantyCsv(const std::string& path, const std::string& contents) {
    std::ofstream file(path);
    REQUIRE(file.is_open());
    file << contents;
  }

  void configureMmaLogger(const std::string& logFile) {
    std::remove(logFile.c_str());
    auto logger = spdlog::basic_logger_mt("mma_usability_logger", logFile);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
  }
}  // namespace

TEST_CASE("REQ-CLT-041: MMA usability - runMenu displays warranty details and exits") {
  test_helpers::ScopedTempWorkingDir env("awms_mma_usability_active");
  REQUIRE(env.ok());

  const std::string warrantyFile = "mma_warranty_data.csv";
  writeWarrantyCsv(warrantyFile, "1001,1,2030-01-02,OperatorTest\n");

  const std::string logFile = "mma_usability_active.log";
  configureMmaLogger(logFile);

  std::istringstream input("1\n1001\n3\n");
  std::ostringstream output;
  ScopedConsoleRedirect redirect(input, output);

  MMA mma;
  mma.runMenu();

  spdlog::shutdown();

  const auto console = output.str();
  CHECK(console.find("MMA Technician Console") != std::string::npos);
  CHECK(console.find("Enter choice:") != std::string::npos);
  CHECK(console.find("Enter aircraft ID:") != std::string::npos);
  CHECK(console.find("Shutting down...") != std::string::npos);

  CHECK(test_helpers::logContains(logFile, "Warranty for aircraft 1001 is ACTIVE"));
  CHECK(test_helpers::logContains(logFile, "Warranty for aircraft 1001 expires on 2030-01-02"));
  CHECK(test_helpers::logContains(logFile,
                                  "Warranty for aircraft 1001 is provided by OperatorTest"));
}
    
TEST_CASE("REQ-CLT-041: MMA usability - runMenu reports missing warranty records") {
  test_helpers::ScopedTempWorkingDir env("awms_mma_usability_missing");
  REQUIRE(env.ok());

  const std::string warrantyFile = "mma_warranty_data.csv";
  writeWarrantyCsv(warrantyFile, "");

  const std::string logFile = "mma_usability_missing.log";
  configureMmaLogger(logFile);

  std::istringstream input("1\n9999\n3\n");
  std::ostringstream output;
  ScopedConsoleRedirect redirect(input, output);

  MMA mma;
  mma.runMenu();

  spdlog::shutdown();

  const auto console = output.str();
  CHECK(console.find("MMA Technician Console") != std::string::npos);
  CHECK(console.find("Enter aircraft ID:") != std::string::npos);
  CHECK(console.find("Shutting down...") != std::string::npos);

  CHECK(test_helpers::logContains(logFile, "No warranty record found for aircraft 9999"));
}

TEST_CASE("REQ-CLT-041: MMA usability - runMenu displays expired warranty details") {
  test_helpers::ScopedTempWorkingDir env("awms_mma_usability_expired");
  REQUIRE(env.ok());

  const std::string warrantyFile = "mma_warranty_data.csv";
  writeWarrantyCsv(warrantyFile, "2002,0,2020-01-01,LegacyOps\n");

  const std::string logFile = "mma_usability_expired.log";
  configureMmaLogger(logFile);

  std::istringstream input("1\n2002\n3\n");
  std::ostringstream output;
  ScopedConsoleRedirect redirect(input, output);

  MMA mma;
  mma.runMenu();

  spdlog::shutdown();

  CHECK(test_helpers::logContains(logFile, "Warranty for aircraft 2002 is EXPIRED"));
  CHECK(test_helpers::logContains(logFile,
                                  "Warranty for aircraft 2002 is provided by LegacyOps"));
  CHECK(!test_helpers::logContains(logFile, "Warranty for aircraft 2002 expires on"));
}

TEST_CASE("REQ-CLT-041: MMA usability - runMenu handles list and command options for unverified aircraft") {
  test_helpers::ScopedTempWorkingDir env("awms_mma_usability_menu_branches");
  REQUIRE(env.ok());

  const std::string warrantyFile = "mma_warranty_data.csv";
  writeWarrantyCsv(warrantyFile, "");

  const std::string logFile = "mma_usability_menu_branches.log";
  configureMmaLogger(logFile);

  std::istringstream input("2\n4\n777\n5\n777\n42\n9\n3\n");
  std::ostringstream output;
  ScopedConsoleRedirect redirect(input, output);

  MMA mma;
  mma.runMenu();

  spdlog::shutdown();

  const auto console = output.str();
  CHECK(console.find("Connected aircraft (0):") != std::string::npos);
  CHECK(console.find("Enter diagnostic code to clear:") != std::string::npos);
  CHECK(console.find("Invalid choice. Please enter 1, 2, 3, 4, or 5.") != std::string::npos);

  CHECK(test_helpers::logContains(
      logFile, "Cannot send DIAGNOSTIC state change command: aircraft 777 is not verified"));
  CHECK(test_helpers::logContains(
      logFile, "Cannot send diagnostic code clear command: aircraft 777 is not verified"));
}

TEST_CASE("REQ-CLT-041: MMA usability - runMenu handles invalid IDs and diagnostic code input") {
  test_helpers::ScopedTempWorkingDir env("awms_mma_usability_invalid_input");
  REQUIRE(env.ok());

  const std::string warrantyFile = "mma_warranty_data.csv";
  writeWarrantyCsv(warrantyFile, "");

  const std::string logFile = "mma_usability_invalid_input.log";
  configureMmaLogger(logFile);

  std::istringstream input("1\nabc\n4\nxyz\n5\nbadid\ncode\n5\n1001\nnot_number\n3\n");
  std::ostringstream output;
  ScopedConsoleRedirect redirect(input, output);

  MMA mma;
  mma.runMenu();

  spdlog::shutdown();

  const auto console = output.str();
  CHECK(console.find("Invalid ID.") != std::string::npos);
  CHECK(console.find("Invalid input.") != std::string::npos);
}