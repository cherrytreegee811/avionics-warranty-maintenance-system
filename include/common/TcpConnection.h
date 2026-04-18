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
      struct MakeSharedEnabler final : TcpConnection {
        explicit MakeSharedEnabler(asio::ip::tcp::socket s) : TcpConnection(std::move(s)) {}
      };
      return std::make_shared<MakeSharedEnabler>(std::move(socket));
    }

    /** @brief Starts async receive loop on the underlying socket. */
    void start() { readData(); }

    /**
     * @brief Queues bytes for async send on the connection executor.
     * @param data Type: const std::vector<uint8_t>&. Serialized packet bytes to send.
     */
    void send(const std::vector<uint8_t>& data) {
      auto self = shared_from_this();
      (void)asio::post(socket_.get_executor(), [self, data]() {
        const bool is_closed
            = (self->state_.load() == ConnectionState::CLOSED) || (!self->socket_.is_open());
        if (!is_closed) {
          self->outgoing_queue_.push_back(std::make_shared<std::vector<uint8_t>>(data));
          self->startNextWrite();
        }
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
      (void)asio::post(socket_.get_executor(), [self = shared_from_this()]() {
        if (self->state_.load() != ConnectionState::CLOSED) {
          std::error_code ignored;
          self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
          self->socket_.close(ignored);
          self->markClosed();
        }
      });
    }

    /**
     * @brief Returns remote peer IP address string when available.
     * @return Type: std::string. Remote address string or "unknown" when unavailable.
     */
    std::string getRemoteAddress() const {
      std::string result = "unknown";
      std::error_code ec;
      {
        const auto endpoint = socket_.remote_endpoint(ec);
        if (!ec) {
          result = endpoint.address().to_string();
        }
      }
      return result;
    }

  private:
    TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void markClosed() { state_.store(ConnectionState::CLOSED); }

    void startNextWrite() {
      const bool can_start
          = (!write_in_progress_) && (!outgoing_queue_.empty()) && (socket_.is_open());
      if (can_start) {
        write_in_progress_ = true;
        auto payload = outgoing_queue_.front();
        auto self = shared_from_this();
        (void)asio::async_write(
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
              } else {
                self->outgoing_queue_.pop_front();
                self->startNextWrite();
              }
            });
      }
    }

    void processIncomingBuffer() {
      bool done = false;
      while (!done) {
        if (incoming_buffer_.size() < sizeof(PacketHeader)) {
          done = true;
        } else {
          PacketHeader header{};
          (void)std::memcpy(&header, incoming_buffer_.data(), sizeof(PacketHeader));

          if (header.magic != PACKET_MAGIC) {
            spdlog::warn("Invalid packet magic from {}. Closing connection.", getRemoteAddress());
            close();
            done = true;
          } else {
            const size_t packet_size = sizeof(PacketHeader) + header.payload_size;
            if (packet_size > kMaxPacketSizeBytes) {
              spdlog::warn("Packet too large ({} bytes) from {}. Closing connection.", packet_size,
                           getRemoteAddress());
              close();
              done = true;
            } else if (incoming_buffer_.size() < packet_size) {
              done = true;
            } else {
              std::vector<uint8_t> packet(packet_size);
              (void)std::memcpy(packet.data(), incoming_buffer_.data(), packet_size);
              (void)incoming_buffer_.erase(incoming_buffer_.begin(),
                                           incoming_buffer_.begin() + packet_size);

              if (handler_) {
                handler_(packet);
              }
            }
          }
        }
      }
    }

    void readData() {
      auto self = shared_from_this();

      auto read_handler = std::make_shared<std::function<void(std::error_code, size_t)>>();
      *read_handler = [self, read_handler](std::error_code ec, size_t length) {
        if (ec) {
          if (ec == asio::error::eof || ec == asio::error::connection_reset
              || ec == asio::error::operation_aborted) {
            spdlog::info("Connection closed by peer: {}", self->getRemoteAddress());
          } else {
            spdlog::error("Read error: {}", ec.message());
          }
          self->markClosed();
        } else {
          (void)self->incoming_buffer_.insert(self->incoming_buffer_.end(), self->buffer_.begin(),
                                              self->buffer_.begin() + length);
          self->processIncomingBuffer();

          if (self->state_.load() != ConnectionState::CLOSED && self->socket_.is_open()) {
            (void)self->socket_.async_read_some(asio::buffer(self->buffer_), *read_handler);
          }
        }
      };

      (void)socket_.async_read_some(asio::buffer(buffer_), *read_handler);
    }

    static constexpr size_t kMaxPacketSizeBytes = 50U * 1024U * 1024U;
    asio::ip::tcp::socket socket_;
    std::array<uint8_t, 4096> buffer_;
    std::vector<uint8_t> incoming_buffer_;
    std::deque<std::shared_ptr<std::vector<uint8_t>>> outgoing_queue_;
    bool write_in_progress_ = false;
    MessageHandler handler_;
    std::atomic<ConnectionState> state_{ConnectionState::UNVERIFIED};
  };

}  // namespace network