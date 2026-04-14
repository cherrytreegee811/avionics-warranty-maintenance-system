#pragma once
/**
 * @file Aircraft.h
 * @brief Declares the aircraft domain model and client-side MMA communication API.
 */

#include <common/Packet.h>
#include <common/TcpConnection.h>
#include <common/WarrantyData.h>

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class StateManager;

namespace aircraft {

  /**
   * @brief Describes the origin of a state transition request.
   */
  enum class TransitionSource {
    MMA_COMMAND,
    AUTOMATIC,
    MANUAL,
    CONNECTION_FALLBACK,
  };

  /**
   * @brief Represents one active aircraft fault entry.
   */
  struct FaultCode {
    int code;
    network::DiagnosticFaultSeverity severity;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
  };

  /**
   * @brief Alias for shared warranty metadata.
   */
  using WarrantyInfo = common::WarrantyInfo;

  /**
   * @brief Captures the latest maintenance record for the aircraft.
   */
  struct MaintenanceInfo {
    std::chrono::system_clock::time_point lastMaintenance;
    std::string technician;
    std::string notes;
  };

  /**
   * @brief Main aircraft aggregate used by the client simulator.
   */
  class Aircraft {
  public:
    /** @brief Constructs an aircraft instance with default state. */
    Aircraft();
    /** @brief Shuts down network resources and worker threads. */
    ~Aircraft();
    /** @brief Initializes startup state and internal managers. */
    void initialize();

    /**
     * @brief Gets the current state name.
    * @return Type: std::string. Current state label.
     */
    std::string getCurrentState() const;
    /**
     * @brief Sets the current state name.
    * @param state Type: const std::string&. New state label.
     */
    void setCurrentState(const std::string& state);

    /**
     * @brief Gets the last maintenance record.
    * @return Type: @ref aircraft::MaintenanceInfo. Latest maintenance details snapshot.
     */
    MaintenanceInfo getLastMaintenance() const;
    /**
     * @brief Gets all active fault codes.
    * @return Type: std::vector<@ref aircraft::FaultCode>. Copy of active fault records.
     */
    std::vector<FaultCode> getFaultCodes() const;
    /**
     * @brief Gets current warranty information.
    * @return Type: @ref aircraft::WarrantyInfo. Current warranty metadata.
     */
    WarrantyInfo getWarranty() const;

    /**
     * @brief Updates the last maintenance record.
    * @param info Type: const @ref aircraft::MaintenanceInfo&. New maintenance details.
     */
    void setLastMaintenance(const MaintenanceInfo& info);
    /**
     * @brief Adds a fault code to the active fault list.
    * @param code Type: const @ref aircraft::FaultCode&. Fault entry to append.
     */
    void addFaultCode(const FaultCode& code);
    /**
     * @brief Attempts to resolve and remove a fault code by numeric code.
    * @param code Type: int. Numeric fault code to remove.
    * @return Type: bool. True if a matching fault code was removed.
     */
    bool resolveFaultCode(int code);
    /** @brief Clears all tracked fault codes. */
    void clearFaultCodes();
    /**
     * @brief Updates warranty information.
    * @param info Type: const @ref aircraft::WarrantyInfo&. New warranty metadata.
     */
    void setWarranty(const WarrantyInfo& info);
    /**
     * @brief Reports whether verification with MMA has completed.
    * @return Type: bool. True when connection verification is complete.
     */
    bool getRunningStatus() const { return verified_; }
    /**
     * @brief Gets this aircraft unique identifier.
    * @return Type: uint64_t. Aircraft identifier value.
     */
    uint64_t getAircraftId() const { return aircraft_id_; }

    /** @brief Session token used by external flows. */
    int token = 0;

    /**
     * @brief Connects to the MMA server endpoint.
    * @param host Type: const std::string&. Hostname or address of MMA server.
    * @param port Type: uint16_t. Listener port for MMA server.
     */
    void connectToMMA(const std::string& host, uint16_t port = 8000);
    /** @brief Sends a landed notification packet to MMA. */
    void sendLandedNotification();
    /**
     * @brief Handles a raw network packet delivered from the connection.
    * @param data Type: const std::vector<uint8_t>&. Serialized packet bytes.
     */
    void onNetworkMessage(const std::vector<uint8_t>& data);
    /**
     * @brief Binds the state manager used by state objects.
    * @param stateManager Type: @ref StateManager*. State manager instance owned externally.
     */
    void setStateManager(StateManager* stateManager);
    /** @brief Synchronizes the state manager to the current persisted state. */
    void syncStateManagerToCurrentState();
    /**
     * @brief Requests transition to a target state.
    * @param targetState Type: @ref network::StateId. Requested destination state.
    * @param source Type: @ref aircraft::TransitionSource. Origin of the transition request.
    * @return Type: bool. True when transition was accepted and applied.
     */
    bool transitionToState(network::StateId targetState,
                           TransitionSource source = TransitionSource::AUTOMATIC);
    /**
     * @brief Sends diagnostic fault payload to MMA.
    * @return Type: bool. True if payload was serialized and queued for send.
     */
    bool sendDiagnosticData();
    /**
     * @brief Sends warranty payload to MMA.
    * @return Type: bool. True if payload was serialized and queued for send.
     */
    bool sendWarrantyData();
    /**
     * @brief Returns whether diagnostic-stage payload can be transmitted.
    * @return Type: bool. True when current state permits diagnostic-stage transfer.
     */
    bool canSendDiagnosticStageData() const;
    /** @brief Marks that MMA explicitly requested diagnostic data. */
    void markDiagnosticRequestedByMMA();

    /**
     * @brief Loads and sends an image file as chunked packets.
    * @param filepath Type: const std::string&. Path to image file.
    * @return Type: bool. True when file was read and all chunks were queued.
     */
    bool sendImageFromFile(const std::string& filepath);
    /**
     * @brief Sends image bytes using chunked transport.
    * @param image_data Type: const std::vector<uint8_t>&. Raw encoded image bytes.
    * @param format Type: @ref network::ImageFormat. Encoding format of image_data.
    * @return Type: bool. True when chunks were generated and queued.
     */
    bool sendImage(const std::vector<uint8_t>& image_data, network::ImageFormat format);

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
    uint64_t aircraft_id_;
    std::atomic<bool> shutting_down_{false};
    bool automatic_transition_in_progress_ = false;
    std::map<uint32_t, network::ImageBuffer> image_reassembly_buffers_;  // image_id -> ImageBuffer
    std::map<uint32_t, std::vector<std::vector<uint8_t>>> sent_image_chunk_payloads_;
    std::deque<uint32_t> sent_image_cache_order_;
    static constexpr size_t kMaxCachedImagesForRetry = 8;
    uint32_t next_image_id_ = 1;
  };

}  // namespace aircraft