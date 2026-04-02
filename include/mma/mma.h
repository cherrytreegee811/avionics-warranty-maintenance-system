#pragma once

#include <common/TcpConnection.h>

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <random>
#include <thread>
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

private:
  void doAccept();
  void handleNewConnection(network::TcpConnection::Ptr conn);
  void processMessage(const std::vector<uint8_t>& data, network::TcpConnection::Ptr conn);
  void displayWarranty(uint64_t aircraftId);

  std::unique_ptr<asio::io_context> io_context_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::vector<network::TcpConnection::Ptr> connections_;
  uint32_t expected_challenge_ = 0;
  bool running_ = false;
  std::thread io_thread_;
  std::unique_ptr<WarrantyManager> warrantyManager_;
  std::atomic<bool> menuRunning_{true};
};