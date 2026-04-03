#pragma once

#include <common/Packet.h>
#include <spdlog/spdlog.h>

#include <array>
#include <asio.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace network {

  enum class ConnectionState { UNVERIFIED, VERIFIED, CLOSED };

  class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
  public:
    using Ptr = std::shared_ptr<TcpConnection>;
    using MessageHandler = std::function<void(const std::vector<uint8_t>&)>;

    static Ptr create(asio::ip::tcp::socket socket) {
      return Ptr(new TcpConnection(std::move(socket)));
    }

    void start() { readData(); }

    void send(const std::vector<uint8_t>& data) {
      auto self = shared_from_this();
      asio::post(socket_.get_executor(), [self, data]() {
        if (self->state_.load() == ConnectionState::CLOSED || !self->socket_.is_open()) {
          return;
        }
        asio::async_write(self->socket_, asio::buffer(data), [self](std::error_code ec, size_t) {
          if (!ec) {
            return;
          }

          if (ec == asio::error::operation_aborted || ec == asio::error::connection_reset
              || ec == asio::error::broken_pipe || ec == asio::error::not_connected
              || ec == asio::error::eof) {
            spdlog::info("Connection send closed for {}: {}", self->getRemoteAddress(),
                         ec.message());
          } else {
            spdlog::error("Send error: {}", ec.message());
          }
          self->markClosed();
        });
      });
    }

    void setMessageHandler(MessageHandler handler) { handler_ = handler; }
    void setState(ConnectionState state) { state_.store(state); }
    ConnectionState getState() const { return state_.load(); }

    void close() {
      asio::post(socket_.get_executor(), [self = shared_from_this()]() {
        if (self->state_.load() == ConnectionState::CLOSED) {
          return;
        }

        std::error_code ignored;
        self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        self->socket_.close(ignored);
        self->markClosed();
      });
    }

    std::string getRemoteAddress() const {
      std::error_code ec;
      auto endpoint = socket_.remote_endpoint(ec);
      if (ec) return "unknown";
      return endpoint.address().to_string();
    }

  private:
    TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void markClosed() { state_.store(ConnectionState::CLOSED); }

    void processIncomingBuffer() {
      while (true) {
        if (incoming_buffer_.size() < sizeof(PacketHeader)) {
          return;
        }

        PacketHeader header{};
        std::memcpy(&header, incoming_buffer_.data(), sizeof(PacketHeader));

        if (header.magic != PACKET_MAGIC) {
          spdlog::warn("Invalid packet magic from {}. Closing connection.", getRemoteAddress());
          close();
          return;
        }

        const size_t packet_size = sizeof(PacketHeader) + header.payload_size;
        if (packet_size > kMaxPacketSizeBytes) {
          spdlog::warn("Packet too large ({} bytes) from {}. Closing connection.", packet_size,
                       getRemoteAddress());
          close();
          return;
        }

        if (incoming_buffer_.size() < packet_size) {
          return;
        }

        std::vector<uint8_t> packet(packet_size);
        std::memcpy(packet.data(), incoming_buffer_.data(), packet_size);
        incoming_buffer_.erase(incoming_buffer_.begin(), incoming_buffer_.begin() + packet_size);

        if (handler_) {
          handler_(packet);
        }
      }
    }

    void readData() {
      auto self = shared_from_this();
      socket_.async_read_some(asio::buffer(buffer_), [self](std::error_code ec, size_t length) {
        if (ec) {
          if (ec == asio::error::eof || ec == asio::error::connection_reset
              || ec == asio::error::operation_aborted) {
            spdlog::info("Connection closed by peer: {}", self->getRemoteAddress());
          } else {
            spdlog::error("Read error: {}", ec.message());
          }
          self->markClosed();
          return;
        }

        self->incoming_buffer_.insert(self->incoming_buffer_.end(), self->buffer_.begin(),
                                      self->buffer_.begin() + length);
        self->processIncomingBuffer();
        self->readData();  // continue reading
      });
    }

    static constexpr size_t kMaxPacketSizeBytes = 1024 * 1024;
    asio::ip::tcp::socket socket_;
    std::array<uint8_t, 4096> buffer_;
    std::vector<uint8_t> incoming_buffer_;
    MessageHandler handler_;
    std::atomic<ConnectionState> state_{ConnectionState::UNVERIFIED};
  };

}  // namespace network