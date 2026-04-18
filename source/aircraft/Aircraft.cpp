/**
 * @file Aircraft.cpp
 * @brief Implements the aircraft domain model and MMA client workflows.
 */

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
#include <random>
#include <sstream>

namespace {

  static std::unique_ptr<aircraft::BaseState> makeStateForId(aircraft::Aircraft& aircraft,
                                                             aircraft::StateManager& stateManager,
                                                             network::StateId stateId) {
    std::unique_ptr<aircraft::BaseState> result;

    switch (stateId) {
      case network::StateId::STANDBY:
        result = std::make_unique<aircraft::StandbyState>(aircraft, stateManager);
        break;
      case network::StateId::DIAGNOSTIC:
        result = std::make_unique<aircraft::DiagnosticState>(aircraft, stateManager);
        break;
      case network::StateId::MAINTENANCE:
        result = std::make_unique<aircraft::MaintenanceState>(aircraft, stateManager);
        break;
      case network::StateId::FAULT:
        result = std::make_unique<aircraft::FaultState>(aircraft, stateManager);
        break;
      default:
        // Unknown state id.
        break;
    }

    return result;
  }

  static bool isAllowedTransition(network::StateId currentState, network::StateId targetState) {
    bool allowed = false;

    switch (currentState) {
      case network::StateId::STANDBY:
        allowed = (targetState == network::StateId::DIAGNOSTIC);
        break;
      case network::StateId::DIAGNOSTIC:
        allowed = (targetState == network::StateId::MAINTENANCE);
        break;
      case network::StateId::MAINTENANCE:
        allowed = (targetState == network::StateId::STANDBY)
                  || (targetState == network::StateId::FAULT);
        break;
      case network::StateId::FAULT:
        allowed = (targetState == network::StateId::STANDBY)
                  || (targetState == network::StateId::DIAGNOSTIC);
        break;
      default:
        // Unknown state id.
        break;
    }

    return allowed;
  }

  static std::optional<network::StateId> stateIdFromString(const std::string& stateName) {
    std::optional<network::StateId> result;

    if (stateName == "STANDBY") {
      result = network::StateId::STANDBY;
    } else if (stateName == "DIAGNOSTIC") {
      result = network::StateId::DIAGNOSTIC;
    } else if (stateName == "MAINTENANCE") {
      result = network::StateId::MAINTENANCE;
    } else if (stateName == "FAULT") {
      result = network::StateId::FAULT;
    } else {
      result = std::nullopt;
    }

    return result;
  }

  static std::string_view stateNameFromId(network::StateId stateId) {
    std::string_view result = "UNKNOWN";

    switch (stateId) {
      case network::StateId::STANDBY:
        result = "STANDBY";
        break;
      case network::StateId::DIAGNOSTIC:
        result = "DIAGNOSTIC";
        break;
      case network::StateId::MAINTENANCE:
        result = "MAINTENANCE";
        break;
      case network::StateId::FAULT:
        result = "FAULT";
        break;
      default:
        // Unknown state id.
        break;
    }

    return result;
  }

  static std::string_view transitionSourceToString(aircraft::TransitionSource source) {
    std::string_view result = "UNKNOWN";

    switch (source) {
      case aircraft::TransitionSource::MMA_COMMAND:
        result = "MMA_COMMAND";
        break;
      case aircraft::TransitionSource::AUTOMATIC:
        result = "AUTOMATIC";
        break;
      case aircraft::TransitionSource::MANUAL:
        result = "MANUAL";
        break;
      case aircraft::TransitionSource::CONNECTION_FALLBACK:
        result = "CONNECTION_FALLBACK";
        break;
      default:
        // Unknown transition source.
        break;
    }

    return result;
  }

}  // namespace

namespace aircraft {

  Aircraft::Aircraft()
      : m_currentState("STANDBY"),
        network_io_context_(std::make_unique<asio::io_context>()),
        network_work_guard_(
            std::make_unique<NetworkWorkGuard>(asio::make_work_guard(*network_io_context_))),
        network_thread_([this]() { (void)network_io_context_->run(); }) {
    // Generate random 5-digit aircraft ID (10000-99999)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(10000, 99999);
    aircraft_id_ = dist(gen);

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
                            "Hydraulic pressure low - right wing",
                            std::chrono::system_clock::now()});
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
    if (m_currentState != state) {
      m_currentState = state;
    }
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
    const auto removed_count = std::erase_if(
        m_faultCodes, [code](const FaultCode& fault) { return fault.code == code; });

    bool resolved = false;
    if (removed_count > 0U) {
      evaluateAutomaticTransitionFromCurrentState();
      resolved = true;
    }

    return resolved;
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
    if (!automatic_transition_in_progress_) {
      const auto currentState = stateIdFromString(m_currentState);
      if (currentState) {
        std::optional<network::StateId> targetState;
        switch (*currentState) {
          case network::StateId::MAINTENANCE:
            if (hasMajorFaults()) {
              targetState = network::StateId::FAULT;
            } else if (!hasAnyFaults()) {
              targetState = network::StateId::STANDBY;
            } else {
              // No automatic transition.
            }
            break;
          case network::StateId::FAULT:
            if (!hasAnyFaults()) {
              targetState = network::StateId::STANDBY;
            } else if (hasOnlyMinorFaults()) {
              targetState = network::StateId::DIAGNOSTIC;
            } else {
              // No automatic transition.
            }
            break;
          default:
            // No automatic transition from other states.
            break;
        }

        if (targetState) {
          automatic_transition_in_progress_ = true;
          const bool transitioned = transitionToState(*targetState, TransitionSource::AUTOMATIC);
          if (!transitioned) {
            spdlog::warn("Automatic transition from {} to {} was rejected", m_currentState,
                         network::stateIdToString(*targetState));
          }
          automatic_transition_in_progress_ = false;
        }
      }
    }
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
        if (!transitionToState(network::StateId::DIAGNOSTIC,
                               TransitionSource::CONNECTION_FALLBACK)) {
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
    bool ok = true;
    bool transitioned_any = false;
    network::StateId requested_target = targetState;
    TransitionSource requested_source = source;

    bool continue_transitioning = true;
    while (continue_transitioning) {
      continue_transitioning = false;

      const auto currentState = stateIdFromString(m_currentState);
      if (!currentState) {
        spdlog::warn("Cannot transition from unknown aircraft state {}", m_currentState);
        ok = false;
      }

      if (ok && !isAllowedTransition(*currentState, requested_target)) {
        spdlog::warn("Rejected transition from {} to {}", m_currentState,
                     network::stateIdToString(requested_target));
        ok = false;
      }

      const std::string targetStateName{stateNameFromId(requested_target)};
      if (ok && (targetStateName == "UNKNOWN")) {
        ok = false;
      }

      if (ok) {
        const std::string previousState = m_currentState;
        setCurrentState(targetStateName);

        spdlog::info("Operational state transition: {} -> {} (source: {})", previousState,
                     targetStateName, transitionSourceToString(requested_source));

        if (verified_ && connection_
            && connection_->getState() == network::ConnectionState::VERIFIED) {
          network::StateChangeConfirmation confirmation{requested_target};
          const auto confirmationPacket = network::serializePacket(
              network::PacketType::STATE_CHANGE_CONFIRMATION, confirmation);
          connection_->send(confirmationPacket);
          spdlog::info("State change confirmation sent to MMA for state {}",
                       network::stateIdToString(requested_target));
        }

        if (!stateManager_) {
          spdlog::warn("StateManager unavailable. State string updated to {} only",
                       targetStateName);
        } else {
          stateManager_->SetState(makeStateForId(*this, *stateManager_, requested_target));
        }

        transitioned_any = true;

        // On MAINTENANCE entry, immediately escalate to FAULT if MAJOR faults are already present.
        if ((requested_target == network::StateId::MAINTENANCE) && hasMajorFaults()) {
          requested_target = network::StateId::FAULT;
          requested_source = TransitionSource::AUTOMATIC;
          continue_transitioning = true;
        }
      }
    }

    return ok && transitioned_any;
  }

  bool Aircraft::sendDiagnosticData() {
    bool ok = true;
    if (!verified_ || !connection_) {
      spdlog::warn("Cannot send diagnostic data before verification/connection");
      ok = false;
    }

    std::vector<network::DiagnosticFaultCode> fault_payload;
    if (ok) {
      fault_payload.reserve(m_faultCodes.size());
      for (const auto& fault : m_faultCodes) {
        const auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      fault.timestamp.time_since_epoch())
                                      .count();
        fault_payload.push_back(network::DiagnosticFaultCode{
            fault.code,
            timestamp_ms,
            fault.severity,
            fault.description,
        });
      }

      const auto payload = network::serializeDiagnosticDataPayload(fault_payload);
      const auto packet = network::serializePacket(network::PacketType::DIAGNOSTIC_DATA, payload);
      connection_->send(packet);
      spdlog::info("Sent {} fault codes to MMA", fault_payload.size());
    }

    return ok;
  }

  bool Aircraft::sendWarrantyData() {
    bool ok = true;
    if (!verified_ || !connection_) {
      spdlog::warn("Cannot send warranty data before verification/connection");
      ok = false;
    }

    if (ok) {
      const auto payload = network::serializeWarrantyDataPayload(m_warranty);
      const auto packet = network::serializePacket(network::PacketType::WARRANTY_DATA, payload);
      connection_->send(packet);
      spdlog::info("Sent warranty data to MMA (active: {}, expiry: {}, provider: {})",
                   m_warranty.isActive, m_warranty.expiryDate, m_warranty.provider);
    }

    return ok;
  }

  bool Aircraft::canSendDiagnosticStageData() const {
    return verified_ && connection_
           && connection_->getState() == network::ConnectionState::VERIFIED;
  }

  void Aircraft::markDiagnosticRequestedByMMA() {
    // Reserved for future sequencing behavior when DIAGNOSTIC was explicitly requested by MMA.
  }

  bool Aircraft::sendImageFromFile(const std::string& filepath) {
    bool ok = true;
    bool sent = false;
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
      spdlog::error("Failed to open image file: {}", filepath);
      ok = false;
    }

    size_t file_size = 0;
    if (ok) {
      ok = ok && static_cast<bool>(file.seekg(0, std::ios::end));
      const std::streampos file_size_pos = file.tellg();
      file_size = static_cast<size_t>(file_size_pos);
      ok = ok && static_cast<bool>(file.seekg(0, std::ios::beg));
    }

    std::vector<char> raw_data;
    if (ok) {
      raw_data.resize(file_size);
      if (file_size > 0U) {
        ok = ok
             && static_cast<bool>(
                 file.read(raw_data.data(), static_cast<std::streamsize>(file_size)));
      }
    }

    if (ok && (file.gcount() != static_cast<std::streamsize>(file_size))) {
      ok = false;
    }

    if (!ok) {
      spdlog::error("Failed to read image file: {}: expected {} bytes, got {}", filepath, file_size,
                    file.gcount());
    }

    if (ok) {
      std::vector<uint8_t> image_data(file_size);
      for (size_t i = 0; i < file_size; ++i) {
        image_data[i] = static_cast<uint8_t>(raw_data[i]);
      }

      sent = sendImage(image_data, network::ImageFormat::PNG);
    }

    return ok && sent;
  }

  bool Aircraft::sendImage(const std::vector<uint8_t>& image_data, network::ImageFormat format) {
    bool ok = true;
    if (!verified_ || !connection_) {
      spdlog::warn("Cannot send image before verification/connection");
      ok = false;
    }

    if (ok && image_data.empty()) {
      spdlog::warn("Cannot send empty image");
      ok = false;
    }

    uint32_t image_id = 0;
    std::vector<std::vector<uint8_t>> chunk_payloads;
    if (ok) {
      // Generate unique image ID
      image_id = next_image_id_++;

      // Serialize image into chunks
      chunk_payloads = network::serializeImagePayload(image_id, image_data, format);
      if (chunk_payloads.empty()) {
        spdlog::error("Failed to serialize image {}", image_id);
        ok = false;
      }
    }

    if (ok) {
      // Send each chunk as a separate SCHEMATIC_CHUNK packet
      for (size_t i = 0; i < chunk_payloads.size(); ++i) {
        const auto& chunk_payload = chunk_payloads[i];
        const auto packet
            = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK, chunk_payload);
        connection_->send(packet);
      }

      sent_image_chunk_payloads_[image_id] = chunk_payloads;
      sent_image_cache_order_.push_back(image_id);
      while (sent_image_cache_order_.size() > kMaxCachedImagesForRetry) {
        const uint32_t evict_image_id = sent_image_cache_order_.front();
        sent_image_cache_order_.pop_front();
        const auto erased_count = sent_image_chunk_payloads_.erase(evict_image_id);
        if (erased_count == 0U) {
          spdlog::warn("Image cache eviction requested unknown image {}", evict_image_id);
        }
      }

      spdlog::info("Sent image {} in {} chunks ({} bytes total)", image_id, chunk_payloads.size(),
                   image_data.size());
    }

    return ok;
  }

  void Aircraft::onNetworkMessage(const std::vector<uint8_t>& data) {
    if (!shutting_down_.load()) {
      network::PacketHeader header{};
      std::vector<uint8_t> payload;

      const bool packet_ok = network::deserializePacket(data, header, payload);
      if (packet_ok) {
        if (!verified_) {
          switch (header.type) {
            case network::PacketType::VERIFICATION_REQUEST: {
              if (payload.size() != sizeof(network::VerificationRequest)) {
                spdlog::error("Invalid verification request payload size: {}", payload.size());
                if (connection_) {
                  connection_->close();
                }
              } else {
                network::VerificationRequest req{};
                (void)std::memcpy(&req, payload.data(), sizeof(req));
                network::VerificationResponse resp{};
                resp.challenge_response = req.challenge ^ 0xDEADBEEFU;
                resp.client_id = aircraft_id_;
                auto resp_packet
                    = network::serializePacket(network::PacketType::VERIFICATION_RESPONSE, resp);
                if (connection_) {
                  connection_->send(resp_packet);
                  verified_ = true;
                  connection_->setState(network::ConnectionState::VERIFIED);
                }
                spdlog::info("Verification successful, client ID {}", aircraft_id_);
                sendLandedNotification();
              }
              break;
            }
            default:
              spdlog::warn("Received non-verification packet before handshake, closing");
              spdlog::default_logger()->flush();
              if (connection_) {
                connection_->close();
              }
              break;
          }
        } else {
          // Handle verified commands (e.g., state changes from server)
          switch (header.type) {
            case network::PacketType::STATE_CHANGE: {
              if (payload.size() != sizeof(network::StateChangeRequest)) {
                spdlog::warn("Invalid STATE_CHANGE payload size: {}", payload.size());
              } else {
                network::StateChangeRequest req{};
                (void)std::memcpy(&req, payload.data(), sizeof(req));
                if (!transitionToState(req.target_state, TransitionSource::MMA_COMMAND)) {
                  spdlog::warn("STATE_CHANGE rejected: invalid target state {}",
                               network::stateIdToString(req.target_state));
                }
              }
              break;
            }
            case network::PacketType::SCHEMATIC_CHUNK: {
              network::ImageChunkHeader chunk_header{};
              std::vector<uint8_t> chunk_data;
              if (!network::deserializeImageChunk(payload, chunk_header, chunk_data)) {
                spdlog::warn("Invalid image chunk format");
              } else {
                spdlog::info("Received image chunk {}/{} (image_id: {}, data: {} bytes)",
                             chunk_header.chunk_index + 1, chunk_header.total_chunks,
                             chunk_header.image_id, chunk_data.size());

                // Get or create reassembly buffer for this image_id
                auto it = image_reassembly_buffers_.find(chunk_header.image_id);
                if (it == image_reassembly_buffers_.end()) {
                  const auto emplace_result = image_reassembly_buffers_.emplace(
                      chunk_header.image_id,
                      network::ImageBuffer(chunk_header.image_id, chunk_header.format,
                                           chunk_header.total_chunks));
                  it = emplace_result.first;
                  if (!emplace_result.second) {
                    spdlog::warn("Failed to create image reassembly buffer for image {}",
                                 chunk_header.image_id);
                  }
                }

                if (!it->second.setExpectedImageCrc(chunk_header.image_crc32)) {
                  spdlog::warn(
                      "Rejected image chunk for image {} due to inconsistent full-image CRC "
                      "(expected 0x{:08X}, got 0x{:08X})",
                      chunk_header.image_id, it->second.expected_image_crc32,
                      chunk_header.image_crc32);
                } else {
                  if (it->second.addChunk(chunk_header.chunk_index, chunk_data)) {
                    const auto complete_image = it->second.reassemble();

                    if (!it->second.validateReassembledCrc(complete_image)) {
                      spdlog::error(
                          "Reassembled image {} failed CRC validation (expected 0x{:08X}, computed "
                          "0x{:08X})",
                          chunk_header.image_id, it->second.expected_image_crc32,
                          network::Crc32::calculate(std::span<const uint8_t>(
                              complete_image.data(), complete_image.size())));
                      (void)image_reassembly_buffers_.erase(it);
                    } else {
                      spdlog::info("Image {} received and reassembled ({} bytes total, format: {})",
                                   chunk_header.image_id, complete_image.size(),
                                   network::imageFormatToString(chunk_header.format));
                      (void)image_reassembly_buffers_.erase(it);
                    }
                  }
                }
              }
              break;
            }
            case network::PacketType::SCHEMATIC_CHUNK_RETRY_REQUEST: {
              if (payload.size() != sizeof(network::SchematicChunkRetryRequest)) {
                spdlog::warn("Invalid SCHEMATIC_CHUNK_RETRY_REQUEST payload size: {}",
                             payload.size());
              } else {
                network::SchematicChunkRetryRequest request{};
                (void)std::memcpy(&request, payload.data(), sizeof(request));

                const auto cached_image_it = sent_image_chunk_payloads_.find(request.image_id);
                if (cached_image_it == sent_image_chunk_payloads_.end()) {
                  spdlog::warn("Retry requested for unknown/evicted image {}", request.image_id);
                } else if (request.chunk_index >= cached_image_it->second.size()) {
                  spdlog::warn("Retry requested with invalid chunk index {} for image {}",
                               request.chunk_index, request.image_id);
                } else {
                  const auto& chunk_payload = cached_image_it->second[request.chunk_index];
                  const auto packet = network::serializePacket(network::PacketType::SCHEMATIC_CHUNK,
                                                               chunk_payload);
                  if (connection_) {
                    connection_->send(packet);
                  }
                  spdlog::info("Resent chunk {} for image {} after MMA retry request",
                               request.chunk_index, request.image_id);
                }
              }
              break;
            }
            case network::PacketType::CLEAR_DIAGNOSTIC_CODE: {
              auto send_confirmation = [this](int32_t code,
                                              network::DiagnosticCodeClearStatus status) {
                const bool can_send
                    = connection_ && connection_->getState() == network::ConnectionState::VERIFIED;
                if (can_send) {
                  const auto resultingState
                      = stateIdFromString(m_currentState).value_or(network::StateId::STANDBY);
                  const network::DiagnosticCodeClearConfirmation confirmation{code, status,
                                                                              resultingState};
                  const auto packet = network::serializePacket(
                      network::PacketType::CLEAR_DIAGNOSTIC_CODE_CONFIRMATION, confirmation);
                  connection_->send(packet);
                }
              };

              if (payload.size() != sizeof(network::DiagnosticCodeClearRequest)) {
                spdlog::warn("Invalid CLEAR_DIAGNOSTIC_CODE payload size: {}", payload.size());
                send_confirmation(0, network::DiagnosticCodeClearStatus::MALFORMED_REQUEST);
              } else {
                network::DiagnosticCodeClearRequest request{};
                (void)std::memcpy(&request, payload.data(), sizeof(request));

                const auto currentState = stateIdFromString(m_currentState);
                if (!currentState
                    || ((*currentState != network::StateId::MAINTENANCE)
                        && (*currentState != network::StateId::FAULT))) {
                  spdlog::warn(
                      "Rejected CLEAR_DIAGNOSTIC_CODE for code {} while in state {} (requires "
                      "MAINTENANCE or FAULT)",
                      request.code, m_currentState);
                  send_confirmation(
                      request.code,
                      network::DiagnosticCodeClearStatus::REJECTED_NOT_IN_CLEARABLE_STATE);
                } else if (!resolveFaultCode(request.code)) {
                  spdlog::warn("Rejected CLEAR_DIAGNOSTIC_CODE for missing code {}", request.code);
                  send_confirmation(request.code,
                                    network::DiagnosticCodeClearStatus::CODE_NOT_FOUND);
                } else {
                  spdlog::info("Cleared diagnostic code {} via MMA command", request.code);
                  send_confirmation(request.code, network::DiagnosticCodeClearStatus::SUCCESS);
                }
              }
              break;
            }
            default:
              break;
          }
        }
      }
    }
  }

  void Aircraft::sendLandedNotification() {
    const bool can_send
        = connection_ && connection_->getState() == network::ConnectionState::VERIFIED;
    if (!can_send) {
      spdlog::error("Cannot send landed notification: not connected/verified");
      spdlog::default_logger()->flush();
    } else {
      auto packet = network::serializePacket(network::PacketType::LANDED_NOTIFICATION);
      connection_->send(packet);

      // REQ-CLT-054
      spdlog::info("Landed notification sent to MMA");
    }
  }

}  // namespace aircraft