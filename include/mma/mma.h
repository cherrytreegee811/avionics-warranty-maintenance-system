#pragma once

#include <common/Packet.h>
#include <common/TcpConnection.h>

#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

class WarrantyManager;

class MMA {
public:
  MMA();
  ~MMA();
  void initialize();
  void startServer(uint16_t port = 8000);
  void stopServer();
  void runMenu();
  void sendDiagnosticStateChange(uint64_t aircraftId);
  void sendDiagnosticCodeClearRequest(uint64_t aircraftId, int32_t code);
  bool getRunningStatus() const { return running_; }

private:
  void doAccept();
  void handleNewConnection(network::TcpConnection::Ptr conn);
  void processMessage(const std::vector<uint8_t>& data, network::TcpConnection::Ptr conn);
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
  std::thread io_thread_;
  std::unique_ptr<WarrantyManager> warrantyManager_;
  std::atomic<bool> menuRunning_{true};
  // Per aircraft, per image: aircraft_id -> (image_id -> ImageBuffer)
  std::unordered_map<uint64_t, std::map<uint32_t, network::ImageBuffer>> image_reassembly_buffers_;
};