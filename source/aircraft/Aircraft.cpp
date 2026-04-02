#include <aircraft/Aircraft.h>
#include <common/Packet.h>
#include <common/WarrantyData.h>
#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace aircraft;

Aircraft::Aircraft() : m_currentState("STANDBY") {
  // Initialize with sample data for demonstration
  // In production, this would come from server/ persistence

  // Sample maintenance data
  m_lastMaintenance.lastMaintenance = std::chrono::system_clock::now();
  m_lastMaintenance.technician = "John Doe";
  m_lastMaintenance.notes = "Routine checkup";

  // Sample fault codes
  m_faultCodes.push_back(
      {101, "Engine temperature sensor fault", std::chrono::system_clock::now()});
  m_faultCodes.push_back({202, "Hydraulic pressure low", std::chrono::system_clock::now()});

  // Sample warranty data
  m_warranty.isActive = true;
  m_warranty.expiryDate = "2027-12-31";
  m_warranty.provider = "Aviation Warranty Corp";
}

void Aircraft::initialize() {
  // TODO: Load persisted data
}

std::string Aircraft::getCurrentState() const { return m_currentState; }

void Aircraft::setCurrentState(const std::string& state) { m_currentState = state; }

MaintenanceInfo Aircraft::getLastMaintenance() const { return m_lastMaintenance; }

std::vector<FaultCode> Aircraft::getFaultCodes() const { return m_faultCodes; }

WarrantyInfo Aircraft::getWarranty() const { return m_warranty; }

void Aircraft::setLastMaintenance(const MaintenanceInfo& info) { m_lastMaintenance = info; }

void Aircraft::addFaultCode(const FaultCode& code) { m_faultCodes.push_back(code); }

void Aircraft::clearFaultCodes() { m_faultCodes.clear(); }

void Aircraft::setWarranty(const WarrantyInfo& info) { m_warranty = info; }

void Aircraft::connectToMMA(const std::string& host, uint16_t port) {
  auto io = std::make_shared<asio::io_context>();
  auto socket = std::make_shared<asio::ip::tcp::socket>(*io);
  asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host), port);

  socket->async_connect(endpoint, [this, socket, io](std::error_code ec) {
    if (ec) {
      spdlog::error("Connect failed: {}", ec.message());
      return;
    }
    spdlog::info("Connected to MMA server");

    // Create TcpConnection with the connected socket
    connection_ = network::TcpConnection::create(std::move(*socket));
    connection_->setMessageHandler(
        [this](const std::vector<uint8_t>& data) { onNetworkMessage(data); });
    connection_->start();
    io->run();
  });

  std::thread([io]() { io->run(); }).detach();
}

void Aircraft::onNetworkMessage(const std::vector<uint8_t>& data) {
  network::PacketHeader header;
  std::vector<uint8_t> payload;
  if (!network::deserializePacket(data, header, payload)) return;

  if (!verified_) {
    if (header.type == network::PacketType::VERIFICATION_REQUEST) {
      network::VerificationRequest req;
      std::memcpy(&req, payload.data(), sizeof(req));
      network::VerificationResponse resp;
      resp.challenge_response = req.challenge ^ 0xDEADBEEF;
      resp.client_id = aircraft_id_;
      auto resp_packet = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE, resp);
      connection_->send(resp_packet);
      verified_ = true;
      connection_->setState(network::ConnectionState::VERIFIED);
      spdlog::info("Verification successful, client ID {}", aircraft_id_);
    } else {
      spdlog::warn("Received non‑verification packet before handshake, closing");
      connection_->close();
    }
  } else {
    // Handle verified commands (e.g., state changes from server)
    if (header.type == network::PacketType::STATE_CHANGE) {
      // parse new state and call setCurrentState()
    }
  }
}
