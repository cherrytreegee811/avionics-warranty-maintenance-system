#pragma once

#include <asio.hpp>
#include <cstdint>
#include <string>

namespace test_helpers {

  class MockAircraft {
  public:
    MockAircraft(const std::string& host, uint16_t port, uint64_t client_id);
    ~MockAircraft();

    void runVerificationAndSendLanded();

  private:
    uint64_t client_id_;
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
  };

}  // namespace test_helpers