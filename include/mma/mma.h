#pragma once
/**
 * @file mma.h
 * @brief Declares the MMA server coordinator and operator command interface.
 */

#include <common/Packet.h>
#include <common/TcpConnection.h>

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

class WarrantyManager;

/**
 * @brief Main MMA server facade for handling aircraft clients and commands.
 */
class MMA {
public:
  /** @brief Constructs MMA server instance. */
  MMA();
  /** @brief Stops background resources and active client sessions. */
  ~MMA();
  /** @brief Initializes MMA internal components. */
  void initialize();
  /** @brief Starts the TCP listener on the requested port. */
  void startServer(uint16_t port = 8000);
  /** @brief Stops listener, worker thread, and active connections. */
  void stopServer();
  /** @brief Runs interactive console menu operations. */
  void runMenu();
  /** @brief Sends a state change request to put an aircraft into diagnostic mode. */
  void sendDiagnosticStateChange(uint64_t aircraftId);
  /** @brief Sends a request to clear a specific aircraft diagnostic code. */
  void sendDiagnosticCodeClearRequest(uint64_t aircraftId, int32_t code);
  /** @brief Returns true while the MMA server is running. */
  bool getRunningStatus() const { return running_; }
  /** @brief Gets the currently configured listener port. */
  uint16_t getListeningPort() const;

private:
  struct ReassemblyRetryState {
    std::chrono::steady_clock::time_point last_chunk_time;
    std::vector<uint8_t> retry_attempts_per_chunk;
  };

  void doAccept();
  void handleNewConnection(network::TcpConnection::Ptr conn);
  void processMessage(const std::vector<uint8_t>& data, network::TcpConnection::Ptr conn);
  void scheduleChunkTimeoutChecks();
  void processMissingChunkTimeouts();
  void sendChunkRetryRequest(uint64_t aircraftId, uint32_t imageId, uint16_t chunkIndex);
  void displayWarranty(uint64_t aircraftId);
  void printDiagnosticFaults(uint64_t aircraftId,
                             const std::vector<network::DiagnosticFaultCode>& faults) const;

  std::unique_ptr<asio::io_context> io_context_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::vector<network::TcpConnection::Ptr> connections_;
  std::unordered_map<uint64_t, network::TcpConnection::Ptr> verified_connections_;
  std::unordered_map<network::TcpConnection*, uint64_t> connection_to_id_;
  std::unordered_map<uint64_t, network::StateId> aircraft_states_;
  uint32_t expected_challenge_ = 0;
  bool running_ = false;
  std::unique_ptr<asio::steady_timer> chunk_timeout_timer_;
  std::thread io_thread_;
  std::unique_ptr<WarrantyManager> warrantyManager_;
  std::atomic<bool> menuRunning_{true};
  // Per aircraft, per image: aircraft_id -> (image_id -> ImageBuffer)
  std::unordered_map<uint64_t, std::map<uint32_t, network::ImageBuffer>> image_reassembly_buffers_;
  std::unordered_map<uint64_t, std::map<uint32_t, ReassemblyRetryState>>
      image_reassembly_retry_state_;
};