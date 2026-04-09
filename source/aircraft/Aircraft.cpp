#include <aircraft/Aircraft.h>
#include <aircraft/DiagnosticState.h>
#include <aircraft/FaultState.h>
#include <aircraft/MaintenanceState.h>
#include <aircraft/StandbyState.h>
#include <aircraft/StateManager.h>
#include <common/Packet.h>
#include <common/WarrantyData.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <chrono>
#include <iomanip>
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

void Aircraft::addFaultCode(const FaultCode& code) { m_faultCodes.push_back(code); }

void Aircraft::clearFaultCodes() { m_faultCodes.clear(); }

void Aircraft::setWarranty(const WarrantyInfo& info) { m_warranty = info; }

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
    return true;
  }

  stateManager_->SetState(makeStateForId(*this, *stateManager_, targetState));

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