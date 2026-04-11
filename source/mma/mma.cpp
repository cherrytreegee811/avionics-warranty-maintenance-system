/**
 * @file mma.cpp
 * @brief Implements the MMA server and command handling flows.
 */

#include <common/Packet.h>
#include <mma/WarrantyManager.h>
#include <mma/mma.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <asio.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <type_traits>

namespace {
  constexpr auto kMissingChunkTimeout = std::chrono::seconds(2);
  constexpr auto kMissingChunkCheckInterval = std::chrono::milliseconds(500);
  constexpr uint8_t kMaxMissingChunkRetryAttempts = 1;
}  // namespace

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

    // Post-write integrity verification: read the file back and compare byte-for-byte.
    std::ifstream verify_file(filepath, std::ios::binary | std::ios::ate);
    if (!verify_file.is_open()) {
      spdlog::error("Failed to open image file for integrity verification: {}", filepath);
      return false;
    }

    const auto saved_size_pos = verify_file.tellg();
    if (saved_size_pos < 0) {
      spdlog::error("Failed to determine saved image size during verification: {}", filepath);
      return false;
    }

    const size_t saved_size = static_cast<size_t>(saved_size_pos);
    if (saved_size != image_data.size()) {
      spdlog::error("Saved image size mismatch for {}: expected {} bytes, got {} bytes", filepath,
                    image_data.size(), saved_size);
      return false;
    }

    verify_file.seekg(0, std::ios::beg);
    std::vector<uint8_t> saved_data(saved_size);
    if (saved_size > 0) {
      verify_file.read(reinterpret_cast<char*>(saved_data.data()),
                       static_cast<std::streamsize>(saved_size));
      if (!verify_file || verify_file.gcount() != static_cast<std::streamsize>(saved_size)) {
        spdlog::error("Failed to read saved image data for verification: {}", filepath);
        return false;
      }
    }

    if (!std::equal(saved_data.begin(), saved_data.end(), image_data.begin())) {
      spdlog::error("Image integrity verification failed (byte mismatch): {}", filepath);
      return false;
    }

    spdlog::info("Successfully saved image to: {}", filepath);
    spdlog::info("Image integrity verified byte-for-byte: {}", filepath);
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
  chunk_timeout_timer_ = std::make_unique<asio::steady_timer>(*io_context_);
  scheduleChunkTimeoutChecks();
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

  if (chunk_timeout_timer_) {
    chunk_timeout_timer_->cancel();
  }

  auto cleanup_done = std::make_shared<std::promise<void>>();
  auto cleanup_future = cleanup_done->get_future();
  asio::post(*io_context_, [this, cleanup_done]() mutable {
    for (auto& conn : connections_) {
      if (conn) {
        conn->close();
      }
    }

    verified_connections_.clear();
    connection_to_id_.clear();
    aircraft_states_.clear();
    connections_.clear();
    image_reassembly_buffers_.clear();
    image_reassembly_retry_state_.clear();

    if (chunk_timeout_timer_) {
      chunk_timeout_timer_.reset();
    }

    cleanup_done->set_value();
  });

  if (cleanup_future.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready) {
    io_context_->restart();
    while (cleanup_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
      if (io_context_->poll_one() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  io_context_->stop();
  if (io_thread_.joinable()) io_thread_.join();
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

  if (header.type == network::PacketType::WARRANTY_DATA) {
    common::WarrantyInfo warranty{};
    if (!network::deserializeWarrantyDataPayload(payload, warranty)) {
      spdlog::warn("Received malformed WARRANTY_DATA payload");
      return;
    }

    uint64_t aircraft_id = 0;
    if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
      aircraft_id = it->second;
    }

    warrantyManager_->setWarranty(aircraft_id, warranty);
    spdlog::info("Persisted warranty data for aircraft {} to mma_warranty_data.csv", aircraft_id);
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

    aircraft_states_[aircraft_id] = confirmation.applied_state;

    spdlog::info("State change confirmation received from aircraft {}: state change to {}",
                 aircraft_id, network::stateIdToString(confirmation.applied_state));
    spdlog::default_logger()->flush();

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
    auto& retry_states = image_reassembly_retry_state_[aircraft_id];
    auto buffer_it = aircraft_buffers.find(chunk_header.image_id);
    if (buffer_it == aircraft_buffers.end()) {
      // Create new reassembly buffer
      aircraft_buffers.emplace(chunk_header.image_id,
                               network::ImageBuffer(chunk_header.image_id, chunk_header.format,
                                                    chunk_header.total_chunks));
      buffer_it = aircraft_buffers.find(chunk_header.image_id);

      ReassemblyRetryState retry_state{};
      retry_state.last_chunk_time = std::chrono::steady_clock::now();
      retry_state.retry_attempts_per_chunk.resize(chunk_header.total_chunks, 0);
      retry_states.emplace(chunk_header.image_id, std::move(retry_state));
    }

    auto retry_it = retry_states.find(chunk_header.image_id);
    if (retry_it != retry_states.end()) {
      retry_it->second.last_chunk_time = std::chrono::steady_clock::now();
    }

    if (!buffer_it->second.setExpectedImageCrc(chunk_header.image_crc32)) {
      spdlog::warn(
          "Rejected image chunk for aircraft {} image {} due to inconsistent full-image CRC "
          "(expected 0x{:08X}, got 0x{:08X})",
          aircraft_id, chunk_header.image_id, buffer_it->second.expected_image_crc32,
          chunk_header.image_crc32);
      return;
    }

    // Add chunk to buffer
    if (buffer_it->second.addChunk(chunk_header.chunk_index, chunk_data)) {
      // Image is now complete
      const auto complete_image = buffer_it->second.reassemble();

      if (!buffer_it->second.validateReassembledCrc(complete_image)) {
        spdlog::error(
            "Reassembled image {} from aircraft {} failed CRC validation (expected 0x{:08X}, "
            "computed 0x{:08X})",
            chunk_header.image_id, aircraft_id, buffer_it->second.expected_image_crc32,
            network::Crc32::calculate(complete_image.data(), complete_image.size()));
        aircraft_buffers.erase(buffer_it);
        retry_states.erase(chunk_header.image_id);
        return;
      }

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
      aircraft_buffers.erase(buffer_it);
      retry_states.erase(chunk_header.image_id);
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
    if (const auto it = connection_to_id_.find(conn.get()); it != connection_to_id_.end()) {
      aircraft_id = it->second;
    }

    aircraft_states_[aircraft_id] = confirmation.resulting_state;

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

void MMA::scheduleChunkTimeoutChecks() {
  if (!running_ || !chunk_timeout_timer_) {
    return;
  }

  chunk_timeout_timer_->expires_after(kMissingChunkCheckInterval);
  chunk_timeout_timer_->async_wait([this](const std::error_code& ec) {
    if (ec || !running_) {
      return;
    }

    processMissingChunkTimeouts();
    scheduleChunkTimeoutChecks();
  });
}

void MMA::processMissingChunkTimeouts() {
  const auto now = std::chrono::steady_clock::now();

  for (auto& [aircraft_id, aircraft_buffers] : image_reassembly_buffers_) {
    auto retry_state_aircraft_it = image_reassembly_retry_state_.find(aircraft_id);
    if (retry_state_aircraft_it == image_reassembly_retry_state_.end()) {
      continue;
    }

    auto& retry_states = retry_state_aircraft_it->second;
    std::vector<uint32_t> images_to_drop;

    for (auto& [image_id, buffer] : aircraft_buffers) {
      auto retry_state_it = retry_states.find(image_id);
      if (retry_state_it == retry_states.end()) {
        continue;
      }

      auto& retry_state = retry_state_it->second;
      if (now - retry_state.last_chunk_time < kMissingChunkTimeout) {
        continue;
      }

      bool should_drop_image = false;

      for (uint16_t idx = 0; idx < buffer.total_chunks; ++idx) {
        if (buffer.received[idx]) {
          continue;
        }

        if (idx >= retry_state.retry_attempts_per_chunk.size()) {
          continue;
        }

        if (retry_state.retry_attempts_per_chunk[idx] < kMaxMissingChunkRetryAttempts) {
          sendChunkRetryRequest(aircraft_id, image_id, idx);
          ++retry_state.retry_attempts_per_chunk[idx];
        } else {
          spdlog::error(
              "Image {} from aircraft {} missing chunk {} after retry; aborting reassembly",
              image_id, aircraft_id, idx);
          should_drop_image = true;
        }
      }

      retry_state.last_chunk_time = now;
      if (should_drop_image) {
        images_to_drop.push_back(image_id);
      }
    }

    for (const auto image_id : images_to_drop) {
      aircraft_buffers.erase(image_id);
      retry_states.erase(image_id);
    }
  }
}

void MMA::sendChunkRetryRequest(uint64_t aircraftId, uint32_t imageId, uint16_t chunkIndex) {
  const auto connection_it = verified_connections_.find(aircraftId);
  if (connection_it == verified_connections_.end()) {
    spdlog::warn("Cannot request missing chunk {} for image {}: aircraft {} not verified/connected",
                 chunkIndex, imageId, aircraftId);
    return;
  }

  const network::SchematicChunkRetryRequest request{imageId, chunkIndex};
  const auto packet
      = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK_RETRY_REQUEST, request);
  connection_it->second->send(packet);

  spdlog::warn("Requested retry for missing chunk {} of image {} from aircraft {}", chunkIndex,
               imageId, aircraftId);
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
      std::cout << "Connected aircraft (" << verified_connections_.size() << "):\n";
      for (auto& [aircraft_id, conn] : verified_connections_) {
        std::cout << std::format("  - Aircraft {} [state: VERIFIED]\n", aircraft_id);
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
  spdlog::default_logger()->flush();
}

void MMA::sendDiagnosticCodeClearRequest(uint64_t aircraftId, int32_t code) {
  const auto connection_it = verified_connections_.find(aircraftId);
  if (connection_it == verified_connections_.end()) {
    spdlog::warn("Cannot send diagnostic code clear command: aircraft {} is not verified",
                 aircraftId);
    return;
  }

  const auto state_it = aircraft_states_.find(aircraftId);
  if (state_it == aircraft_states_.end()) {
    spdlog::warn(
        "Cannot send diagnostic code clear command to aircraft {}: state is unknown. "
        "Wait for state confirmation from the aircraft.",
        aircraftId);
    return;
  }

  if (state_it->second != network::StateId::MAINTENANCE
      && state_it->second != network::StateId::FAULT) {
    spdlog::warn(
        "Cannot send diagnostic code clear command to aircraft {}: aircraft is in {} (requires "
        "MAINTENANCE or FAULT)",
        aircraftId, network::stateIdToString(state_it->second));
    return;
  }

  network::DiagnosticCodeClearRequest request{code};
  const auto packet = network::serializePacket(network::PacketType::CLEAR_DIAGNOSTIC_CODE, request);
  connection_it->second->send(packet);
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