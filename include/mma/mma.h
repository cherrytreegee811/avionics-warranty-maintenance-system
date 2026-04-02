#pragma once

#include <common/TcpConnection.h>

#include <asio.hpp>
#include <memory>
#include <random>
#include <vector>

class MMA {
public:
  MMA();
  void initialize();
  void startServer(uint16_t port = 8000);
  void stopServer();

private:
  void doAccept();
  void handleNewConnection(network::TcpConnection::Ptr conn);
  void processMessage(const std::vector<uint8_t>& data, network::TcpConnection::Ptr conn);

  std::unique_ptr<asio::io_context> io_context_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::vector<network::TcpConnection::Ptr> connections_;
  uint32_t expected_challenge_ = 0;
  bool running_ = false;
  std::thread io_thread_;
};