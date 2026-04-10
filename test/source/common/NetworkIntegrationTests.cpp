#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/StateManager.h>
#include <common/Packet.h>
#include <common/TcpConnection.h>
#include <doctest/doctest.h>
#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <array>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <regex>
#include <thread>

using namespace std::chrono_literals;

using namespace network;

namespace {

  using namespace std::chrono_literals;

  template <typename T>
  bool waitUntilReady(std::future<T>& future, std::chrono::milliseconds timeout = 2000ms) {
    return future.wait_for(timeout) == std::future_status::ready;
  }

  bool logContainsLine(const std::string& filename, const std::string& pattern) {
    std::ifstream file(filename);
    if (!file.is_open()) {
      return false;
    }

    std::regex re(pattern);
    std::string line;
    while (std::getline(file, line)) {
      if (std::regex_search(line, re)) {
        return true;
      }
    }

    return false;
  }

  std::vector<uint8_t> readBinaryFile(const std::string& filepath) {
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
                             std::vector<uint8_t>& payload_out,
                             std::chrono::milliseconds timeout = 5000ms) {
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
        std::this_thread::sleep_for(10ms);
        continue;
      } else {
        return false;
      }

      if (accumulated.size() < sizeof(network::PacketHeader)) {
        continue;
      }

      network::PacketHeader tentative_header{};
      std::memcpy(&tentative_header, accumulated.data(), sizeof(tentative_header));
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

  template <typename F>
  bool waitForCondition(F&& condition, std::chrono::milliseconds timeout = 4000ms) {
    const auto start = std::chrono::steady_clock::now();
    while (!condition()) {
      if (std::chrono::steady_clock::now() - start > timeout) {
        return false;
      }

      std::this_thread::sleep_for(50ms);
    }

    return true;
  }

}  // namespace

// ============================================================================
// REQ-NET-081/US-001: Our applications shall use TCP/IP to communicate
// ============================================================================

TEST_CASE("US-001: Integration - client and server communicate over TcpConnection") {
  asio::io_context io_context;
  using tcp = asio::ip::tcp;

  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 0));
  const uint16_t server_port = acceptor.local_endpoint().port();

  std::promise<TcpConnection::Ptr> accepted_connection_promise;
  auto accepted_connection_future = accepted_connection_promise.get_future();

  std::promise<PacketType> server_received_type_promise;
  auto server_received_type_future = server_received_type_promise.get_future();

  std::promise<StateId> client_received_state_promise;
  auto client_received_state_future = client_received_state_promise.get_future();

  std::atomic<bool> server_packet_recorded{false};
  std::atomic<bool> client_packet_recorded{false};

  acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
    if (ec) {
      accepted_connection_promise.set_exception(
          std::make_exception_ptr(std::runtime_error(ec.message())));
      return;
    }

    auto server_connection = TcpConnection::create(std::move(socket));
    server_connection->setMessageHandler(
        [&, server_connection](const std::vector<uint8_t>& packet) {
          PacketHeader header{};
          std::vector<uint8_t> payload;
          if (!deserializePacket(packet, header, payload)) {
            return;
          }

          if (header.type == PacketType::LANDED_NOTIFICATION
              && !server_packet_recorded.exchange(true)) {
            server_received_type_promise.set_value(header.type);

            const StateChangeRequest request{StateId::DIAGNOSTIC};
            const auto response = serializePacket(PacketType::STATE_CHANGE, request);
            server_connection->send(response);
          }
        });
    server_connection->start();
    accepted_connection_promise.set_value(server_connection);
  });

  tcp::socket client_socket(io_context);
  client_socket.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), server_port));
  auto client_connection = TcpConnection::create(std::move(client_socket));

  client_connection->setMessageHandler([&](const std::vector<uint8_t>& packet) {
    PacketHeader header{};
    std::vector<uint8_t> payload;
    if (!deserializePacket(packet, header, payload)) {
      return;
    }

    if (header.type != PacketType::STATE_CHANGE || payload.size() != sizeof(StateChangeRequest)
        || client_packet_recorded.exchange(true)) {
      return;
    }

    StateChangeRequest request{};
    std::memcpy(&request, payload.data(), sizeof(request));
    client_received_state_promise.set_value(request.target_state);
  });
  client_connection->start();

  std::thread io_thread([&]() { io_context.run(); });

  REQUIRE(waitUntilReady(accepted_connection_future));
  auto server_connection = accepted_connection_future.get();
  REQUIRE(server_connection != nullptr);

  const auto landed = serializePacket(PacketType::LANDED_NOTIFICATION, nullptr, 0);
  client_connection->send(landed);

  REQUIRE(waitUntilReady(server_received_type_future));
  CHECK(server_received_type_future.get() == PacketType::LANDED_NOTIFICATION);

  REQUIRE(waitUntilReady(client_received_state_future));
  CHECK(client_received_state_future.get() == StateId::DIAGNOSTIC);

  client_connection->close();
  server_connection->close();
  acceptor.close();
  io_context.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }
}

TEST_CASE("US-014: Integration - Successful verification flow") {
  // We first need to initialize a server from which we are listening.
  // A connection must be simulated for testing flows.
  MMA server;
  // client must be referenced directly from namespace.
  aircraft::Aircraft client;

  server.startServer(0);
  const auto port = server.getListeningPort();
  REQUIRE(port != 0);

  // Now we simulate a connection from the client.
  std::string host = "127.0.0.1";
  client.connectToMMA(host, port);

  // Time is allocated for TCP handshake.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // CHECK(server.getRunningStatus() == true);

  server.stopServer();
}

TEST_CASE("US-014: Integration - Command rejection when connection is UNVERIFIED") {
  MMA server;
  const uint16_t testPort = 8005;  // Use a fresh port
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  asio::io_context test_io;
  asio::ip::tcp::socket socket(test_io);

  std::error_code ec;
  socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), testPort), ec);
  REQUIRE(!ec);

  // 1. CLEAR THE BUFFER: The server sends a VerificationRequest immediately.
  // We need to consume that so it doesn't interfere with our EOF check.
  std::vector<uint8_t> dummy(1024);
  socket.read_some(asio::buffer(dummy), ec);
  // We don't care what's in 'dummy', we just want it out of the way.

  socket.non_blocking(true, ec);
  REQUIRE(!ec);

  // 2. SEND THE ILLEGAL COMMAND
  auto packet = network::serializePacket(network::PacketType::LANDED_NOTIFICATION, nullptr, 0);
  asio::write(socket, asio::buffer(packet));

  // 3. WAIT AND CHECK FOR CLOSURE
  // Poll in non-blocking mode so the test cannot hang indefinitely.
  bool closed = false;
  for (int i = 0; i < 50; ++i) {
    char buf[1];
    socket.read_some(asio::buffer(buf), ec);
    if (ec == asio::error::eof) {
      closed = true;
      break;
    }
    if (ec == asio::error::would_block || ec == asio::error::try_again) {
      std::this_thread::sleep_for(100ms);
      continue;
    }

    if (ec) {
      break;
    }

    std::this_thread::sleep_for(100ms);
  }

  CHECK(closed == true);
  CHECK(ec == asio::error::eof);

  server.stopServer();
}

TEST_CASE("REQ-SRV-053/REQ-SRV-055/REQ-SRV-057/US-011: MMA logs landed and state transitions") {
  const std::string testLogFile = "test_mma_landed_and_transition_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_mma_transition_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;

  const uint16_t testPort = 8006;
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client;
    StateManager stateManager;
    client.setStateManager(&stateManager);
    client.syncStateManagerToCurrentState();

    client.connectToMMA("127.0.0.1", testPort);

    REQUIRE(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile, "Client 12345 verified")
                 && logContainsLine(testLogFile, "Aircraft 12345 landed");
        },
        4000ms));

    server.sendDiagnosticStateChange(12345);

    CHECK(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile, "Aircraft 12345 landed")
                 && logContainsLine(testLogFile,
                                    "Sent DIAGNOSTIC state change command to aircraft 12345")
                 && logContainsLine(testLogFile, "Aircraft 12345 transitioned to DIAGNOSTIC state");
        },
        10000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-NET-013/US-011: DIAGNOSTIC_DATA severity is logged by MMA") {
  const std::string testLogFile = "test_mma_diagnostic_severity_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_mma_diagnostic_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  const uint16_t testPort = 8007;
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client;
    client.connectToMMA("127.0.0.1", testPort);

    REQUIRE(waitForCondition(
        [&]() { return logContainsLine(testLogFile, "Client 12345 verified"); }, 4000ms));

    CHECK(client.sendDiagnosticData());

    CHECK(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile,
                                 "Fault Code '101' \\(aircraft: 12345\\): \\[MINOR\\] - '.*'")
                 && logContainsLine(testLogFile,
                                    "Fault Code '102' \\(aircraft: 12345\\): \\[MAJOR\\] - '.*'");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE(
    "US-012: MMA can clear one diagnostic code when aircraft is in MAINTENANCE with explicit "
    "confirmation") {
  const std::string testLogFile = "test_mma_clear_diagnostic_code_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_mma_clear_code_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  const uint16_t testPort = 8008;
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client;
    StateManager stateManager;
    client.setStateManager(&stateManager);
    client.syncStateManagerToCurrentState();

    client.connectToMMA("127.0.0.1", testPort);

    REQUIRE(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile, "Client 12345 verified")
                 && logContainsLine(testLogFile, "Aircraft 12345 landed");
        },
        4000ms));

    server.sendDiagnosticStateChange(12345);
    spdlog::default_logger()->flush();

    REQUIRE(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile,
                                 "Sent DIAGNOSTIC state change command to aircraft 12345")
                 && logContainsLine(testLogFile,
                                    "Aircraft 12345 transitioned to MAINTENANCE state");
        },
        4000ms));

    server.sendDiagnosticCodeClearRequest(12345, 101);

    CHECK(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile,
                                 "Sent diagnostic code clear command to aircraft 12345 for "
                                 "code 101")
                 && logContainsLine(testLogFile,
                                    "Diagnostic code clear succeeded for aircraft 12345 \\(code: "
                                    "101, state: FAULT\\)");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("US-012: MMA allows clear request in FAULT state") {
  const std::string testLogFile = "test_mma_clear_diagnostic_code_blocked_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("test_mma_clear_code_blocked_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  const uint16_t testPort = 8009;
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  {
    aircraft::Aircraft client;
    StateManager stateManager;
    client.setStateManager(&stateManager);
    client.syncStateManagerToCurrentState();

    client.connectToMMA("127.0.0.1", testPort);

    REQUIRE(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile, "Client 12345 verified")
                 && logContainsLine(testLogFile, "Aircraft 12345 landed");
        },
        4000ms));

    server.sendDiagnosticStateChange(12345);
    spdlog::default_logger()->flush();

    REQUIRE(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile, "Aircraft 12345 transitioned to MAINTENANCE state");
        },
        4000ms));

    server.sendDiagnosticCodeClearRequest(12345, 101);

    REQUIRE(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile,
                                 "Diagnostic code clear succeeded for aircraft 12345 \\(code: "
                                 "101, state: FAULT\\)");
        },
        4000ms));

    server.sendDiagnosticCodeClearRequest(12345, 203);

    CHECK(waitForCondition(
        [&]() {
          return logContainsLine(
                     testLogFile,
                     "Sent diagnostic code clear command to aircraft 12345 for code 203")
                 && logContainsLine(
                     testLogFile,
                     "Diagnostic code clear succeeded for aircraft 12345 \\(code: 203, "
                     "state: FAULT\\)");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("Verification timeout handling.") {
  const std::string testLogFile = "/tmp/verification_timeout_test.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("verification_timeout_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  {
    aircraft::Aircraft client;
    client.initialize();

    // Use a non-routable destination so the client cannot complete connection/verification.
    client.connectToMMA("10.255.255.1", 65000);

    CHECK(waitForCondition([&]() { return client.getCurrentState() == "DIAGNOSTIC"; }, 8000ms));

    // Depending on host network routing, this may surface as timeout or immediate connect failure.
    CHECK(logContainsLine(testLogFile,
                          "Connection timeout, changing to DIAGNOSTIC|Connect failed: .*"));
    CHECK(logContainsLine(testLogFile, "Operational state transition: STANDBY -> DIAGNOSTIC"));
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("Invalid verification response handling.") {
  const std::string testLogFile = "/tmp/invalid_verification_response_test.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("invalid_verification_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  server.startServer(0);
  const uint16_t port = server.getListeningPort();
  REQUIRE(port != 0);

  asio::io_context io;
  asio::ip::tcp::socket socket(io);

  std::error_code ec;
  socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
  REQUIRE(!ec);

  // Read the server's verification request packet.
  std::vector<uint8_t> incoming(1024);
  size_t len = socket.read_some(asio::buffer(incoming), ec);
  REQUIRE(!ec);
  REQUIRE(len > 0);

  network::PacketHeader header{};
  std::vector<uint8_t> payload;
  REQUIRE(network::deserializePacket(std::vector<uint8_t>(incoming.begin(), incoming.begin() + len),
                                     header, payload));
  REQUIRE(header.type == network::PacketType::VERIFICATION_REQUEST);
  REQUIRE(payload.size() == sizeof(network::VerificationRequest));

  network::VerificationRequest request{};
  std::memcpy(&request, payload.data(), sizeof(request));

  // Send an invalid challenge response intentionally.
  network::VerificationResponse invalid_response{};
  invalid_response.challenge_response = request.challenge ^ 0xBAD0C0DE;
  invalid_response.client_id = 99999;
  const auto invalid_packet
      = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE, invalid_response);
  asio::write(socket, asio::buffer(invalid_packet), ec);
  REQUIRE(!ec);

  socket.non_blocking(true, ec);
  REQUIRE(!ec);

  // Server should close the socket after failed verification.
  bool closed = false;
  for (int i = 0; i < 50; ++i) {
    char probe[1];
    socket.read_some(asio::buffer(probe), ec);
    if (ec == asio::error::eof || ec == asio::error::connection_reset) {
      closed = true;
      break;
    }
    if (ec == asio::error::would_block || ec == asio::error::try_again) {
      std::this_thread::sleep_for(100ms);
      continue;
    }

    if (ec) {
      break;
    }

    std::this_thread::sleep_for(100ms);
  }

  CHECK(closed);
  CHECK(logContainsLine(testLogFile, "Verification failed, closing connection"));

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

// ============================================================================
// Image transfer integration: Client sends multi-chunk image to Server
// ============================================================================

TEST_CASE("Integration: Aircraft sends multi-chunk image to MMA") {
  const std::string saved_image_path = "res/recv/aircraft_12345_image_1.png";
  std::remove(saved_image_path.c_str());

  // Setup MMA server
  const std::string testLogFile = "/tmp/image_transfer_test.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("file_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  {
    MMA server;
    server.initialize();

    std::thread server_thread([&]() { server.startServer(9001); });

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Setup Aircraft client
    aircraft::Aircraft client;
    client.initialize();

    std::thread client_thread([&]() { client.connectToMMA("127.0.0.1", 9001); });

    // Give client time to connect and verify
    std::this_thread::sleep_for(1000ms);

    CHECK(client.getRunningStatus());

    // Now test image transmission
    // Create a 2MB test image
    const size_t image_size = 2 * 1024 * 1024;
    std::vector<uint8_t> test_image(image_size);
    for (size_t i = 0; i < image_size; ++i) {
      test_image[i] = static_cast<uint8_t>(i % 256);
    }

    // Send image
    CHECK(client.sendImage(test_image, ImageFormat::PNG));

    // Wait for server to receive and reassemble all chunks
    CHECK(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile,
                                 "Image .* received and reassembled \\(2097152 bytes.*");
        },
        10000ms));

    CHECK(waitForCondition(
        [&]() { return logContainsLine(testLogFile, "Image integrity verified byte-for-byte:"); },
        5000ms));

    // Verify correct log entries for chunk reception
    CHECK(logContainsLine(testLogFile, "Received image chunk"));
    CHECK(logContainsLine(testLogFile, "format: PNG"));

    // Verify persisted file is byte-for-byte identical to original image.
    CHECK(std::filesystem::exists(saved_image_path));
    const auto saved_image_bytes = readBinaryFile(saved_image_path);
    CHECK(saved_image_bytes == test_image);

    server.stopServer();
    client_thread.join();
    server_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
  std::remove(saved_image_path.c_str());
}

TEST_CASE("Integration: Image transfer with small payload") {
  const std::string saved_image_path = "res/recv/aircraft_12345_image_1.jpg";
  std::remove(saved_image_path.c_str());

  // Setup MMA server
  const std::string testLogFile = "/tmp/image_transfer_small_test.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("file_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  {
    MMA server;
    server.initialize();

    std::thread server_thread([&]() { server.startServer(9002); });

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Setup Aircraft client
    aircraft::Aircraft client;
    client.initialize();

    std::thread client_thread([&]() { client.connectToMMA("127.0.0.1", 9002); });

    // Give client time to connect and verify
    std::this_thread::sleep_for(1000ms);

    CHECK(client.getRunningStatus());

    // Send small image (single chunk)
    const std::vector<uint8_t> small_image(10240, 0xAB);  // 10KB
    CHECK(client.sendImage(small_image, ImageFormat::JPEG));

    // Wait for server to receive
    CHECK(waitForCondition(
        [&]() {
          return logContainsLine(testLogFile,
                                 "Image .* received and reassembled \\(10240 bytes.*format: JPEG");
        },
        5000ms));

    CHECK(waitForCondition(
        [&]() { return logContainsLine(testLogFile, "Image integrity verified byte-for-byte:"); },
        5000ms));

    CHECK(std::filesystem::exists(saved_image_path));
    const auto saved_image_bytes = readBinaryFile(saved_image_path);
    CHECK(saved_image_bytes == small_image);

    server.stopServer();
    client_thread.join();
    server_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
  std::remove(saved_image_path.c_str());
}

TEST_CASE("Integration: MMA requests and receives missing image chunk retry") {
  const std::string testLogFile = "/tmp/image_retry_request_success_test.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("image_retry_success_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  server.startServer(0);
  const uint16_t port = server.getListeningPort();
  REQUIRE(port != 0);

  asio::io_context io;
  asio::ip::tcp::socket socket(io);
  std::error_code ec;
  socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
  REQUIRE(!ec);

  network::PacketHeader incoming_header{};
  std::vector<uint8_t> incoming_payload;
  REQUIRE(readPacketWithTimeout(socket, incoming_header, incoming_payload, 3000ms));
  REQUIRE(incoming_header.type == network::PacketType::VERIFICATION_REQUEST);
  REQUIRE(incoming_payload.size() == sizeof(network::VerificationRequest));

  network::VerificationRequest verification_request{};
  std::memcpy(&verification_request, incoming_payload.data(), sizeof(verification_request));
  network::VerificationResponse verification_response{};
  verification_response.challenge_response = verification_request.challenge ^ 0xDEADBEEF;
  verification_response.client_id = 12345;
  const auto verification_response_packet
      = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE, verification_response);
  asio::write(socket, asio::buffer(verification_response_packet), ec);
  REQUIRE(!ec);

  const std::vector<uint8_t> image_data{0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70};
  const uint32_t image_id = 9001;
  const uint32_t image_crc = network::Crc32::calculate(image_data.data(), image_data.size());

  const std::vector<uint8_t> chunk0_data{0x10, 0x20, 0x30};
  const std::vector<uint8_t> chunk1_data{0x40, 0x50, 0x60, 0x70};

  const network::ImageChunkHeader chunk0_header{
      image_id,
      0,
      2,
      static_cast<uint32_t>(chunk0_data.size()),
      network::ImageFormat::RAW,
      network::Crc32::calculate(chunk0_data.data(), chunk0_data.size()),
      image_crc};
  const network::ImageChunkHeader chunk1_header{
      image_id,
      1,
      2,
      static_cast<uint32_t>(chunk1_data.size()),
      network::ImageFormat::RAW,
      network::Crc32::calculate(chunk1_data.data(), chunk1_data.size()),
      image_crc};

  std::vector<uint8_t> chunk0_payload(sizeof(chunk0_header) + chunk0_data.size());
  std::memcpy(chunk0_payload.data(), &chunk0_header, sizeof(chunk0_header));
  std::memcpy(chunk0_payload.data() + sizeof(chunk0_header), chunk0_data.data(),
              chunk0_data.size());

  std::vector<uint8_t> chunk1_payload(sizeof(chunk1_header) + chunk1_data.size());
  std::memcpy(chunk1_payload.data(), &chunk1_header, sizeof(chunk1_header));
  std::memcpy(chunk1_payload.data() + sizeof(chunk1_header), chunk1_data.data(),
              chunk1_data.size());

  // Send only first chunk so MMA must request retry for chunk 1.
  const auto chunk0_packet = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK,
                                                      chunk0_payload.data(), chunk0_payload.size());
  asio::write(socket, asio::buffer(chunk0_packet), ec);
  REQUIRE(!ec);

  network::PacketHeader retry_header{};
  std::vector<uint8_t> retry_payload;
  REQUIRE(readPacketWithTimeout(socket, retry_header, retry_payload, 5000ms));
  REQUIRE(retry_header.type == network::PacketType::SCHEMATIC_CHUNK_RETRY_REQUEST);
  REQUIRE(retry_payload.size() == sizeof(network::SchematicChunkRetryRequest));

  network::SchematicChunkRetryRequest retry_request{};
  std::memcpy(&retry_request, retry_payload.data(), sizeof(retry_request));
  CHECK(retry_request.image_id == image_id);
  CHECK(retry_request.chunk_index == 1);

  const auto chunk1_packet = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK,
                                                      chunk1_payload.data(), chunk1_payload.size());
  asio::write(socket, asio::buffer(chunk1_packet), ec);
  REQUIRE(!ec);

  CHECK(waitForCondition(
      [&]() {
        return logContainsLine(testLogFile,
                               "Image 9001 from aircraft 12345 received and reassembled");
      },
      5000ms));

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("Integration: MMA logs error when missing chunk retry fails") {
  const std::string testLogFile = "/tmp/image_retry_request_failure_test.log";
  std::remove(testLogFile.c_str());

  auto logger_source = spdlog::basic_logger_mt("image_retry_failure_logger", testLogFile);
  spdlog::set_default_logger(logger_source);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  MMA server;
  server.startServer(0);
  const uint16_t port = server.getListeningPort();
  REQUIRE(port != 0);

  asio::io_context io;
  asio::ip::tcp::socket socket(io);
  std::error_code ec;
  socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
  REQUIRE(!ec);

  network::PacketHeader incoming_header{};
  std::vector<uint8_t> incoming_payload;
  REQUIRE(readPacketWithTimeout(socket, incoming_header, incoming_payload, 3000ms));
  REQUIRE(incoming_header.type == network::PacketType::VERIFICATION_REQUEST);
  REQUIRE(incoming_payload.size() == sizeof(network::VerificationRequest));

  network::VerificationRequest verification_request{};
  std::memcpy(&verification_request, incoming_payload.data(), sizeof(verification_request));
  network::VerificationResponse verification_response{};
  verification_response.challenge_response = verification_request.challenge ^ 0xDEADBEEF;
  verification_response.client_id = 12345;
  const auto verification_response_packet
      = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE, verification_response);
  asio::write(socket, asio::buffer(verification_response_packet), ec);
  REQUIRE(!ec);

  const std::vector<uint8_t> chunk0_data{0xAA, 0xBB, 0xCC};
  const std::vector<uint8_t> full_image_data{0xAA, 0xBB, 0xCC, 0xDD};
  const uint32_t image_id = 9002;
  const uint32_t image_crc
      = network::Crc32::calculate(full_image_data.data(), full_image_data.size());

  const network::ImageChunkHeader chunk0_header{
      image_id,
      0,
      2,
      static_cast<uint32_t>(chunk0_data.size()),
      network::ImageFormat::RAW,
      network::Crc32::calculate(chunk0_data.data(), chunk0_data.size()),
      image_crc};
  std::vector<uint8_t> chunk0_payload(sizeof(chunk0_header) + chunk0_data.size());
  std::memcpy(chunk0_payload.data(), &chunk0_header, sizeof(chunk0_header));
  std::memcpy(chunk0_payload.data() + sizeof(chunk0_header), chunk0_data.data(),
              chunk0_data.size());

  const auto chunk0_packet = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK,
                                                      chunk0_payload.data(), chunk0_payload.size());
  asio::write(socket, asio::buffer(chunk0_packet), ec);
  REQUIRE(!ec);

  // Do not send missing chunk 1 even after retry request.
  CHECK(waitForCondition(
      [&]() {
        return logContainsLine(testLogFile,
                               "Requested retry for missing chunk 1 of image 9002 from aircraft "
                               "12345");
      },
      6000ms));

  CHECK(waitForCondition(
      [&]() {
        return logContainsLine(testLogFile,
                               "Image 9002 from aircraft 12345 missing chunk 1 after retry; "
                               "aborting reassembly");
      },
      8000ms));

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}
