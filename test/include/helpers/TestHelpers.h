#pragma once

#include <common/Packet.h>

#include <asio.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace mma {
  class MMA;
}

namespace test_helpers {

  template <typename F> bool waitFor(F&& condition, int timeout_ms = 3000);

  template <typename F> bool waitFor(F&& condition, std::chrono::milliseconds timeout);

  bool logContains(const std::string& filename, const std::string& pattern);
  bool logFileNonEmpty(const std::string& filename);

  template <typename T>
  bool waitUntilReady(std::future<T>& future,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds{2000});

  std::vector<uint8_t> readBinaryFile(const std::filesystem::path& filepath);

  bool readPacketWithTimeout(asio::ip::tcp::socket& socket, network::PacketHeader& header_out,
                             std::vector<uint8_t>& payload_out,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

  class ScopedTempWorkingDir {
  public:
    explicit ScopedTempWorkingDir(const std::string& prefix);

    ScopedTempWorkingDir(const ScopedTempWorkingDir&) = delete;
    ScopedTempWorkingDir& operator=(const ScopedTempWorkingDir&) = delete;

    bool ok() const { return ok_; }
    const std::filesystem::path& dir() const { return dir_; }

    ~ScopedTempWorkingDir();

  private:
    std::filesystem::path old_cwd_;
    std::filesystem::path dir_;
    bool ok_ = false;
  };

  class ScopedMmaStopper {
  public:
    explicit ScopedMmaStopper(mma::MMA& server);

    ScopedMmaStopper(const ScopedMmaStopper&) = delete;
    ScopedMmaStopper& operator=(const ScopedMmaStopper&) = delete;

    ~ScopedMmaStopper();

  private:
    mma::MMA& server_;
  };

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

template <typename F> bool test_helpers::waitFor(F&& condition, std::chrono::milliseconds timeout) {
  const auto timeout_ms = static_cast<int>(timeout.count());
  return waitFor(std::forward<F>(condition), timeout_ms);
}

template <typename T>
bool test_helpers::waitUntilReady(std::future<T>& future, std::chrono::milliseconds timeout) {
  return future.wait_for(timeout) == std::future_status::ready;
}