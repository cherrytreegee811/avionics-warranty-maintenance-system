#include <common/Packet.h>
#include <mma/WarrantyManager.h>
#include <mma/mma.h>
#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

MMA::MMA()
    : io_context_(std::make_unique<asio::io_context>()),
      warrantyManager_(std::make_unique<WarrantyManager>()) {
  warrantyManager_->load();
}

MMA::~MMA() = default;

void MMA::initialize() {}

void MMA::startServer(uint16_t port) {
  if (running_) return;
  asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
  acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(*io_context_, endpoint);
  spdlog::info("MMA server listening on port {}", port);
  doAccept();
  running_ = true;
  io_thread_ = std::thread([this]() { io_context_->run(); });
}

void MMA::stopServer() {
  if (!running_) return;
  menuRunning_ = false;
  io_context_->stop();
  if (io_thread_.joinable()) io_thread_.join();
  running_ = false;
  spdlog::info("MMA server stopped");
}

void MMA::doAccept() {
  acceptor_->async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
    if (!ec) {
      auto conn = network::TcpConnection::create(std::move(socket));
      handleNewConnection(conn);
    } else {
      spdlog::error("Accept error: {}", ec.message());
    }
    doAccept();
  });
}

void MMA::handleNewConnection(network::TcpConnection::Ptr conn) {
  spdlog::info("New connection from {}", conn->getRemoteAddress());
  connections_.push_back(conn);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist;
  expected_challenge_ = dist(gen);
  network::VerificationRequest req{
      expected_challenge_,
      static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count())};
  auto packet = network::serializePacket(network::PacketType::VERIFICATION_REQUEST, req);
  conn->send(packet);

  conn->setMessageHandler(
      [this, conn](const std::vector<uint8_t>& data) { processMessage(data, conn); });
  conn->start();
}

void MMA::processMessage(const std::vector<uint8_t>& data, network::TcpConnection::Ptr conn) {
  network::PacketHeader header;
  std::vector<uint8_t> payload;
  if (!network::deserializePacket(data, header, payload)) {
    spdlog::error("Malformed packet, closing connection");
    conn->close();
    return;
  }

  if (conn->getState() == network::ConnectionState::UNVERIFIED) {
    if (header.type == network::PacketType::VERIFICATION_RESPONSE) {
      network::VerificationResponse resp;
      if (payload.size() == sizeof(resp)) {
        std::memcpy(&resp, payload.data(), sizeof(resp));
        if (resp.challenge_response == (expected_challenge_ ^ 0xDEADBEEF)) {
          conn->setState(network::ConnectionState::VERIFIED);
          spdlog::info("Client {} verified", resp.client_id);
          verified_connections_[resp.client_id] = conn;
          connection_to_id_[conn.get()] = resp.client_id;
        } else {
          spdlog::error("Verification failed, closing connection");
          conn->close();
        }
      } else {
        spdlog::error("Invalid verification response size");
        conn->close();
      }
    } else {
      spdlog::warn("Rejecting command type {} from unverified client",
                   static_cast<int>(header.type));
      conn->close();
    }
    return;
  }

  if (header.type == network::PacketType::LANDED_NOTIFICATION) {
    spdlog::info("Client landed notification received");
    // Future: change aircraft state to DIAGNOSTIC
    return;
  }

  if (header.type == network::PacketType::DIAGNOSTIC_DATA) {
    std::vector<network::DiagnosticFaultCode> faults;
    if (!network::deserializeDiagnosticDataPayload(payload, faults)) {
      spdlog::warn("Received malformed DIAGNOSTIC_DATA payload");
      return;
    }

    uint64_t aircraft_id = 0;
    if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
      aircraft_id = it->second;
    }
    printDiagnosticFaults(aircraft_id, faults);
  }
}

void MMA::runMenu() {
  std::cout << "\n========== MMA Technician Console ==========\n";
  std::cout << "1. View warranty status\n";
  std::cout << "2. List connected aircraft\n";
  std::cout << "3. Shutdown server\n";
  std::cout << "4. Set aircraft to DIAGNOSTIC\n";
  std::cout << "============================================\n";
  std::cout << "Enter choice: ";

  std::string line;
  while (menuRunning_ && std::getline(std::cin, line)) {
    if (line == "1") {
      std::cout << "Enter aircraft ID: ";
      std::string idStr;
      std::getline(std::cin, idStr);
      try {
        uint64_t id = std::stoull(idStr);
        displayWarranty(id);
      } catch (...) {
        std::cout << "Invalid ID.\n";
      }
    } else if (line == "2") {
      std::cout << "Connected aircraft (" << connections_.size() << "):\n";
      for (auto& conn : connections_) {
        const char* state_label = "UNVERIFIED";
        if (conn->getState() == network::ConnectionState::VERIFIED) {
          state_label = "VERIFIED";
        } else if (conn->getState() == network::ConnectionState::CLOSED) {
          state_label = "CLOSED";
        }

        std::cout << "  - " << conn->getRemoteAddress() << " [state: "
                  << state_label
                  << "]\n";
      }
    } else if (line == "3") {
      std::cout << "Shutting down...\n";
      menuRunning_ = false;
      break;
    } else if (line == "4") {
      std::cout << "Enter aircraft ID: ";
      std::string idStr;
      std::getline(std::cin, idStr);
      try {
        const uint64_t id = std::stoull(idStr);
        sendDiagnosticStateChange(id);
      } catch (...) {
        std::cout << "Invalid ID.\n";
      }
    } else {
      std::cout << "Invalid choice. Please enter 1, 2, 3, or 4.\n";
    }
    // Show menu again
    std::cout << "\n1. View warranty status\n2. List connected aircraft\n3. Shutdown\n4. Set "
                 "aircraft to DIAGNOSTIC\nChoice: ";
  }
}

void MMA::sendDiagnosticStateChange(uint64_t aircraftId) {
  const auto it = verified_connections_.find(aircraftId);
  if (it == verified_connections_.end()) {
    std::cout << "No verified aircraft found with ID " << aircraftId << "\n";
    return;
  }

  network::StateChangeRequest req{network::StateId::DIAGNOSTIC};
  const auto packet = network::serializePacket(network::PacketType::STATE_CHANGE, req);
  it->second->send(packet);
  std::cout << "Sent DIAGNOSTIC state change command to aircraft " << aircraftId << "\n";
}

void MMA::printDiagnosticFaults(uint64_t aircraftId,
                                const std::vector<network::DiagnosticFaultCode>& faults) const {
  std::cout << "\nDiagnostic report from aircraft " << aircraftId << ":\n";
  if (faults.empty()) {
    std::cout << "  No active fault codes.\n";
    return;
  }

  for (const auto& fault : faults) {
    std::cout << "  - Code " << fault.code << ": " << fault.description
              << " [timestamp_ms=" << fault.timestamp_epoch_ms << "]\n";
  }
}

void MMA::displayWarranty(uint64_t aircraftId) {
  auto opt = warrantyManager_->getWarranty(aircraftId);
  if (!opt) {
    std::cout << "No warranty record found for aircraft " << aircraftId << "\n";
    return;
  }
  const auto& info = *opt;
  std::cout << "Warranty for aircraft " << aircraftId << ":\n";
  std::cout << "  Status: " << (info.isActive ? "ACTIVE" : "EXPIRED") << "\n";
  if (info.isActive) {
    std::cout << "  Expiry Date: " << info.expiryDate << "\n";
  }
  std::cout << "  Provider: " << info.provider << "\n";
}