#pragma once

#include <common/TcpConnection.h>

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <thread>

namespace test_helpers {

  class MockMMA {
  public:
    explicit MockMMA(uint16_t port);
    ~MockMMA();

    bool hasReceivedLanded() const;

  private:
    void accept();
    void onMessage(const std::vector<uint8_t>& data);
    void sendVerificationRequest();

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread io_thread_;
    std::atomic<bool> received_landed_{false};
    std::atomic<bool> verified_{false};
    network::TcpConnection::Ptr connection_;
    uint32_t challenge_ = 0x12345678;
  };

}  // namespace test_helpers