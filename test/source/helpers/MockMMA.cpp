#include <common/Packet.h>
#include <helpers/MockMMA.h>

#include <cstring>
#include <mutex>

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
      } else if (header.type == PacketType::STATE_CHANGE_CONFIRMATION
                 && payload.size() == sizeof(StateChangeConfirmation)) {
        StateChangeConfirmation confirmation{};
        std::memcpy(&confirmation, payload.data(), sizeof(confirmation));
        confirmation_count_.fetch_add(1);
        std::lock_guard<std::mutex> lock(confirmations_mutex_);
        applied_confirmations_.push_back(confirmation.applied_state);
      } else if (header.type == PacketType::DIAGNOSTIC_DATA) {
        std::vector<DiagnosticFaultCode> faults;
        if (deserializeDiagnosticDataPayload(payload, faults)) {
          diagnostic_fault_count_.store(faults.size());
          received_diagnostic_data_ = true;
        }
      } else if (header.type == PacketType::WARRANTY_DATA) {
        common::WarrantyInfo warranty{};
        if (deserializeWarrantyDataPayload(payload, warranty)) {
          received_warranty_data_ = true;
        }
      }
    }
  }

  bool MockMMA::hasReceivedLanded() const { return received_landed_.load(); }

  bool MockMMA::isVerified() const { return verified_.load(); }

  bool MockMMA::sendStateChange(network::StateId targetState) {
    if (!verified_.load() || !connection_) {
      return false;
    }

    const StateChangeRequest request{targetState};
    const auto packet = serializePacket(PacketType::STATE_CHANGE, request);
    connection_->send(packet);
    return true;
  }

  bool MockMMA::hasConfirmationForState(network::StateId state) const {
    std::lock_guard<std::mutex> lock(confirmations_mutex_);
    for (const auto applied_state : applied_confirmations_) {
      if (applied_state == state) {
        return true;
      }
    }

    return false;
  }

  size_t MockMMA::stateChangeConfirmationCount() const { return confirmation_count_.load(); }

  bool MockMMA::hasReceivedDiagnosticData() const { return received_diagnostic_data_.load(); }

  size_t MockMMA::receivedDiagnosticFaultCount() const {
    return diagnostic_fault_count_.load();
  }

  bool MockMMA::hasReceivedWarrantyData() const { return received_warranty_data_.load(); }

  void MockMMA::closeClientConnection() {
    if (connection_) {
      connection_->close();
    }
  }

}  // namespace test_helpers