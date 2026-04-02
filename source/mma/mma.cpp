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
  }
}

void MMA::runMenu() {
  std::cout << "\n========== MMA Technician Console ==========\n";
  std::cout << "1. View warranty status\n";
  std::cout << "2. List connected aircraft\n";
  std::cout << "3. Shutdown server\n";
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
        std::cout << "  - " << conn->getRemoteAddress() << " [state: "
                  << (conn->getState() == network::ConnectionState::VERIFIED ? "VERIFIED"
                                                                             : "UNVERIFIED")
                  << "]\n";
      }
    } else if (line == "3") {
      std::cout << "Shutting down...\n";
      menuRunning_ = false;
      break;
    } else {
      std::cout << "Invalid choice. Please enter 1, 2, or 3.\n";
    }
    // Show menu again
    std::cout << "\n1. View warranty status\n2. List connected aircraft\n3. Shutdown\nChoice: ";
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