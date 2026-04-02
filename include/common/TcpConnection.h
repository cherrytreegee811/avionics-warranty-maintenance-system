#pragma once

#include <spdlog/spdlog.h>

#include <array>
#include <asio.hpp>
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
        asio::async_write(self->socket_, asio::buffer(data), [self](std::error_code ec, size_t) {
          if (ec) spdlog::error("Send error: {}", ec.message());
        });
      });
    }

    void setMessageHandler(MessageHandler handler) { handler_ = handler; }
    void setState(ConnectionState state) { state_ = state; }
    ConnectionState getState() const { return state_; }

    void close() {
      asio::post(socket_.get_executor(), [self = shared_from_this()]() { self->socket_.close(); });
    }

    std::string getRemoteAddress() const {
      std::error_code ec;
      auto endpoint = socket_.remote_endpoint(ec);
      if (ec) return "unknown";
      return endpoint.address().to_string();
    }

  private:
    TcpConnection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void readData() {
      auto self = shared_from_this();
      socket_.async_read_some(asio::buffer(buffer_), [self](std::error_code ec, size_t length) {
        if (ec) {
          if (ec == asio::error::eof) {
            spdlog::info("Connection closed by peer: {}", self->getRemoteAddress());
          } else {
            spdlog::error("Read error: {}", ec.message());
          }
          return;
        }
        std::vector<uint8_t> data(self->buffer_.begin(), self->buffer_.begin() + length);
        if (self->handler_) self->handler_(data);
        self->readData();  // continue reading
      });
    }

    asio::ip::tcp::socket socket_;
    std::array<uint8_t, 4096> buffer_;
    MessageHandler handler_;
    ConnectionState state_ = ConnectionState::UNVERIFIED;
  };

}  // namespace network