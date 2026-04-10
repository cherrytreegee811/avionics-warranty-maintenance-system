#pragma once

#include <chrono>
#include <fstream>
#include <regex>
#include <string>
#include <thread>

namespace test_helpers {

  template <typename F> bool waitFor(F&& condition, int timeout_ms = 3000);

  bool logContains(const std::string& filename, const std::string& pattern);
  bool logFileNonEmpty(const std::string& filename);

}  // namespace test_helpers

template <typename F> bool test_helpers::waitFor(F&& condition, int timeout_ms) {
  auto start = std::chrono::steady_clock::now();
  while (!condition()) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                              - start)
            .count()
        > timeout_ms)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return true;
}  // namespace test_helpers