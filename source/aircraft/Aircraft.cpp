#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>
#include <aircraft/FaultState.h>
#include <aircraft/MaintenanceState.h>
#include <aircraft/StandbyState.h>
#include <aircraft/StateManager.h>
#include <common/Packet.h>
#include <common/WarrantyData.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <asio.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>

namespace {

  std::unique_ptr<BaseState> makeStateForId(aircraft::Aircraft& aircraft,
                                            StateManager& stateManager, network::StateId stateId) {
    switch (stateId) {
      case network::StateId::STANDBY:
        return std::make_unique<StandbyState>(aircraft, stateManager);
      case network::StateId::DIAGNOSTIC:
        return std::make_unique<DiagnosticState>(aircraft, stateManager);
      case network::StateId::MAINTENANCE:
        return std::make_unique<MaintenanceState>(aircraft, stateManager);
      case network::StateId::FAULT:
        return std::make_unique<FaultState>(aircraft, stateManager);
      default:
        return nullptr;
    }
  }

  bool isAllowedTransition(network::StateId currentState, network::StateId targetState) {
    switch (currentState) {
      case network::StateId::STANDBY:
        return targetState == network::StateId::DIAGNOSTIC;
      case network::StateId::DIAGNOSTIC:
        return targetState == network::StateId::MAINTENANCE;
      case network::StateId::MAINTENANCE:
        return targetState == network::StateId::STANDBY || targetState == network::StateId::FAULT;
      case network::StateId::FAULT:
        return targetState == network::StateId::STANDBY
               || targetState == network::StateId::DIAGNOSTIC;
      default:
        return false;
    }
  }

  std::optional<network::StateId> stateIdFromString(const std::string& stateName) {
    if (stateName == "STANDBY") return network::StateId::STANDBY;
    if (stateName == "DIAGNOSTIC") return network::StateId::DIAGNOSTIC;
    if (stateName == "MAINTENANCE") return network::StateId::MAINTENANCE;
    if (stateName == "FAULT") return network::StateId::FAULT;
    return std::nullopt;
  }

  std::string_view stateNameFromId(network::StateId stateId) {
    switch (stateId) {
      case network::StateId::STANDBY:
        return "STANDBY";
      case network::StateId::DIAGNOSTIC:
        return "DIAGNOSTIC";
      case network::StateId::MAINTENANCE:
        return "MAINTENANCE";
      case network::StateId::FAULT:
        return "FAULT";
      default:
        return "UNKNOWN";
    }
  }

  std::string_view transitionSourceToString(aircraft::TransitionSource source) {
    switch (source) {
      case aircraft::TransitionSource::MMA_COMMAND:
        return "MMA_COMMAND";
      case aircraft::TransitionSource::AUTOMATIC:
        return "AUTOMATIC";
      case aircraft::TransitionSource::MANUAL:
        return "MANUAL";
      case aircraft::TransitionSource::CONNECTION_FALLBACK:
        return "CONNECTION_FALLBACK";
      default:
        return "UNKNOWN";
    }
  }

}  // namespace

using namespace aircraft;

Aircraft::Aircraft()
    : m_currentState("STANDBY"),
      network_io_context_(std::make_unique<asio::io_context>()),
      network_work_guard_(
          std::make_unique<NetworkWorkGuard>(asio::make_work_guard(*network_io_context_))),
      network_thread_([this]() { network_io_context_->run(); }) {
  // Initialize with sample data for demonstration
  // In production, this would come from server/ persistence

  // Sample maintenance data
  m_lastMaintenance.lastMaintenance = std::chrono::system_clock::now();
  m_lastMaintenance.technician = "John Doe";
  m_lastMaintenance.notes = "Routine checkup";

  // Sample fault codes
  m_faultCodes.push_back({101, network::DiagnosticFaultSeverity::MINOR,
                          "Engine temperature sensor fault - left engine",
                          std::chrono::system_clock::now()});
  m_faultCodes.push_back({102, network::DiagnosticFaultSeverity::MAJOR,
                          "Hydraulic pressure low - right wing", std::chrono::system_clock::now()});
  m_faultCodes.push_back({203, network::DiagnosticFaultSeverity::MINOR,
                          "Altitude indicator disagreement", std::chrono::system_clock::now()});
  m_faultCodes.push_back({900, network::DiagnosticFaultSeverity::MINOR,
                          "Galley refrigeration fault - forward galley",
                          std::chrono::system_clock::now()});
  m_faultCodes.push_back({903, network::DiagnosticFaultSeverity::MINOR,
                          "In-flight entertainment system fault",
                          std::chrono::system_clock::now()});
  m_faultCodes.push_back({909, network::DiagnosticFaultSeverity::MINOR, "WiFi connectivity issue",
                          std::chrono::system_clock::now()});

  // Sample warranty data
  m_warranty.isActive = true;
  m_warranty.expiryDate = "2027-12-31";
  m_warranty.provider = "Aviation Warranty Corp";
}

Aircraft::~Aircraft() {
  shutting_down_.store(true);

  if (connection_) {
    connection_->setMessageHandler(nullptr);
    connection_->close();
    connection_.reset();
  }

  if (network_work_guard_) {
    network_work_guard_->reset();
  }
  if (network_io_context_) {
    network_io_context_->stop();
  }
  if (network_thread_.joinable()) {
    network_thread_.join();
  }
}

void Aircraft::initialize() {
  // TODO: Load persisted data
}

std::string Aircraft::getCurrentState() const { return m_currentState; }

void Aircraft::setCurrentState(const std::string& state) {
  if (m_currentState == state) {
    return;
  }

  m_currentState = state;
}

MaintenanceInfo Aircraft::getLastMaintenance() const { return m_lastMaintenance; }

std::vector<FaultCode> Aircraft::getFaultCodes() const { return m_faultCodes; }

WarrantyInfo Aircraft::getWarranty() const { return m_warranty; }

void Aircraft::setLastMaintenance(const MaintenanceInfo& info) { m_lastMaintenance = info; }

void Aircraft::addFaultCode(const FaultCode& code) {
  m_faultCodes.push_back(code);
  evaluateAutomaticTransitionFromCurrentState();
}

bool Aircraft::resolveFaultCode(int code) {
  const auto beforeSize = m_faultCodes.size();
  m_faultCodes.erase(std::remove_if(m_faultCodes.begin(), m_faultCodes.end(),
                                    [code](const FaultCode& fault) { return fault.code == code; }),
                     m_faultCodes.end());

  if (m_faultCodes.size() == beforeSize) {
    return false;
  }

  evaluateAutomaticTransitionFromCurrentState();
  return true;
}

void Aircraft::clearFaultCodes() {
  m_faultCodes.clear();
  evaluateAutomaticTransitionFromCurrentState();
}

void Aircraft::setWarranty(const WarrantyInfo& info) { m_warranty = info; }

bool Aircraft::hasAnyFaults() const { return !m_faultCodes.empty(); }

bool Aircraft::hasMajorFaults() const {
  return std::any_of(m_faultCodes.begin(), m_faultCodes.end(), [](const FaultCode& fault) {
    return fault.severity == network::DiagnosticFaultSeverity::MAJOR;
  });
}

bool Aircraft::hasOnlyMinorFaults() const { return hasAnyFaults() && !hasMajorFaults(); }

void Aircraft::evaluateAutomaticTransitionFromCurrentState() {
  if (automatic_transition_in_progress_) {
    return;
  }

  const auto currentState = stateIdFromString(m_currentState);
  if (!currentState) {
    return;
  }

  std::optional<network::StateId> targetState;
  switch (*currentState) {
    case network::StateId::MAINTENANCE:
      if (hasMajorFaults()) {
        targetState = network::StateId::FAULT;
      } else if (!hasAnyFaults()) {
        targetState = network::StateId::STANDBY;
      }
      break;
    case network::StateId::FAULT:
      if (!hasAnyFaults()) {
        targetState = network::StateId::STANDBY;
      } else if (hasOnlyMinorFaults()) {
        targetState = network::StateId::DIAGNOSTIC;
      }
      break;
    default:
      break;
  }

  if (!targetState) {
    return;
  }

  automatic_transition_in_progress_ = true;
  transitionToState(*targetState, TransitionSource::AUTOMATIC);
  automatic_transition_in_progress_ = false;
}

void Aircraft::connectToMMA(const std::string& host, uint16_t port) {
  verified_ = false;

  auto socket = std::make_shared<asio::ip::tcp::socket>(*network_io_context_);
  auto timer = std::make_shared<asio::steady_timer>(*network_io_context_);

  timer->expires_after(std::chrono::seconds(5));
  timer->async_wait([this, socket](std::error_code ec) {
    if (shutting_down_.load()) {
      return;
    }

    if (!ec) {
      // REQ-CLT-082
      spdlog::error("Connection timeout, changing to DIAGNOSTIC");
      if (!transitionToState(network::StateId::DIAGNOSTIC, TransitionSource::CONNECTION_FALLBACK)) {
        spdlog::warn("Connection fallback transition to DIAGNOSTIC was rejected");
      }

      socket->close();
    }
  });

  socket->async_connect(
      asio::ip::tcp::endpoint(asio::ip::make_address(host), port),
      [this, socket, timer](std::error_code ec) {
        if (shutting_down_.load()) {
          return;
        }

        timer->cancel();
        if (ec) {
          spdlog::error("Connect failed: {}", ec.message());
          if (!transitionToState(network::StateId::DIAGNOSTIC,
                                 TransitionSource::CONNECTION_FALLBACK)) {
            spdlog::warn("Connection fallback transition to DIAGNOSTIC was rejected");
          }
        } else {
          spdlog::info("Connected to MMA server");
          connection_ = network::TcpConnection::create(std::move(*socket));
          connection_->setMessageHandler([this](const auto& data) { onNetworkMessage(data); });
          connection_->start();
        }
      });
}

void Aircraft::setStateManager(StateManager* stateManager) { stateManager_ = stateManager; }

void Aircraft::syncStateManagerToCurrentState() {
  if (!stateManager_) {
    spdlog::warn("StateManager unavailable. Cannot sync current state {}", m_currentState);
    return;
  }

  const auto currentState = stateIdFromString(m_currentState);
  if (!currentState) {
    spdlog::warn("Cannot sync unknown aircraft state {}", m_currentState);
    return;
  }

  auto state = makeStateForId(*this, *stateManager_, *currentState);
  if (!state) {
    spdlog::warn("Cannot sync unsupported aircraft state {}", m_currentState);
    return;
  }

  stateManager_->SetState(std::move(state));
}

bool Aircraft::transitionToState(network::StateId targetState, TransitionSource source) {
  const auto currentState = stateIdFromString(m_currentState);
  if (!currentState) {
    spdlog::warn("Cannot transition from unknown aircraft state {}", m_currentState);
    return false;
  }

  if (!isAllowedTransition(*currentState, targetState)) {
    spdlog::warn("Rejected transition from {} to {}", m_currentState,
                 network::stateIdToString(targetState));
    return false;
  }

  const std::string targetStateName{stateNameFromId(targetState)};
  if (targetStateName == "UNKNOWN") {
    return false;
  }

  const std::string previousState = m_currentState;
  setCurrentState(targetStateName);

  spdlog::info("Operational state transition: {} -> {} (source: {})", previousState,
               targetStateName, transitionSourceToString(source));

  if (verified_ && connection_ && connection_->getState() == network::ConnectionState::VERIFIED) {
    network::StateChangeConfirmation confirmation{targetState};
    const auto confirmationPacket
        = network::serializePacket(network::PacketType::STATE_CHANGE_CONFIRMATION, confirmation);
    connection_->send(confirmationPacket);
    spdlog::info("State change confirmation sent to MMA for state {}",
                 network::stateIdToString(targetState));
  }

  if (!stateManager_) {
    spdlog::warn("StateManager unavailable. State string updated to {} only", targetStateName);
    if (targetState == network::StateId::MAINTENANCE && hasMajorFaults()) {
      evaluateAutomaticTransitionFromCurrentState();
    }
    return true;
  }

  stateManager_->SetState(makeStateForId(*this, *stateManager_, targetState));

  // On MAINTENANCE entry, immediately escalate to FAULT if MAJOR faults are already present.
  if (targetState == network::StateId::MAINTENANCE && hasMajorFaults()) {
    evaluateAutomaticTransitionFromCurrentState();
  }

  return true;
}

bool Aircraft::sendDiagnosticData() {
  if (!verified_ || !connection_) {
    spdlog::warn("Cannot send diagnostic data before verification/connection");
    return false;
  }

  std::vector<network::DiagnosticFaultCode> fault_payload;
  fault_payload.reserve(m_faultCodes.size());
  for (const auto& fault : m_faultCodes) {
    const auto timestamp_ms
        = std::chrono::duration_cast<std::chrono::milliseconds>(fault.timestamp.time_since_epoch())
              .count();
    fault_payload.push_back(network::DiagnosticFaultCode{
        fault.code,
        timestamp_ms,
        fault.severity,
        fault.description,
    });
  }

  const auto payload = network::serializeDiagnosticDataPayload(fault_payload);
  const auto packet = network::serializePacket(network::PacketType::DIAGNOSTIC_DATA, payload.data(),
                                               payload.size());
  connection_->send(packet);
  spdlog::info("Sent {} fault codes to MMA", fault_payload.size());
  return true;
}

bool Aircraft::sendImageFromFile(const std::string& filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file) {
    spdlog::error("Failed to open image file: {}", filepath);
    return false;
  }
  file.seekg(0, std::ios::end);
  const std::streampos file_size_pos = file.tellg();
  const size_t file_size = static_cast<size_t>(file_size_pos);
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> image_data(file_size);
  file.read(reinterpret_cast<char*>(image_data.data()), static_cast<std::streamsize>(file_size));

  if (!file || file.gcount() != static_cast<std::streamsize>(file_size)) {
    spdlog::error("Failed to read image file: {}: expected {} bytes, got {}", filepath, file_size,
                  file.gcount());
    return false;
  }

  return sendImage(image_data, network::ImageFormat::PNG);
}

bool Aircraft::sendImage(const std::vector<uint8_t>& image_data, network::ImageFormat format) {
  if (!verified_ || !connection_) {
    spdlog::warn("Cannot send image before verification/connection");
    return false;
  }

  if (image_data.empty()) {
    spdlog::warn("Cannot send empty image");
    return false;
  }

  // Generate unique image ID
  const uint32_t image_id = next_image_id_++;

  // Serialize image into chunks
  const auto chunk_payloads = network::serializeImagePayload(image_id, image_data, format);

  if (chunk_payloads.empty()) {
    spdlog::error("Failed to serialize image {}", image_id);
    return false;
  }

  // Send each chunk as a separate SCHEMATIC_CHUNK packet
  for (size_t i = 0; i < chunk_payloads.size(); ++i) {
    const auto& chunk_payload = chunk_payloads[i];
    const auto packet = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK,
                                                 chunk_payload.data(), chunk_payload.size());
    connection_->send(packet);
  }

  sent_image_chunk_payloads_[image_id] = chunk_payloads;
  sent_image_cache_order_.push_back(image_id);
  while (sent_image_cache_order_.size() > kMaxCachedImagesForRetry) {
    const uint32_t evict_image_id = sent_image_cache_order_.front();
    sent_image_cache_order_.pop_front();
    sent_image_chunk_payloads_.erase(evict_image_id);
  }

  spdlog::info("Sent image {} in {} chunks ({} bytes total)", image_id, chunk_payloads.size(),
               image_data.size());
  return true;
}

void Aircraft::onNetworkMessage(const std::vector<uint8_t>& data) {
  if (shutting_down_.load()) {
    return;
  }

  network::PacketHeader header;
  std::vector<uint8_t> payload;
  if (!network::deserializePacket(data, header, payload)) return;

  if (!verified_) {
    if (header.type == network::PacketType::VERIFICATION_REQUEST) {
      if (payload.size() != sizeof(network::VerificationRequest)) {
        spdlog::error("Invalid verification request payload size: {}", payload.size());
        connection_->close();
        return;
      }
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
      sendLandedNotification();
    } else {
      spdlog::warn("Received non-verification packet before handshake, closing");
      spdlog::default_logger()->flush();
      connection_->close();
    }
  } else {
    // Handle verified commands (e.g., state changes from server)
    if (header.type == network::PacketType::STATE_CHANGE) {
      if (payload.size() != sizeof(network::StateChangeRequest)) {
        spdlog::warn("Invalid STATE_CHANGE payload size: {}", payload.size());
        return;
      }

      network::StateChangeRequest req{};
      std::memcpy(&req, payload.data(), sizeof(req));
      if (!transitionToState(req.target_state, TransitionSource::MMA_COMMAND)) {
        spdlog::warn("STATE_CHANGE rejected: invalid target state {}",
                     network::stateIdToString(req.target_state));
        return;
      }
    } else if (header.type == network::PacketType::SCHEMATIC_CHUNK) {
      // Handle image chunk reception
      network::ImageChunkHeader chunk_header;
      std::vector<uint8_t> chunk_data;
      if (!network::deserializeImageChunk(payload, chunk_header, chunk_data)) {
        spdlog::warn("Invalid image chunk format");
        return;
      }

      spdlog::info("Received image chunk {}/{} (image_id: {}, data: {} bytes)",
                   chunk_header.chunk_index + 1, chunk_header.total_chunks, chunk_header.image_id,
                   chunk_data.size());

      // Get or create reassembly buffer for this image_id
      auto it = image_reassembly_buffers_.find(chunk_header.image_id);
      if (it == image_reassembly_buffers_.end()) {
        // Create new reassembly buffer
        image_reassembly_buffers_.emplace(
            chunk_header.image_id, network::ImageBuffer(chunk_header.image_id, chunk_header.format,
                                                        chunk_header.total_chunks));
        it = image_reassembly_buffers_.find(chunk_header.image_id);
      }

      if (!it->second.setExpectedImageCrc(chunk_header.image_crc32)) {
        spdlog::warn(
            "Rejected image chunk for image {} due to inconsistent full-image CRC (expected "
            "0x{:08X}, got 0x{:08X})",
            chunk_header.image_id, it->second.expected_image_crc32, chunk_header.image_crc32);
        return;
      }

      // Add chunk to buffer
      if (it->second.addChunk(chunk_header.chunk_index, chunk_data)) {
        // Image is now complete
        const auto complete_image = it->second.reassemble();

        if (!it->second.validateReassembledCrc(complete_image)) {
          spdlog::error(
              "Reassembled image {} failed CRC validation (expected 0x{:08X}, computed "
              "0x{:08X})",
              chunk_header.image_id, it->second.expected_image_crc32,
              network::Crc32::calculate(complete_image.data(), complete_image.size()));
          image_reassembly_buffers_.erase(it);
          return;
        }

        spdlog::info("Image {} received and reassembled ({} bytes total, format: {})",
                     chunk_header.image_id, complete_image.size(),
                     network::imageFormatToString(chunk_header.format));
        image_reassembly_buffers_.erase(it);
      }
      return;
    }

    if (header.type == network::PacketType::SCHEMATIC_CHUNK_RETRY_REQUEST) {
      if (payload.size() != sizeof(network::SchematicChunkRetryRequest)) {
        spdlog::warn("Invalid SCHEMATIC_CHUNK_RETRY_REQUEST payload size: {}", payload.size());
        return;
      }

      network::SchematicChunkRetryRequest request{};
      std::memcpy(&request, payload.data(), sizeof(request));

      const auto cached_image_it = sent_image_chunk_payloads_.find(request.image_id);
      if (cached_image_it == sent_image_chunk_payloads_.end()) {
        spdlog::warn("Retry requested for unknown/evicted image {}", request.image_id);
        return;
      }

      if (request.chunk_index >= cached_image_it->second.size()) {
        spdlog::warn("Retry requested with invalid chunk index {} for image {}",
                     request.chunk_index, request.image_id);
        return;
      }

      const auto& chunk_payload = cached_image_it->second[request.chunk_index];
      const auto packet = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK,
                                                   chunk_payload.data(), chunk_payload.size());
      connection_->send(packet);
      spdlog::info("Resent chunk {} for image {} after MMA retry request", request.chunk_index,
                   request.image_id);
      return;
    }

    if (header.type == network::PacketType::CLEAR_DIAGNOSTIC_CODE) {
      auto send_confirmation = [this](int32_t code, network::DiagnosticCodeClearStatus status) {
        if (!connection_ || connection_->getState() != network::ConnectionState::VERIFIED) {
          return;
        }

        const auto resultingState
            = stateIdFromString(m_currentState).value_or(network::StateId::STANDBY);
        const network::DiagnosticCodeClearConfirmation confirmation{code, status, resultingState};
        const auto packet = network::serializePacket(
            network::PacketType::CLEAR_DIAGNOSTIC_CODE_CONFIRMATION, confirmation);
        connection_->send(packet);
      };

      if (payload.size() != sizeof(network::DiagnosticCodeClearRequest)) {
        spdlog::warn("Invalid CLEAR_DIAGNOSTIC_CODE payload size: {}", payload.size());
        send_confirmation(0, network::DiagnosticCodeClearStatus::MALFORMED_REQUEST);
        return;
      }

      network::DiagnosticCodeClearRequest request{};
      std::memcpy(&request, payload.data(), sizeof(request));

      const auto currentState = stateIdFromString(m_currentState);
      if (!currentState
          || (*currentState != network::StateId::MAINTENANCE
              && *currentState != network::StateId::FAULT)) {
        spdlog::warn(
            "Rejected CLEAR_DIAGNOSTIC_CODE for code {} while in state {} (requires "
            "MAINTENANCE or FAULT)",
            request.code, m_currentState);
        send_confirmation(request.code,
                          network::DiagnosticCodeClearStatus::REJECTED_NOT_IN_CLEARABLE_STATE);
        return;
      }

      if (!resolveFaultCode(request.code)) {
        spdlog::warn("Rejected CLEAR_DIAGNOSTIC_CODE for missing code {}", request.code);
        send_confirmation(request.code, network::DiagnosticCodeClearStatus::CODE_NOT_FOUND);
        return;
      }

      spdlog::info("Cleared diagnostic code {} via MMA command", request.code);
      send_confirmation(request.code, network::DiagnosticCodeClearStatus::SUCCESS);
      return;
    }
  }
}

void Aircraft::sendLandedNotification() {
  if (!connection_ || connection_->getState() != network::ConnectionState::VERIFIED) {
    spdlog::error("Cannot send landed notification: not connected/verified");
    spdlog::default_logger()->flush();
    return;
  }
  auto packet = network::serializePacket(network::PacketType::LANDED_NOTIFICATION, nullptr, 0);
  connection_->send(packet);

  // REQ-CLT-054
  spdlog::info("Landed notification sent to MMA");
}