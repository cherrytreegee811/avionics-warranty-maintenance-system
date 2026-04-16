#pragma once
/**
 * @file TcpConnection.h
 * @brief Declares asynchronous TCP connection wrapper for packetized transport.
 */

#include <common/Packet.h>
#include <spdlog/spdlog.h>

#include <array>
#include <asio.hpp>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace network {

  /** @brief Lifecycle state of a network connection session. */
  enum class ConnectionState { UNVERIFIED, VERIFIED, CLOSED };

  /**
   * @brief Async TCP connection with framed packet buffering and queued writes.
   */
  class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
  public:
    /** @brief Shared pointer alias for a connection instance. */
    using Ptr = std::shared_ptr<TcpConnection>;
    /** @brief Callback type for completed packet-sized messages. */
    using MessageHandler = std::function<void(const std::vector<uint8_t>&)>;

    /**
     * @brief Factory function for heap-allocated shared connection instances.
     * @param socket Type: asio::ip::tcp::socket. Connected socket to wrap.
     * @return Type: @ref network::TcpConnection::Ptr. Shared connection instance.
     */
    static Ptr create(asio::ip::tcp::socket socket) {
      return Ptr(new TcpConnection(std::move(socket)));
    }

    /** @brief Starts async receive loop on the underlying socket. */
    void start() { readData(); }

    /**
     * @brief Queues bytes for async send on the connection executor.
     * @param data Type: const std::vector<uint8_t>&. Serialized packet bytes to send.
     */
    void send(const std::vector<uint8_t>& data) {
      auto self = shared_from_this();
      asio::post(socket_.get_executor(), [self, data]() {
        if (self->state_.load() == ConnectionState::CLOSED || !self->socket_.is_open()) {
          return;
        }
        self->outgoing_queue_.push_back(std::make_shared<std::vector<uint8_t>>(data));
        self->startNextWrite();
      });
    }

    /**
     * @brief Registers callback invoked for each complete incoming packet.
     * @param handler Type: @ref MessageHandler. Callback for complete packet payloads.
     */
    void setMessageHandler(MessageHandler handler) { handler_ = handler; }
    /**
     * @brief Updates connection verification state.
     * @param state Type: @ref network::ConnectionState. New connection lifecycle state.
     */
    void setState(ConnectionState state) { state_.store(state); }
    /**
     * @brief Returns current connection verification state.
     * @return Type: @ref network::ConnectionState. Current connection lifecycle state.
     */
    ConnectionState getState() const { return state_.load(); }

    /** @brief Closes socket and transitions state to closed. */
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

    /**
     * @brief Returns remote peer IP address string when available.
     * @return Type: std::string. Remote address string or "unknown" when unavailable.
     */
    std::string getRemoteAddress() const {
      std::error_code ec;
      auto endpoint = socket_.remote_endpoint(ec);
      if (ec) return "unknown";
      return endpoint.address().to_string();
    }

  private:
    TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void markClosed() { state_.store(ConnectionState::CLOSED); }

    void startNextWrite() {
      if (write_in_progress_ || outgoing_queue_.empty() || !socket_.is_open()) {
        return;
      }

      write_in_progress_ = true;
      auto payload = outgoing_queue_.front();
      auto self = shared_from_this();
      asio::async_write(
          socket_, asio::buffer(*payload), [self, payload](std::error_code ec, size_t) {
            self->write_in_progress_ = false;
            if (ec) {
              if (ec == asio::error::operation_aborted || ec == asio::error::connection_reset
                  || ec == asio::error::broken_pipe || ec == asio::error::not_connected
                  || ec == asio::error::eof) {
                spdlog::info("Connection send closed for {}: {}", self->getRemoteAddress(),
                             ec.message());
              } else {
                spdlog::error("Send error: {}", ec.message());
              }
              self->markClosed();
              self->outgoing_queue_.clear();
              return;
            }

            self->outgoing_queue_.pop_front();
            self->startNextWrite();
          });
    }

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

    static constexpr size_t kMaxPacketSizeBytes = 50 * 1024 * 1024;
    asio::ip::tcp::socket socket_;
    std::array<uint8_t, 4096> buffer_;
    std::vector<uint8_t> incoming_buffer_;
    std::deque<std::shared_ptr<std::vector<uint8_t>>> outgoing_queue_;
    bool write_in_progress_ = false;
    MessageHandler handler_;
    std::atomic<ConnectionState> state_{ConnectionState::UNVERIFIED};
  };

}  // namespace network