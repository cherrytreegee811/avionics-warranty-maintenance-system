#include <common/Packet.h>
#include <helpers/MockAircraft.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <thread>

using namespace network;

namespace test_helpers {

  MockAircraft::MockAircraft(const std::string& host, uint16_t port, uint64_t client_id)
      : client_id_(client_id), socket_(io_context_) {
    socket_.connect(asio::ip::tcp::endpoint(asio::ip::make_address(host), port));
    spdlog::info("MockAircraft connected to {}:{}", host, port);
  }

  MockAircraft::~MockAircraft() {
    std::error_code ec;
    socket_.close(ec);
  }

  void MockAircraft::runVerificationAndSendLanded() {
    // Receive VERIFICATION_REQUEST
    std::vector<uint8_t> buffer(1024);
    asio::error_code ec;
    size_t len = socket_.read_some(asio::buffer(buffer), ec);
    if (ec) {
      spdlog::error("MockAircraft read failed: {}", ec.message());
      throw std::runtime_error("Read failed");
    }

    PacketHeader hdr;
    std::vector<uint8_t> payload;
    if (!deserializePacket(std::vector<uint8_t>(buffer.begin(), buffer.begin() + len), hdr,
                           payload)) {
      throw std::runtime_error("Invalid packet");
    }

    if (hdr.type != PacketType::VERIFICATION_REQUEST
        || payload.size() != sizeof(VerificationRequest)) {
      throw std::runtime_error("Unexpected verification request");
    }

    VerificationRequest req;
  (void)std::memcpy(&req, payload.data(), sizeof(req));

    // Compute response
    VerificationResponse resp;
    resp.challenge_response = req.challenge ^ 0xDEADBEEF;
    resp.client_id = client_id_;
    auto response_packet = serializePacket(PacketType::VERIFICATION_RESPONSE, resp);
    asio::write(socket_, asio::buffer(response_packet));

    // Send LANDED notification
    auto landed_packet = serializePacket(PacketType::LANDED_NOTIFICATION);
    asio::write(socket_, asio::buffer(landed_packet));
    spdlog::info("MockAircraft sent LANDED notification for client {}", client_id_);
  }

}  // namespace test_helpers