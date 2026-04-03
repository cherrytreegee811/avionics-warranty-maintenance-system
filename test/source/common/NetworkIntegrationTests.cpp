#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <common/Packet.h>
#include <common/TcpConnection.h>
#include <doctest/doctest.h>
#include <mma/mma.h>

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

using namespace network;

namespace {

  using namespace std::chrono_literals;

  template <typename T>
  bool waitUntilReady(std::future<T>& future, std::chrono::milliseconds timeout = 2000ms) {
    return future.wait_for(timeout) == std::future_status::ready;
  }

}  // namespace

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

TEST_CASE("Successful verification flow.") {
  // We first need to initialize a server from which we are listening.
  // A connection must be simulated for testing flows.
  MMA server;
  // client must be referenced directly from namespace.
  aircraft::Aircraft client;

  auto port = 8000;  // server port. Maps to uint16_t
  server.startServer(port);

  // Now we simulate a connection from the client.
  std::string host = "127.0.0.1";
  client.connectToMMA(host, port);

  // Time is allocated for TCP handshake.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // CHECK(server.getRunningStatus() == true);

  server.stopServer();
}

TEST_CASE("Command rejection when connection is UNVERIFIED.") {
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

  // 2. SEND THE ILLEGAL COMMAND
  auto packet = network::serializePacket(network::PacketType::LANDED_NOTIFICATION, nullptr, 0);
  asio::write(socket, asio::buffer(packet));

  // 3. WAIT AND CHECK FOR CLOSURE
  // Instead of one read, we wait specifically for the EOF.
  bool closed = false;
  for (int i = 0; i < 5; ++i) {
    char buf[1];
    socket.read_some(asio::buffer(buf), ec);
    if (ec == asio::error::eof) {
      closed = true;
      break;
    }
    std::this_thread::sleep_for(100ms);
  }

  CHECK(closed == true);
  CHECK(ec == asio::error::eof);

  server.stopServer();
}

/* TEST_CASE("Verification timeout handling.") {} */

/* TEST_CASE("Invalid verification response handling.") */
