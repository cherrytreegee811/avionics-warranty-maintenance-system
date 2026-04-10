#pragma once
#include <common/Packet.h>
#include <common/TcpConnection.h>
#include <common/WarrantyData.h>

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class StateManager;

namespace aircraft {

  enum class TransitionSource {
    MMA_COMMAND,
    AUTOMATIC,
    MANUAL,
    CONNECTION_FALLBACK,
  };

  struct FaultCode {
    int code;
    network::DiagnosticFaultSeverity severity;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
  };

  using WarrantyInfo = common::WarrantyInfo;

  struct MaintenanceInfo {
    std::chrono::system_clock::time_point lastMaintenance;
    std::string technician;
    std::string notes;
  };

  class Aircraft {
  public:
    Aircraft();
    ~Aircraft();
    void initialize();

    // State getters/setters
    std::string getCurrentState() const;
    void setCurrentState(const std::string& state);

    // Data getters
    MaintenanceInfo getLastMaintenance() const;
    std::vector<FaultCode> getFaultCodes() const;
    WarrantyInfo getWarranty() const;

    // Data setters
    void setLastMaintenance(const MaintenanceInfo& info);
    void addFaultCode(const FaultCode& code);
    bool resolveFaultCode(int code);
    void clearFaultCodes();
    void setWarranty(const WarrantyInfo& info);
    bool getRunningStatus() const { return verified_; }

    int token = 0;

    void connectToMMA(const std::string& host, uint16_t port = 8000);
    void sendLandedNotification();
    void onNetworkMessage(const std::vector<uint8_t>& data);
    void setStateManager(StateManager* stateManager);
    void syncStateManagerToCurrentState();
    bool transitionToState(network::StateId targetState,
                           TransitionSource source = TransitionSource::AUTOMATIC);
    bool sendDiagnosticData();
    bool sendWarrantyData();
    bool canSendDiagnosticStageData() const;
    void markDiagnosticRequestedByMMA();

  private:
    bool hasAnyFaults() const;
    bool hasMajorFaults() const;
    bool hasOnlyMinorFaults() const;
    void evaluateAutomaticTransitionFromCurrentState();

    using NetworkWorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    std::string m_currentState;
    MaintenanceInfo m_lastMaintenance;
    std::vector<FaultCode> m_faultCodes;
    WarrantyInfo m_warranty;
    std::shared_ptr<network::TcpConnection> connection_;
    std::unique_ptr<asio::io_context> network_io_context_;
    std::unique_ptr<NetworkWorkGuard> network_work_guard_;
    std::thread network_thread_;
    StateManager* stateManager_ = nullptr;
    bool verified_ = false;
    uint64_t aircraft_id_ = 12345;
    std::atomic<bool> shutting_down_{false};
    bool automatic_transition_in_progress_ = false;
    bool landed_notification_sent_ = false;
    bool diagnostic_requested_by_mma_ = false;
  };

}  // namespace aircraft