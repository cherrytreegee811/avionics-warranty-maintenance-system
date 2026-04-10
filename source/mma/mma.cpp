#include <common/Packet.h>
#include <mma/WarrantyManager.h>
#include <mma/mma.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <type_traits>

// Helper function to map ImageFormat to file extension
static std::string formatToExtension(network::ImageFormat format) {
  switch (format) {
    case network::ImageFormat::PNG:
      return ".png";
    case network::ImageFormat::JPEG:
      return ".jpg";
    case network::ImageFormat::RAW:
      return ".raw";
    default:
      return ".bin";
  }
}

// Helper function to ensure directory exists and save image to disk
static bool saveReceivedImage(uint64_t aircraft_id, uint32_t image_id,
                              const std::vector<uint8_t>& image_data, network::ImageFormat format) {
  const std::string recv_dir = "res/recv";

  // Create directory if it doesn't exist
  try {
    std::filesystem::create_directories(recv_dir);
  } catch (const std::exception& e) {
    spdlog::error("Failed to create recv directory: {}", e.what());
    return false;
  }

  // Generate unique filename: aircraft_<id>_image_<id>.<ext>
  std::ostringstream filename_stream;
  filename_stream << recv_dir << "/aircraft_" << aircraft_id << "_image_" << image_id
                  << formatToExtension(format);

  const std::string filepath = filename_stream.str();

  // Write image to file
  try {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      spdlog::error("Failed to open image file for writing: {}", filepath);
      return false;
    }

    file.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());

    if (!file) {
      spdlog::error("Error writing image data to file: {}", filepath);
      return false;
    }

    file.close();
    spdlog::info("Successfully saved image to: {}", filepath);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Exception while saving image {}: {}", filepath, e.what());
    return false;
  }
}

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
  running_ = true;
  doAccept();
  io_thread_ = std::thread([this]() { io_context_->run(); });
}

void MMA::stopServer() {
  if (!running_) return;
  menuRunning_ = false;
  running_ = false;

  if (acceptor_) {
    std::error_code ignored;
    acceptor_->close(ignored);
  }

  for (auto& conn : connections_) {
    if (conn) {
      conn->close();
    }
  }

  verified_connections_.clear();
  connection_to_id_.clear();
  connections_.clear();

  io_context_->stop();
  if (io_thread_.joinable()) io_thread_.join();
  spdlog::info("MMA server stopped");
}

void MMA::doAccept() {
  if (!running_ || !acceptor_) {
    return;
  }

  acceptor_->async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
    if (!ec) {
      auto conn = network::TcpConnection::create(std::move(socket));
      handleNewConnection(conn);
    } else {
      spdlog::error("Accept error: {}", ec.message());
    }
    if (running_) {
      doAccept();
    }
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
    uint64_t aircraft_id = 0;
    if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
      aircraft_id = it->second;
    }

    spdlog::info("Aircraft {} landed", aircraft_id);
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
    return;
  }

  if (header.type == network::PacketType::STATE_CHANGE_CONFIRMATION) {
    if (payload.size() != sizeof(network::StateChangeConfirmation)) {
      spdlog::warn("Invalid STATE_CHANGE_CONFIRMATION payload size: {}", payload.size());
      return;
    }

    network::StateChangeConfirmation confirmation{};
    std::memcpy(&confirmation, payload.data(), sizeof(confirmation));

    uint64_t aircraft_id = 0;
    if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
      aircraft_id = it->second;
    }

    spdlog::info("State change confirmation received from aircraft {}: state change to {}",
                 aircraft_id, network::stateIdToString(confirmation.applied_state));

    switch (confirmation.applied_state) {
      case network::StateId::DIAGNOSTIC:
        spdlog::info("Aircraft {} transitioned to DIAGNOSTIC state", aircraft_id);
        break;
      case network::StateId::MAINTENANCE:
        spdlog::info("Aircraft {} transitioned to MAINTENANCE state", aircraft_id);
        break;
      case network::StateId::FAULT:
        spdlog::warn("Aircraft {} transitioned to FAULT state", aircraft_id);
        break;
      default:
        break;
    }
    return;
  }

  if (header.type == network::PacketType::SCHEMATIC_CHUNK) {
    // Handle image chunk reception
    network::ImageChunkHeader chunk_header;
    std::vector<uint8_t> chunk_data;
    if (!network::deserializeImageChunk(payload, chunk_header, chunk_data)) {
      spdlog::warn("Invalid image chunk format");
      return;
    }

    uint64_t aircraft_id = 0;
    if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
      aircraft_id = it->second;
    }

    spdlog::info("Received image chunk {}/{} from aircraft {} (image_id: {}, data: {} bytes)",
                 chunk_header.chunk_index + 1, chunk_header.total_chunks, aircraft_id,
                 chunk_header.image_id, chunk_data.size());

    // Get or create reassembly buffer for this aircraft/image combination
    auto& aircraft_buffers = image_reassembly_buffers_[aircraft_id];
    auto it = aircraft_buffers.find(chunk_header.image_id);
    if (it == aircraft_buffers.end()) {
      // Create new reassembly buffer
      aircraft_buffers.emplace(chunk_header.image_id,
                               network::ImageBuffer(chunk_header.image_id, chunk_header.format,
                                                    chunk_header.total_chunks));
      it = aircraft_buffers.find(chunk_header.image_id);
    }

    // Add chunk to buffer
    if (it->second.addChunk(chunk_header.chunk_index, chunk_data)) {
      // Image is now complete
      const auto complete_image = it->second.reassemble();
      spdlog::info(
          "Image {} from aircraft {} received and reassembled ({} bytes total, format: {})",
          chunk_header.image_id, aircraft_id, complete_image.size(),
          network::imageFormatToString(chunk_header.format));

      // Save image to disk with proper naming and error handling
      if (saveReceivedImage(aircraft_id, chunk_header.image_id, complete_image,
                            chunk_header.format)) {
        spdlog::info("Image successfully archived for aircraft {}", aircraft_id);
      } else {
        spdlog::warn("Failed to save image {} from aircraft {}", chunk_header.image_id,
                     aircraft_id);
      }

      // Remove from reassembly buffer
      aircraft_buffers.erase(it);
    }
    return;
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

        std::cout << "  - " << conn->getRemoteAddress() << " [state: " << state_label << "]\n";
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
    spdlog::warn("Cannot send DIAGNOSTIC state change command: aircraft {} is not verified",
                 aircraftId);
    return;
  }

  network::StateChangeRequest req{network::StateId::DIAGNOSTIC};
  const auto packet = network::serializePacket(network::PacketType::STATE_CHANGE, req);
  it->second->send(packet);
  spdlog::info("Sent DIAGNOSTIC state change command to aircraft {}", aircraftId);
}

void MMA::printDiagnosticFaults(uint64_t aircraftId,
                                const std::vector<network::DiagnosticFaultCode>& faults) const {
  if (faults.empty()) {
    spdlog::info("Received diagnostic data from aircraft {} with no active faults", aircraftId);
    return;
  }

  for (const auto& fault : faults) {
    spdlog::info("Fault Code '{}' (aircraft: {}): [{}] - '{}'", fault.code, aircraftId,
                 network::diagnosticFaultSeverityToString(fault.severity), fault.description);
  }
}

void MMA::displayWarranty(uint64_t aircraftId) {
  auto opt = warrantyManager_->getWarranty(aircraftId);
  if (!opt) {
    spdlog::warn("No warranty record found for aircraft {}", aircraftId);
    return;
  }
  const auto& info = *opt;
  spdlog::info("Displayed warranty information for aircraft {}", aircraftId);
  spdlog::info("Warranty for aircraft {} is {}", aircraftId, info.isActive ? "ACTIVE" : "EXPIRED");
  if (info.isActive) {
    spdlog::info("Warranty for aircraft {} expires on {}", aircraftId, info.expiryDate);
  }
  spdlog::info("Warranty for aircraft {} is provided by {}", aircraftId, info.provider);
}