// socket_client_base.hpp
/*
 * socket_client_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-29

Description: Template base class for socket client operations (TCP/SSL unified)

**************************************************/

#ifndef ATOM_CONNECTION_SOCKET_CLIENT_BASE_HPP
#define ATOM_CONNECTION_SOCKET_CLIENT_BASE_HPP

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "socket_types.hpp"

namespace atom::connection {

/**
 * @brief Type traits for socket types
 */
template <typename Socket>
struct SocketTraits {
    static constexpr bool is_ssl = false;
};

template <>
struct SocketTraits<asio::ssl::stream<asio::ip::tcp::socket>> {
    static constexpr bool is_ssl = true;
};

/**
 * @class SocketClientBase
 * @brief Template base class for TCP and SSL socket clients
 *
 * This class provides unified read/write operations for both TCP and SSL
 * sockets, eliminating code duplication between the two implementations.
 *
 * @tparam Socket The socket type (tcp::socket or ssl::stream<tcp::socket>)
 */
template <typename Socket>
class SocketClientBase : public std::enable_shared_from_this<SocketClientBase<Socket>> {
public:
    using MessageHandler = std::function<void(const Message&)>;
    using DisconnectHandler = std::function<void()>;
    using SendCallback = std::function<void(bool success)>;

    /**
     * @brief Construct a socket client
     * @param id Unique client identifier
     * @param socket The socket object
     */
    SocketClientBase(size_t id, std::shared_ptr<Socket> socket)
        : id_(id),
          socket_(std::move(socket)),
          is_authenticated_(false),
          connect_time_(std::chrono::system_clock::now()),
          last_activity_time_(connect_time_),
          messages_sent_(0),
          messages_received_(0),
          bytes_sent_(0),
          bytes_received_(0) {}

    virtual ~SocketClientBase() = default;

    // Non-copyable
    SocketClientBase(const SocketClientBase&) = delete;
    SocketClientBase& operator=(const SocketClientBase&) = delete;

    /**
     * @brief Get client ID
     */
    [[nodiscard]] size_t getId() const noexcept { return id_; }

    /**
     * @brief Check if client is authenticated
     */
    [[nodiscard]] bool isAuthenticated() const noexcept {
        return is_authenticated_.load(std::memory_order_acquire);
    }

    /**
     * @brief Set authentication status
     */
    void setAuthenticated(bool auth) noexcept {
        is_authenticated_.store(auth, std::memory_order_release);
    }

    /**
     * @brief Set client metadata
     */
    void setMetadata(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        metadata_[key] = value;
    }

    /**
     * @brief Get client metadata
     */
    [[nodiscard]] std::string getMetadata(const std::string& key) const {
        std::lock_guard<std::mutex> lock(metadata_mutex_);
        auto it = metadata_.find(key);
        return it != metadata_.end() ? it->second : "";
    }

    /**
     * @brief Get remote address
     */
    [[nodiscard]] std::string getRemoteAddress() const {
        try {
            if constexpr (SocketTraits<Socket>::is_ssl) {
                return socket_->lowest_layer()
                    .remote_endpoint()
                    .address()
                    .to_string();
            } else {
                return socket_->remote_endpoint().address().to_string();
            }
        } catch (const std::exception&) {
            return "unknown";
        }
    }

    /**
     * @brief Get connection time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getConnectTime()
        const noexcept {
        return connect_time_;
    }

    /**
     * @brief Get last activity time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getLastActivityTime()
        const noexcept {
        return last_activity_time_.load(std::memory_order_acquire);
    }

    /**
     * @brief Update last activity time
     */
    void updateLastActivity() noexcept {
        last_activity_time_.store(std::chrono::system_clock::now(),
                                  std::memory_order_release);
    }

    /**
     * @brief Send a message
     */
    void send(const Message& message, SendCallback callback = nullptr) {
        bytes_sent_.fetch_add(message.data.size(), std::memory_order_relaxed);
        messages_sent_.fetch_add(1, std::memory_order_relaxed);
        updateLastActivity();

        asio::async_write(
            *socket_, asio::buffer(message.data),
            [callback](std::error_code ec, std::size_t) {
                if (callback) {
                    callback(!ec);
                }
            });
    }

    /**
     * @brief Start reading from socket
     */
    void startReading(MessageHandler message_handler,
                      DisconnectHandler disconnect_handler) {
        message_handler_ = std::move(message_handler);
        disconnect_handler_ = std::move(disconnect_handler);
        doRead();
    }

    /**
     * @brief Disconnect the client
     */
    void disconnect() {
        try {
            if constexpr (SocketTraits<Socket>::is_ssl) {
                socket_->lowest_layer().close();
            } else {
                socket_->close();
            }
        } catch (const std::exception&) {
            // Already closed or other error
        }
    }

    // Statistics getters
    [[nodiscard]] size_t getMessagesSent() const noexcept {
        return messages_sent_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t getMessagesReceived() const noexcept {
        return messages_received_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t getBytesSent() const noexcept {
        return bytes_sent_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] size_t getBytesReceived() const noexcept {
        return bytes_received_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get client info structure
     */
    [[nodiscard]] ClientInfo getClientInfo() const {
        return ClientInfo{
            .id = static_cast<int>(id_),
            .address = getRemoteAddress(),
            .connected_time = std::chrono::steady_clock::now(),  // Approximate
            .bytes_received = getBytesReceived(),
            .bytes_sent = getBytesSent(),
            .messages_received = getMessagesReceived(),
            .messages_sent = getMessagesSent(),
            .is_authenticated = isAuthenticated()};
    }

protected:
    /**
     * @brief Perform async read operation
     */
    void doRead() {
        auto buffer = std::make_shared<std::vector<char>>(read_buffer_size_);
        auto self = this->shared_from_this();

        socket_->async_read_some(
            asio::buffer(*buffer),
            [this, self, buffer](std::error_code ec, std::size_t length) {
                if (!ec) {
                    bytes_received_.fetch_add(length,
                                              std::memory_order_relaxed);
                    messages_received_.fetch_add(1, std::memory_order_relaxed);
                    updateLastActivity();

                    Message msg;
                    msg.type = Message::Type::TEXT;
                    msg.data.assign(buffer->begin(), buffer->begin() + length);
                    msg.sender_id = id_;

                    if (message_handler_) {
                        message_handler_(msg);
                    }

                    doRead();
                } else {
                    if (disconnect_handler_) {
                        disconnect_handler_();
                    }
                }
            });
    }

    size_t id_;
    std::shared_ptr<Socket> socket_;
    std::atomic<bool> is_authenticated_;
    MessageHandler message_handler_;
    DisconnectHandler disconnect_handler_;
    std::chrono::system_clock::time_point connect_time_;
    std::atomic<std::chrono::system_clock::time_point> last_activity_time_;
    std::atomic<size_t> messages_sent_;
    std::atomic<size_t> messages_received_;
    std::atomic<size_t> bytes_sent_;
    std::atomic<size_t> bytes_received_;
    std::unordered_map<std::string, std::string> metadata_;
    mutable std::mutex metadata_mutex_;
    static constexpr size_t read_buffer_size_ = 4096;
};

// Type aliases for convenience
using TcpClient = SocketClientBase<asio::ip::tcp::socket>;
using SslClient = SocketClientBase<asio::ssl::stream<asio::ip::tcp::socket>>;

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SOCKET_CLIENT_BASE_HPP
