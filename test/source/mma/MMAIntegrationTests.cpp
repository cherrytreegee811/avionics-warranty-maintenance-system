#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <common/Packet.h>
#include <doctest/doctest.h>
#include <helpers/MockAircraft.h>
#include <helpers/TestHelpers.h>
#include <mma/mma.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ============================================================================
// REQ-SRV-053: The MMA logs when it receives a landed notification from an aircraft.
// ============================================================================

TEST_CASE("REQ-SRV-053: MMA server logs landed notification from client") {
  const std::string mmaLogFile = "test_mma_server_landed.log";
  std::remove(mmaLogFile.c_str());

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(mmaLogFile, true);
  auto logger = std::make_shared<spdlog::logger>("mma_logger_test", file_sink);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);
  spdlog::flush_on(spdlog::level::info);

  mma::MMA server;
  const uint16_t testPort = 8040;
  server.startServer(testPort);
  std::this_thread::sleep_for(100ms);

  {
    test_helpers::MockAircraft client("127.0.0.1", testPort, 12345);
    client.runVerificationAndSendLanded();
    std::this_thread::sleep_for(200ms);
  }

  server.stopServer();

  bool found = test_helpers::waitFor(
      [&]() { return test_helpers::logContains(mmaLogFile, "Aircraft 12345 landed"); }, 2000);

  CHECK(found);

  spdlog::shutdown();
  std::remove(mmaLogFile.c_str());
}

// ============================================================================
// REQ-SYS-080: Both the client and server shall require connection verification before accepting
//              any commands or returning results.
// ============================================================================

TEST_CASE("REQ-SYS-080: MMA closes connection on invalid verification response size") {
  mma::MMA server;
  server.startServer(0);
  test_helpers::ScopedMmaStopper stop(server);

  const auto port = server.getListeningPort();
  REQUIRE(port != 0);

  asio::io_context io;
  asio::ip::tcp::socket socket(io);

  std::error_code ec;
  socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
  REQUIRE(!ec);

  network::PacketHeader request_header{};
  std::vector<uint8_t> request_payload;
  REQUIRE(test_helpers::readPacketWithTimeout(socket, request_header, request_payload, 2000ms));
  CHECK(request_header.type == network::PacketType::VERIFICATION_REQUEST);

  const auto invalid_response
      = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE);
  asio::write(socket, asio::buffer(invalid_response), ec);
  REQUIRE(!ec);

  socket.non_blocking(true, ec);
  REQUIRE(!ec);

  bool closed = false;
  for (int i = 0; i < 40; ++i) {
    std::array<uint8_t, 1> buf{};
    socket.read_some(asio::buffer(buf), ec);
    if (ec == asio::error::eof) {
      closed = true;
      break;
    }
    if (ec == asio::error::would_block || ec == asio::error::try_again) {
      ec.clear();
      std::this_thread::sleep_for(50ms);
      continue;
    }
    if (ec) {
      break;
    }
    std::this_thread::sleep_for(50ms);
  }

  CHECK(closed);
}

TEST_CASE("REQ-SYS-080: MMA closes connection on incorrect verification challenge response") {
  mma::MMA server;
  server.startServer(0);
  test_helpers::ScopedMmaStopper stop(server);

  const auto port = server.getListeningPort();
  REQUIRE(port != 0);

  asio::io_context io;
  asio::ip::tcp::socket socket(io);

  std::error_code ec;
  socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), ec);
  REQUIRE(!ec);

  network::PacketHeader request_header{};
  std::vector<uint8_t> request_payload;
  REQUIRE(test_helpers::readPacketWithTimeout(socket, request_header, request_payload, 2000ms));
  REQUIRE(request_header.type == network::PacketType::VERIFICATION_REQUEST);
  REQUIRE(request_payload.size() == sizeof(network::VerificationRequest));

  network::VerificationRequest req{};
  (void)std::memcpy(&req, request_payload.data(), sizeof(req));

  network::VerificationResponse bad_resp{};
  bad_resp.client_id = 424242;
  bad_resp.challenge_response = req.challenge ^ 0xBAD0C0DE;  // intentionally incorrect
  const auto response_packet
      = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE, bad_resp);
  asio::write(socket, asio::buffer(response_packet), ec);
  REQUIRE(!ec);

  socket.non_blocking(true, ec);
  REQUIRE(!ec);

  bool closed = false;
  for (int i = 0; i < 40; ++i) {
    std::array<uint8_t, 1> buf{};
    socket.read_some(asio::buffer(buf), ec);
    if (ec == asio::error::eof) {
      closed = true;
      break;
    }
    if (ec == asio::error::would_block || ec == asio::error::try_again) {
      ec.clear();
      std::this_thread::sleep_for(50ms);
      continue;
    }
    if (ec) {
      break;
    }
    std::this_thread::sleep_for(50ms);
  }

  CHECK(closed);
}
