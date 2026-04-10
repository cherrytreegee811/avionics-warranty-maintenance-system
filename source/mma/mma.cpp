#include <common/Packet.h>
#include <mma/WarrantyManager.h>
#include <mma/mma.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
  io_context_->restart();
  asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);

  acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(*io_context_);
  acceptor_->open(endpoint.protocol());
  acceptor_->set_option(asio::socket_base::reuse_address(true));
  acceptor_->bind(endpoint);
  acceptor_->listen();

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

  std::vector<network::TcpConnection::Ptr> connections_snapshot;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    connections_snapshot = connections_;
  }
  for (auto& conn : connections_snapshot) {
    if (conn) {
      conn->close();
    }
  }

  io_context_->stop();
  if (io_thread_.joinable()) io_thread_.join();

  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    verified_connections_.clear();
    connection_to_id_.clear();
    landed_aircraft_.clear();
    diagnostic_confirmed_aircraft_.clear();
    session_warranty_available_.clear();
    aircraft_states_.clear();
    image_reassembly_buffers_.clear();
    connections_.clear();
  }
  spdlog::info("MMA server stopped");
}

uint16_t MMA::getListeningPort() const {
  if (!acceptor_) {
    return 0;
  }

  std::error_code ec;
  const auto endpoint = acceptor_->local_endpoint(ec);
  if (ec) {
    return 0;
  }

  return endpoint.port();
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
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    connections_.push_back(conn);
  }

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
          {
            std::lock_guard<std::mutex> lock(state_mutex_);
            verified_connections_[resp.client_id] = conn;
            connection_to_id_[conn.get()] = resp.client_id;
            landed_aircraft_[resp.client_id] = false;
            diagnostic_confirmed_aircraft_[resp.client_id] = false;
            session_warranty_available_[resp.client_id] = false;
          }
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
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
        aircraft_id = it->second;
      }

      if (aircraft_id != 0) {
        landed_aircraft_[aircraft_id] = true;
      }
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
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
        aircraft_id = it->second;
      }
    }
    printDiagnosticFaults(aircraft_id, faults);
    return;
  }

  if (header.type == network::PacketType::WARRANTY_DATA) {
    common::WarrantyInfo warranty;
    if (!network::deserializeWarrantyDataPayload(payload, warranty)) {
      spdlog::warn("Received malformed WARRANTY_DATA payload");
      return;
    }

    uint64_t aircraft_id = 0;
    bool landed = false;
    bool diagnostic_confirmed = false;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
        aircraft_id = it->second;
      }

      if (aircraft_id != 0) {
        landed = landed_aircraft_.contains(aircraft_id) && landed_aircraft_[aircraft_id];
        diagnostic_confirmed = diagnostic_confirmed_aircraft_.contains(aircraft_id)
                               && diagnostic_confirmed_aircraft_[aircraft_id];
      }
    }

    if (aircraft_id == 0) {
      spdlog::warn("Received WARRANTY_DATA from unknown aircraft connection");
      return;
    }

    if (!landed || !diagnostic_confirmed) {
      spdlog::warn(
          "Ignoring WARRANTY_DATA for aircraft {} because sequence prerequisites are unmet "
          "(landed: {}, diagnostic_confirmed: {})",
          aircraft_id, landed, diagnostic_confirmed);
      return;
    }

    warrantyManager_->setWarranty(aircraft_id, warranty);
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      session_warranty_available_[aircraft_id] = true;
    }
    spdlog::info("Updated warranty data for aircraft {}", aircraft_id);
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
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
        aircraft_id = it->second;
      }

      aircraft_states_[aircraft_id] = confirmation.applied_state;
    }

    spdlog::info("State change confirmation received from aircraft {}: state change to {}",
                 aircraft_id, network::stateIdToString(confirmation.applied_state));
    spdlog::default_logger()->flush();

    switch (confirmation.applied_state) {
      case network::StateId::DIAGNOSTIC:
        if (aircraft_id != 0) {
          std::lock_guard<std::mutex> lock(state_mutex_);
          diagnostic_confirmed_aircraft_[aircraft_id] = true;
          session_warranty_available_[aircraft_id] = false;
        }
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
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
        aircraft_id = it->second;
      }
    }

    spdlog::info("Received image chunk {}/{} from aircraft {} (image_id: {}, data: {} bytes)",
                 chunk_header.chunk_index + 1, chunk_header.total_chunks, aircraft_id,
                 chunk_header.image_id, chunk_data.size());

    bool image_complete = false;
    std::vector<uint8_t> complete_image;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      // Get or create reassembly buffer for this aircraft/image combination
      auto& aircraft_buffers = image_reassembly_buffers_[aircraft_id];
      auto buffer_it = aircraft_buffers.find(chunk_header.image_id);
      if (buffer_it == aircraft_buffers.end()) {
        // Create new reassembly buffer
        aircraft_buffers.emplace(chunk_header.image_id,
                                 network::ImageBuffer(chunk_header.image_id, chunk_header.format,
                                                      chunk_header.total_chunks));
        buffer_it = aircraft_buffers.find(chunk_header.image_id);
      }

      // Add chunk to buffer
      if (buffer_it->second.addChunk(chunk_header.chunk_index, chunk_data)) {
        image_complete = true;
        complete_image = buffer_it->second.reassemble();
        // Remove from reassembly buffer
        aircraft_buffers.erase(buffer_it);
      }
    }

    if (image_complete) {
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

    }

    return;
  }

  if (header.type == network::PacketType::CLEAR_DIAGNOSTIC_CODE_CONFIRMATION) {
    if (payload.size() != sizeof(network::DiagnosticCodeClearConfirmation)) {
      spdlog::warn("Invalid CLEAR_DIAGNOSTIC_CODE_CONFIRMATION payload size: {}", payload.size());
      return;
    }

    network::DiagnosticCodeClearConfirmation confirmation{};
    std::memcpy(&confirmation, payload.data(), sizeof(confirmation));

    uint64_t aircraft_id = 0;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
        aircraft_id = it->second;
      }

      aircraft_states_[aircraft_id] = confirmation.resulting_state;
    }

    if (confirmation.status == network::DiagnosticCodeClearStatus::SUCCESS) {
      spdlog::info("Diagnostic code clear succeeded for aircraft {} (code: {}, state: {})",
                   aircraft_id, confirmation.code,
                   network::stateIdToString(confirmation.resulting_state));
    } else {
      spdlog::warn("Diagnostic code clear failed for aircraft {} (code: {}, status: {}, state: {})",
                   aircraft_id, confirmation.code,
                   network::diagnosticCodeClearStatusToString(confirmation.status),
                   network::stateIdToString(confirmation.resulting_state));
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
  std::cout << "5. Clear one diagnostic code (MAINTENANCE or FAULT)\n";
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
      std::vector<network::TcpConnection::Ptr> connections_snapshot;
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        connections_snapshot = connections_;
      }

      std::cout << "Connected aircraft (" << connections_snapshot.size() << "):\n";
      for (auto& conn : connections_snapshot) {
        const char* state_label = "UNVERIFIED";
        if (conn->getState() == network::ConnectionState::VERIFIED) {
          state_label = "VERIFIED";
        } else if (conn->getState() == network::ConnectionState::CLOSED) {
          state_label = "CLOSED";
        }

        std::cout << fmt::format("  - {} [state: {}]\n", conn->getRemoteAddress(), state_label);
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
    } else if (line == "5") {
      std::cout << "Enter aircraft ID: ";
      std::string idStr;
      std::getline(std::cin, idStr);
      std::cout << "Enter diagnostic code to clear: ";
      std::string codeStr;
      std::getline(std::cin, codeStr);
      try {
        const uint64_t id = std::stoull(idStr);
        const int32_t code = std::stoi(codeStr);
        sendDiagnosticCodeClearRequest(id, code);
      } catch (...) {
        std::cout << "Invalid input.\n";
      }
    } else {
      std::cout << "Invalid choice. Please enter 1, 2, 3, 4, or 5.\n";
    }
    // Show menu again
    std::cout
        << "\n1. View warranty status\n2. List connected aircraft\n3. Shutdown\n4. Set "
           "aircraft to DIAGNOSTIC\n5. Clear one diagnostic code (MAINTENANCE or FAULT)\nChoice: ";
  }
}

void MMA::sendDiagnosticStateChange(uint64_t aircraftId) {
  network::TcpConnection::Ptr conn;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    const auto it = verified_connections_.find(aircraftId);
    if (it != verified_connections_.end()) {
      conn = it->second;
    }
  }

  if (!conn) {
    spdlog::warn("Cannot send DIAGNOSTIC state change command: aircraft {} is not verified",
                 aircraftId);
    return;
  }

  network::StateChangeRequest req{network::StateId::DIAGNOSTIC};
  const auto packet = network::serializePacket(network::PacketType::STATE_CHANGE, req);
  conn->send(packet);
  spdlog::info("Sent DIAGNOSTIC state change command to aircraft {}", aircraftId);
  spdlog::default_logger()->flush();
}

void MMA::sendDiagnosticCodeClearRequest(uint64_t aircraftId, int32_t code) {
  network::TcpConnection::Ptr connection;
  std::optional<network::StateId> state;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    const auto connection_it = verified_connections_.find(aircraftId);
    if (connection_it != verified_connections_.end()) {
      connection = connection_it->second;
    }

    const auto state_it = aircraft_states_.find(aircraftId);
    if (state_it != aircraft_states_.end()) {
      state = state_it->second;
    }
  }

  if (!connection) {
    spdlog::warn("Cannot send diagnostic code clear command: aircraft {} is not verified",
                 aircraftId);
    return;
  }

  if (!state.has_value()) {
    spdlog::warn(
        "Cannot send diagnostic code clear command to aircraft {}: state is unknown. "
        "Wait for state confirmation from the aircraft.",
        aircraftId);
    return;
  }

  if (*state != network::StateId::MAINTENANCE && *state != network::StateId::FAULT) {
    spdlog::warn(
        "Cannot send diagnostic code clear command to aircraft {}: aircraft is in {} (requires "
        "MAINTENANCE or FAULT)",
        aircraftId, network::stateIdToString(*state));
    return;
  }

  network::DiagnosticCodeClearRequest request{code};
  const auto packet = network::serializePacket(network::PacketType::CLEAR_DIAGNOSTIC_CODE, request);
  connection->send(packet);
  spdlog::info("Sent diagnostic code clear command to aircraft {} for code {}", aircraftId, code);
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
  const bool session_warranty_available = [&]() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return session_warranty_available_.contains(aircraftId)
           && session_warranty_available_[aircraftId];
  }();
  if (!session_warranty_available) {
    spdlog::warn(
        "Warranty data for aircraft {} is not yet available in this session. Set aircraft to "
        "DIAGNOSTIC and wait for transfer.",
        aircraftId);
    return;
  }

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