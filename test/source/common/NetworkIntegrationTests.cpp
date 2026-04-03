#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <common/Packet.h>
#include <common/TcpConnection.h>
#include <doctest/doctest.h>

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

using namespace network;

namespace {

  using namespace std::chrono_literals;

  template <typename T>
  bool waitUntilReady(std::future<T>& future, std::chrono::milliseconds timeout = 2000ms) {
    return future.wait_for(timeout) == std::future_status::ready;
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
    server_connection->setMessageHandler([&, server_connection](const std::vector<uint8_t>& packet) {
      PacketHeader header{};
      std::vector<uint8_t> payload;
      if (!deserializePacket(packet, header, payload)) {
        return;
      }

      if (header.type == PacketType::LANDED_NOTIFICATION && !server_packet_recorded.exchange(true)) {
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
