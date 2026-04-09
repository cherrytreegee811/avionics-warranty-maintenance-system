#include <common/Packet.h>
#include <helpers/MockMMA.h>

#include <cstring>

using namespace network;

namespace test_helpers {

  MockMMA::MockMMA(uint16_t port)
      : acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
    accept();
    io_thread_ = std::thread([this] { io_context_.run(); });
  }

  MockMMA::~MockMMA() {
    io_context_.stop();
    if (io_thread_.joinable()) io_thread_.join();
  }

  void MockMMA::accept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
      if (ec) return;
      connection_ = TcpConnection::create(std::move(socket));
      connection_->setMessageHandler([this](const std::vector<uint8_t>& data) { onMessage(data); });
      connection_->start();
      sendVerificationRequest();
      accept();  // Accept next connection
    });
  }

  void MockMMA::sendVerificationRequest() {
    VerificationRequest req{challenge_, 12345ULL};
    auto packet = serializePacket(PacketType::VERIFICATION_REQUEST, req);
    connection_->send(packet);
  }

  void MockMMA::onMessage(const std::vector<uint8_t>& data) {
    PacketHeader header;
    std::vector<uint8_t> payload;
    if (!deserializePacket(data, header, payload)) return;

    if (!verified_) {
      if (header.type == PacketType::VERIFICATION_RESPONSE
          && payload.size() == sizeof(VerificationResponse)) {
        VerificationResponse resp;
        std::memcpy(&resp, payload.data(), sizeof(resp));
        // Optionally verify challenge_response, but we accept any for test.
        verified_ = true;
        // After verification, the client will send LANDED automatically.
      }
    } else {
      if (header.type == PacketType::LANDED_NOTIFICATION) {
        received_landed_ = true;
      }
    }
  }

  bool MockMMA::hasReceivedLanded() const { return received_landed_.load(); }

}  // namespace test_helpers