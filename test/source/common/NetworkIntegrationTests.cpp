// Needs refactoring
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <aircraft/Aircraft.h>
#include <common/Packet.h>
#include <doctest/doctest.h>
#include <mma/mma.h>

#include <asio.hpp>
#include <chrono>
#include <cstring>
#include <thread>
using namespace std::chrono_literals;

using namespace network;

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