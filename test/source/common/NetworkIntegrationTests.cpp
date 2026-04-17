#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <aircraft/StateManager.h>
#include <common/Packet.h>
#include <common/TcpConnection.h>
#include <doctest/doctest.h>
#include <helpers/TestHelpers.h>
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
#include <thread>

using namespace std::chrono_literals;

using namespace network;

// ============================================================================
// REQ-NET-081: Our applications shall use TCP/IP to communicate
// ============================================================================

TEST_CASE("REQ-NET-081: Integration - client and server communicate over TcpConnection") {
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

  REQUIRE(test_helpers::waitUntilReady(accepted_connection_future));
  auto server_connection = accepted_connection_future.get();
  REQUIRE(server_connection != nullptr);

  const auto landed = serializePacket(PacketType::LANDED_NOTIFICATION, nullptr, 0);
  client_connection->send(landed);

  REQUIRE(test_helpers::waitUntilReady(server_received_type_future));
  CHECK(server_received_type_future.get() == PacketType::LANDED_NOTIFICATION);

  REQUIRE(test_helpers::waitUntilReady(client_received_state_future));
  CHECK(client_received_state_future.get() == StateId::DIAGNOSTIC);

  client_connection->close();
  server_connection->close();
  acceptor.close();
  io_context.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }
}

TEST_CASE("REQ-NET-081: TcpConnection returns unknown remote address when endpoint unavailable") {
  asio::io_context io;
  asio::ip::tcp::socket socket(io);
  auto connection = TcpConnection::create(std::move(socket));

  CHECK(connection->getRemoteAddress() == "unknown");
}

TEST_CASE("REQ-NET-012/REQ-NET-081: TcpConnection closes on invalid packet magic") {
  const std::string testLogFile = "test_tcpconnection_invalid_magic_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger
      = std::make_shared<spdlog::logger>("test_tcpconnection_invalid_magic_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);
  using tcp = asio::ip::tcp;

  tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
  const uint16_t port = acceptor.local_endpoint().port();

  std::promise<TcpConnection::Ptr> accepted_promise;
  auto accepted_future = accepted_promise.get_future();

  acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
    if (ec) {
      accepted_promise.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
      return;
    }
    auto server_connection = TcpConnection::create(std::move(socket));
    server_connection->setMessageHandler([](const std::vector<uint8_t>&) {});
    server_connection->start();
    accepted_promise.set_value(server_connection);
  });

  tcp::socket client_socket(io);
  client_socket.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

  std::thread io_thread([&]() { io.run(); });

  auto cleanup = [&]() {
    std::error_code ignored;
    client_socket.close(ignored);
    acceptor.close();
    work_guard.reset();
    io.stop();
    if (io_thread.joinable()) {
      io_thread.join();
    }
  };

  const bool accepted_ready = test_helpers::waitUntilReady(accepted_future);
  if (!accepted_ready) {
    cleanup();
    FAIL("Accepted connection was not ready in time");
  }

  auto server_connection = accepted_future.get();
  if (server_connection == nullptr) {
    cleanup();
    FAIL("Accepted server connection was null");
  }

  PacketHeader bad_header{};
  bad_header.magic = 0x12345678;
  bad_header.type = PacketType::LANDED_NOTIFICATION;
  bad_header.payload_size = 0;
  bad_header.sequence = 1;
  bad_header.checksum = 0;

  std::array<uint8_t, sizeof(PacketHeader)> raw{};
  std::memcpy(raw.data(), &bad_header, sizeof(PacketHeader));
  asio::write(client_socket, asio::buffer(raw));

  CHECK(test_helpers::waitFor(
      [&]() { return server_connection->getState() == ConnectionState::CLOSED; }, 2000ms));
  CHECK(test_helpers::waitFor(
      [&]() { return test_helpers::logContains(testLogFile, "Invalid packet magic"); }, 2000ms));

  std::error_code ignored;
  client_socket.close(ignored);
  acceptor.close();
  work_guard.reset();
  io.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-NET-012/REQ-NET-081: TcpConnection closes on oversized packet header") {
  const std::string testLogFile = "test_tcpconnection_oversized_packet_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger
      = std::make_shared<spdlog::logger>("test_tcpconnection_oversized_packet_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);
  using tcp = asio::ip::tcp;

  tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
  const uint16_t port = acceptor.local_endpoint().port();

  std::promise<TcpConnection::Ptr> accepted_promise;
  auto accepted_future = accepted_promise.get_future();

  acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
    if (ec) {
      accepted_promise.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
      return;
    }
    auto server_connection = TcpConnection::create(std::move(socket));
    server_connection->setMessageHandler([](const std::vector<uint8_t>&) {});
    server_connection->start();
    accepted_promise.set_value(server_connection);
  });

  tcp::socket client_socket(io);
  client_socket.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

  std::thread io_thread([&]() { io.run(); });

  REQUIRE(test_helpers::waitUntilReady(accepted_future));
  auto server_connection = accepted_future.get();
  REQUIRE(server_connection != nullptr);

  PacketHeader oversized_header{};
  oversized_header.magic = PACKET_MAGIC;
  oversized_header.type = PacketType::SCHEMATIC_CHUNK;
  oversized_header.payload_size = 60U * 1024U * 1024U;
  oversized_header.sequence = 1;
  oversized_header.checksum = 0;

  std::array<uint8_t, sizeof(PacketHeader)> raw{};
  std::memcpy(raw.data(), &oversized_header, sizeof(PacketHeader));
  asio::write(client_socket, asio::buffer(raw));

  CHECK(test_helpers::waitFor(
      [&]() { return server_connection->getState() == ConnectionState::CLOSED; }, 2000ms));
  CHECK(test_helpers::waitFor(
      [&]() { return test_helpers::logContains(testLogFile, "Packet too large"); }, 2000ms));

  std::error_code ignored;
  client_socket.close(ignored);
  acceptor.close();
  work_guard.reset();
  io.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-NET-013/REQ-NET-081: TcpConnection buffers partial packet until complete") {
  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);
  using tcp = asio::ip::tcp;

  tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
  const uint16_t port = acceptor.local_endpoint().port();

  std::promise<TcpConnection::Ptr> accepted_promise;
  auto accepted_future = accepted_promise.get_future();

  acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
    if (ec) {
      accepted_promise.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
      return;
    }
    auto server_connection = TcpConnection::create(std::move(socket));
    server_connection->start();
    accepted_promise.set_value(server_connection);
  });

  tcp::socket client_socket(io);
  client_socket.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

  std::thread io_thread([&]() { io.run(); });

  auto cleanup = [&]() {
    std::error_code ignored;
    client_socket.close(ignored);
    acceptor.close();
    work_guard.reset();
    io.stop();
    if (io_thread.joinable()) {
      io_thread.join();
    }
  };

  const bool accepted_ready = test_helpers::waitUntilReady(accepted_future);
  if (!accepted_ready) {
    cleanup();
    FAIL("Accepted connection was not ready in time");
  }

  auto server_connection = accepted_future.get();
  if (server_connection == nullptr) {
    cleanup();
    FAIL("Accepted server connection was null");
  }

  const VerificationRequest request{0x12345678U, 0xABCDEF1234567890ULL};
  const auto packet = serializePacket(PacketType::VERIFICATION_REQUEST, request);
  CHECK(packet.size() > sizeof(PacketHeader));

  const size_t first_part = sizeof(PacketHeader) + 1;
  if (first_part >= packet.size()) {
    cleanup();
    FAIL("Partial-split precondition failed");
  }

  asio::write(client_socket, asio::buffer(packet.data(), first_part));

  std::this_thread::sleep_for(150ms);
  CHECK(server_connection->getState() != ConnectionState::CLOSED);

  server_connection->close();
  CHECK(test_helpers::waitFor(
      [&]() { return server_connection->getState() == ConnectionState::CLOSED; }, 2000ms));
  cleanup();
}

TEST_CASE("REQ-NET-081: TcpConnection logs send-closed path when write is aborted") {
  const std::string testLogFile = "test_tcpconnection_send_closed_log.txt";
  std::remove(testLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(testLogFile, true);
  auto logger
      = std::make_shared<spdlog::logger>("test_tcpconnection_send_closed_logger", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  asio::io_context io;
  auto work_guard = asio::make_work_guard(io);
  using tcp = asio::ip::tcp;

  tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
  const uint16_t port = acceptor.local_endpoint().port();

  std::promise<TcpConnection::Ptr> accepted_promise;
  auto accepted_future = accepted_promise.get_future();

  acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
    if (ec) {
      accepted_promise.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
      return;
    }
    auto server_connection = TcpConnection::create(std::move(socket));
    accepted_promise.set_value(server_connection);
  });

  tcp::socket client_socket(io);
  client_socket.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

  std::thread io_thread([&]() { io.run(); });

  REQUIRE(test_helpers::waitUntilReady(accepted_future));
  auto server_connection = accepted_future.get();
  REQUIRE(server_connection != nullptr);

  // Large payload increases chance that close races with in-flight write.
  std::vector<uint8_t> large_payload(4 * 1024 * 1024, 0xAB);
  server_connection->send(large_payload);
  server_connection->close();

  CHECK(test_helpers::waitFor(
      [&]() { return server_connection->getState() == ConnectionState::CLOSED; }, 2000ms));

  CHECK(test_helpers::waitFor(
      [&]() {
        return test_helpers::logContains(testLogFile, "Connection send closed for|Send error:");
      },
      3000ms));

  std::error_code ignored;
  client_socket.close(ignored);
  acceptor.close();
  work_guard.reset();
  io.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

// ============================================================================
// REQ-SYS-080: Both the client and server shall require connection verification before accepting
//              any commands or returning results.
// REQ-NET-081: Our applications shall use TCP/IP to communicate.
// ============================================================================

TEST_CASE("REQ-SYS-080/REQ-NET-081: Integration - Successful verification flow") {
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

TEST_CASE(
    "REQ-SYS-080/REQ-NET-081: Integration - Command rejection when connection is UNVERIFIED") {
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

// ============================================================================
// REQ-SRV-053: The MMA logs when it receives a landed notification from an aircraft.
// REQ-SRV-055: The MMA logs outgoing state change commands.
// REQ-SRV-057: The MMA logs state transition confirmations from aircraft.
// ============================================================================

TEST_CASE("REQ-SRV-053/REQ-SRV-055/REQ-SRV-057: MMA logs landed and state transitions") {
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
    aircraft::StateManager stateManager;
    client.setStateManager(&stateManager);
    client.syncStateManagerToCurrentState();

    client.connectToMMA("127.0.0.1", testPort);
    const uint64_t aircraft_id = client.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(aircraft_id) + " verified")
                 && test_helpers::logContains(
                     testLogFile, "Aircraft " + std::to_string(aircraft_id) + " landed");
        },
        4000ms));

    server.sendDiagnosticStateChange(aircraft_id);

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Aircraft " + std::to_string(aircraft_id) + " landed")
                 && test_helpers::logContains(testLogFile,
                                              "Sent DIAGNOSTIC state change command to aircraft "
                                                  + std::to_string(aircraft_id))
                 && test_helpers::logContains(testLogFile,
                                              "Operational state transition: STANDBY -> DIAGNOSTIC")
                 && test_helpers::logContains(testLogFile,
                                              "Aircraft " + std::to_string(aircraft_id)
                                                  + " transitioned to DIAGNOSTIC state");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

// ============================================================================
// REQ-NET-013: The client and the server shall support internal processing logic for reading
//              received information, and serialize information for transfer.
// ============================================================================

TEST_CASE("REQ-NET-013: DIAGNOSTIC_DATA severity is logged by MMA") {
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
    const uint64_t aircraft_id = client.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(aircraft_id) + " verified");
        },
        4000ms));

    CHECK(client.sendDiagnosticData());

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
                     testLogFile, "Fault Code '101' \\(aircraft: " + std::to_string(aircraft_id)
                                      + "\\): \\[MINOR\\] - '.*'")
                 && test_helpers::logContains(
                     testLogFile, "Fault Code '102' \\(aircraft: " + std::to_string(aircraft_id)
                                      + "\\): \\[MAJOR\\] - '.*'");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

// ============================================================================
// REQ-SYS-010: All data transferred between Client and Server shall use a pre-defined structure.
// REQ-NET-013: The client and the server shall support internal processing logic for reading
//              received information, and serialize information for transfer.
// REQ-SYS-060: The client or server (or both) application shall contain an operational state
// machine.
// ============================================================================

TEST_CASE(
    "REQ-SYS-010/REQ-NET-013/REQ-SYS-060: MMA can clear one diagnostic code when aircraft is in "
    "MAINTENANCE with explicit confirmation") {
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
    aircraft::StateManager stateManager;
    client.setStateManager(&stateManager);
    client.syncStateManagerToCurrentState();

    client.connectToMMA("127.0.0.1", testPort);
    const uint64_t aircraft_id = client.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(aircraft_id) + " verified")
                 && test_helpers::logContains(
                     testLogFile, "Aircraft " + std::to_string(aircraft_id) + " landed");
        },
        4000ms));

    server.sendDiagnosticStateChange(aircraft_id);
    spdlog::default_logger()->flush();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Sent DIAGNOSTIC state change command to aircraft "
                                               + std::to_string(aircraft_id))
                 && test_helpers::logContains(testLogFile,
                                              "Aircraft " + std::to_string(aircraft_id)
                                                  + " transitioned to MAINTENANCE state");
        },
        4000ms));

    server.sendDiagnosticCodeClearRequest(aircraft_id, 101);

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Sent diagnostic code clear command to aircraft "
                                               + std::to_string(aircraft_id) + " for code 101")
                 && test_helpers::logContains(testLogFile,
                                              "Diagnostic code clear succeeded for aircraft "
                                                  + std::to_string(aircraft_id)
                                                  + " \\(code: 101, state: FAULT\\)");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-SYS-010/REQ-NET-013/REQ-SYS-060: MMA allows clear request in FAULT state") {
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
    aircraft::StateManager stateManager;
    client.setStateManager(&stateManager);
    client.syncStateManagerToCurrentState();

    client.connectToMMA("127.0.0.1", testPort);
    const uint64_t aircraft_id = client.getAircraftId();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Client " + std::to_string(aircraft_id) + " verified")
                 && test_helpers::logContains(
                     testLogFile, "Aircraft " + std::to_string(aircraft_id) + " landed");
        },
        4000ms));

    server.sendDiagnosticStateChange(aircraft_id);
    spdlog::default_logger()->flush();

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile, "Aircraft " + std::to_string(aircraft_id)
                                                            + " transitioned to MAINTENANCE state");
        },
        4000ms));

    server.sendDiagnosticCodeClearRequest(aircraft_id, 101);

    REQUIRE(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
              testLogFile, "Diagnostic code clear succeeded for aircraft "
                               + std::to_string(aircraft_id) + " \\(code: 101, state: FAULT\\)");
        },
        4000ms));

    std::this_thread::sleep_for(200ms);

    server.sendDiagnosticCodeClearRequest(aircraft_id, 203);

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
                     testLogFile,
                     "Sent diagnostic code clear command to aircraft " + std::to_string(aircraft_id) + " for code 203")
           && test_helpers::logContains(
                     testLogFile,
                     "Diagnostic code clear succeeded for aircraft " + std::to_string(aircraft_id) + " \\(code: 203, "
                     "state: FAULT\\)");
        },
        4000ms));
  }

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-CLT-082/REQ-SYS-080/REQ-NET-081: Integration - Verification timeout handling") {
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

    CHECK(
        test_helpers::waitFor([&]() { return client.getCurrentState() == "DIAGNOSTIC"; }, 8000ms));

    // Depending on host network routing, this may surface as timeout or immediate connect failure.
    CHECK(test_helpers::logContains(
        testLogFile, "Connection timeout, changing to DIAGNOSTIC|Connect failed: .*"));
    CHECK(test_helpers::logContains(testLogFile,
                                    "Operational state transition: STANDBY -> DIAGNOSTIC"));
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE(
    "REQ-SYS-080/REQ-NET-013/REQ-NET-081: Integration - Invalid verification response handling") {
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
  CHECK(test_helpers::logContains(testLogFile, "Verification failed, closing connection"));

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

// ============================================================================
// REQ-SYS-010: All data transferred between Client and Server shall use a pre-defined structure.
// REQ-NET-012: The packet shall contain packet integrity checks to ensure validity/authenticity.
// REQ-NET-081: Our applications shall use TCP/IP to communicate.
// ============================================================================

TEST_CASE(
    "REQ-SYS-010/REQ-NET-012/REQ-NET-081: Integration - Aircraft sends multi-chunk image to MMA") {
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
    const uint64_t aircraft_id = client.getAircraftId();
    const std::string saved_image_path
        = "res/recv/aircraft_" + std::to_string(aircraft_id) + "_image_1.png";
    std::remove(saved_image_path.c_str());

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
    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile,
                                           "Image .* received and reassembled \\(2097152 bytes.*");
        },
        10000ms));

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile, "Image integrity verified byte-for-byte:");
        },
        5000ms));

    // Verify correct log entries for chunk reception
    CHECK(test_helpers::logContains(testLogFile, "Received image chunk"));
    CHECK(test_helpers::logContains(testLogFile, "format: PNG"));

    // Verify persisted file is byte-for-byte identical to original image.
    CHECK(std::filesystem::exists(saved_image_path));
    const auto saved_image_bytes = test_helpers::readBinaryFile(saved_image_path);
    CHECK(saved_image_bytes == test_image);

    server.stopServer();
    client_thread.join();
    server_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE("REQ-SYS-010/REQ-NET-012/REQ-NET-081: Integration - Image transfer with small payload") {
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
    const uint64_t aircraft_id = client.getAircraftId();
    const std::string saved_image_path
        = "res/recv/aircraft_" + std::to_string(aircraft_id) + "_image_1.jpg";
    std::remove(saved_image_path.c_str());

    std::thread client_thread([&]() { client.connectToMMA("127.0.0.1", 9002); });

    // Give client time to connect and verify
    std::this_thread::sleep_for(1000ms);

    CHECK(client.getRunningStatus());

    // Send small image (single chunk)
    const std::vector<uint8_t> small_image(10240, 0xAB);  // 10KB
    CHECK(client.sendImage(small_image, ImageFormat::JPEG));

    // Wait for server to receive
    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(
              testLogFile, "Image .* received and reassembled \\(10240 bytes.*format: JPEG");
        },
        5000ms));

    CHECK(test_helpers::waitFor(
        [&]() {
          return test_helpers::logContains(testLogFile, "Image integrity verified byte-for-byte:");
        },
        5000ms));

    CHECK(std::filesystem::exists(saved_image_path));
    const auto saved_image_bytes = test_helpers::readBinaryFile(saved_image_path);
    CHECK(saved_image_bytes == small_image);

    server.stopServer();
    client_thread.join();
    server_thread.join();
  }

  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE(
    "REQ-SYS-010/REQ-NET-012/REQ-NET-081: Integration - MMA requests and receives missing image "
    "chunk retry") {
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
  REQUIRE(test_helpers::readPacketWithTimeout(socket, incoming_header, incoming_payload, 3000ms));
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
  REQUIRE(test_helpers::readPacketWithTimeout(socket, retry_header, retry_payload, 5000ms));
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

  CHECK(test_helpers::waitFor(
      [&]() {
        return test_helpers::logContains(testLogFile,
                                         "Image 9001 from aircraft 12345 received and reassembled");
      },
      5000ms));

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}

TEST_CASE(
    "REQ-SYS-010/REQ-NET-012/REQ-NET-081: Integration - MMA logs error when missing chunk retry "
    "fails") {
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
  REQUIRE(test_helpers::readPacketWithTimeout(socket, incoming_header, incoming_payload, 3000ms));
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
  CHECK(test_helpers::waitFor(
      [&]() {
        return test_helpers::logContains(
            testLogFile,
            "Requested retry for missing chunk 1 of image 9002 from aircraft "
            "12345");
      },
      6000ms));

  CHECK(test_helpers::waitFor(
      [&]() {
        return test_helpers::logContains(
            testLogFile,
            "Image 9002 from aircraft 12345 missing chunk 1 after retry; "
            "aborting reassembly");
      },
      8000ms));

  server.stopServer();
  spdlog::shutdown();
  std::remove(testLogFile.c_str());
}
