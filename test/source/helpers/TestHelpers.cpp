#include <common/Packet.h>
#include <helpers/TestHelpers.h>
#include <mma/mma.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <regex>
#include <thread>

namespace test_helpers {

  bool logContains(const std::string& filename, const std::string& pattern) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    std::regex re(pattern);
    std::string line;
    while (std::getline(file, line)) {
      if (std::regex_search(line, re)) return true;
    }
    return false;
  }

  bool logFileNonEmpty(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    return file.is_open() && file.tellg() > 0;
  }

  std::vector<uint8_t> readBinaryFile(const std::filesystem::path& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return {};
    }

    const auto file_size_pos = file.tellg();
    if (file_size_pos < 0) {
      return {};
    }

    const size_t file_size = static_cast<size_t>(file_size_pos);
    std::vector<uint8_t> data(file_size);
    file.seekg(0, std::ios::beg);

    if (file_size > 0) {
      file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(file_size));
      if (!file || file.gcount() != static_cast<std::streamsize>(file_size)) {
        return {};
      }
    }

    return data;
  }

  bool readPacketWithTimeout(asio::ip::tcp::socket& socket, network::PacketHeader& header_out,
                             std::vector<uint8_t>& payload_out, std::chrono::milliseconds timeout) {
    std::error_code ec;
    socket.non_blocking(true, ec);
    if (ec) {
      return false;
    }

    std::vector<uint8_t> accumulated;
    accumulated.reserve(2048);
    std::array<uint8_t, 4096> read_buffer{};

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
      const size_t read_bytes = socket.read_some(asio::buffer(read_buffer), ec);
      if (!ec) {
        accumulated.insert(accumulated.end(), read_buffer.begin(),
                           read_buffer.begin() + read_bytes);
      } else if (ec == asio::error::would_block || ec == asio::error::try_again) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        continue;
      } else {
        return false;
      }

      if (accumulated.size() < sizeof(network::PacketHeader)) {
        continue;
      }

      network::PacketHeader tentative_header{};
      (void)std::memcpy(&tentative_header, accumulated.data(), sizeof(tentative_header));
      const size_t total_packet_size
          = sizeof(network::PacketHeader) + tentative_header.payload_size;
      if (accumulated.size() < total_packet_size) {
        continue;
      }

      const std::vector<uint8_t> packet_bytes(accumulated.begin(),
                                              accumulated.begin() + total_packet_size);
      return network::deserializePacket(packet_bytes, header_out, payload_out);
    }

    return false;
  }

  ScopedTempWorkingDir::ScopedTempWorkingDir(const std::string& prefix)
      : old_cwd_(std::filesystem::current_path()) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tid_hash
        = static_cast<unsigned long long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    dir_ = std::filesystem::temp_directory_path()
           / (prefix + "_" + std::to_string(stamp) + "_" + std::to_string(tid_hash));

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
      return;
    }

    std::filesystem::current_path(dir_, ec);
    if (ec) {
      dir_.clear();
      return;
    }

    ok_ = true;
  }

  ScopedTempWorkingDir::~ScopedTempWorkingDir() {
    // Ensure file sinks are closed before cleanup (important if the test aborts early).
    spdlog::shutdown();

    std::error_code ec;
    if (!old_cwd_.empty()) {
      std::filesystem::current_path(old_cwd_, ec);
    }
    if (!dir_.empty()) {
      std::filesystem::remove_all(dir_, ec);
    }
  }

  ScopedMmaStopper::ScopedMmaStopper(mma::MMA& server) : server_(server) {}

  ScopedMmaStopper::~ScopedMmaStopper() { server_.stopServer(); }

}  // namespace test_helpers