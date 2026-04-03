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

}  // namespace

using namespace aircraft;

Aircraft::Aircraft()
  : m_currentState("STANDBY"),
    network_io_context_(std::make_unique<asio::io_context>()),
    network_work_guard_(std::make_unique<NetworkWorkGuard>(
      asio::make_work_guard(*network_io_context_))),
    network_thread_([this]() { network_io_context_->run(); }) {
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

Aircraft::~Aircraft() {
  if (connection_) {
    connection_->close();
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
  // REQ-CLT-056
  spdlog::info("Aircraft state changing from {} to {}", m_currentState, state);
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
    if (!ec) {
      // REQ-CLT-082
      spdlog::error("Connection timeout, changing to DIAGNOSTIC");
      setCurrentState("DIAGNOSTIC");

      socket->close();
    }
  });

  socket->async_connect(
      asio::ip::tcp::endpoint(asio::ip::make_address(host), port),
      [this, socket, timer](std::error_code ec) {
        timer->cancel();
        if (ec) {
          spdlog::error("Connect failed: {}", ec.message());
          setCurrentState("DIAGNOSTIC");
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

bool Aircraft::transitionToState(network::StateId targetState) {
  const auto currentState = stateIdFromString(m_currentState);
  if (!currentState) {
    spdlog::warn("Cannot transition from unknown aircraft state {}", m_currentState);
    return false;
  }

  if (!isAllowedTransition(*currentState, targetState)) {
    spdlog::warn("Rejected transition from {} to {}", m_currentState,
                 static_cast<int>(targetState));
    return false;
  }

  std::string state_name;
  switch (targetState) {
    case network::StateId::STANDBY:
      state_name = "STANDBY";
      break;
    case network::StateId::DIAGNOSTIC:
      state_name = "DIAGNOSTIC";
      break;
    case network::StateId::MAINTENANCE:
      state_name = "MAINTENANCE";
      break;
    case network::StateId::FAULT:
      state_name = "FAULT";
      break;
    default:
      return false;
  }

  if (!stateManager_) {
    setCurrentState(state_name);
    spdlog::warn("StateManager unavailable. State string updated to {} only", state_name);
    return true;
  }

  setCurrentState(state_name);
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
      spdlog::warn("Received non‑verification packet before handshake, closing");
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
      if (!transitionToState(req.target_state)) {
        spdlog::warn("STATE_CHANGE rejected: invalid target state {}",
                     static_cast<int>(req.target_state));
        return;
      }
      spdlog::info("State changed to {} from MMA command", m_currentState);
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