#pragma once

#include <common/Packet.h>
#include <common/TcpConnection.h>

#include <asio.hpp>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace test_helpers {

  class MockMMA {
  public:
    explicit MockMMA(uint16_t port);
    ~MockMMA();

    bool hasReceivedLanded() const;
    bool isVerified() const;
    bool sendStateChange(network::StateId targetState);
    bool sendChunkRetryRequest(uint32_t imageId, uint16_t chunkIndex);
    bool sendSchematicChunkPayload(const std::vector<uint8_t>& chunkPayload);
    bool hasConfirmationForState(network::StateId state) const;
    size_t stateChangeConfirmationCount() const;
    bool hasReceivedDiagnosticData() const;
    size_t receivedDiagnosticFaultCount() const;
    bool hasReceivedWarrantyData() const;
    size_t receivedSchematicChunkCount() const;
    std::vector<std::vector<uint8_t>> receivedSchematicChunkPayloads() const;
    void closeClientConnection();

  private:
    void accept();
    void onMessage(const std::vector<uint8_t>& data);
    void sendVerificationRequest();

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread io_thread_;
    std::atomic<bool> received_landed_{false};
    std::atomic<bool> verified_{false};
    std::atomic<bool> received_diagnostic_data_{false};
    std::atomic<bool> received_warranty_data_{false};
    std::atomic<size_t> confirmation_count_{0};
    std::atomic<size_t> diagnostic_fault_count_{0};
    std::atomic<size_t> schematic_chunk_count_{0};
    network::TcpConnection::Ptr connection_;
    uint32_t challenge_ = 0x12345678;
    mutable std::mutex confirmations_mutex_;
    std::vector<network::StateId> applied_confirmations_;
    mutable std::mutex schematic_chunks_mutex_;
    std::vector<std::vector<uint8_t>> received_schematic_chunk_payloads_;
  };

}  // namespace test_helpers
