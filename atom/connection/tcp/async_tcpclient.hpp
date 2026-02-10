/*
 * async_tcpclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: Async TCP Client Class with ASIO backend

*************************************************/

#ifndef ATOM_CONNECTION_ASYNC_TCPCLIENT_HPP
#define ATOM_CONNECTION_ASYNC_TCPCLIENT_HPP

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "tcp_common.hpp"

namespace atom::async::connection {

// Re-export common types for backward compatibility
using atom::connection::BackoffCalculator;
using atom::connection::ConnectionState;
using atom::connection::ConnectionStats;
using atom::connection::ProxyConfig;
using atom::connection::TcpClientConfig;

// Alias for backward compatibility
using ConnectionConfig = TcpClientConfig;

/**
 * @brief Async TCP Client using ASIO backend
 *
 * Features:
 * - SSL/TLS support
 * - Automatic reconnection with exponential backoff
 * - Heartbeat mechanism
 * - Connection statistics
 * - Thread-safe operations
 */
class TcpClient {
public:
    using OnConnectedCallback = std::function<void()>;
    using OnConnectingCallback = std::function<void()>;
    using OnDisconnectedCallback = std::function<void()>;
    using OnDataReceivedCallback =
        std::function<void(const std::vector<char>&)>;
    using OnErrorCallback = std::function<void(const std::string&)>;
    using OnStateChangedCallback =
        std::function<void(ConnectionState, ConnectionState)>;
    using OnHeartbeatCallback = std::function<void()>;

    explicit TcpClient(const TcpClientConfig& config = TcpClientConfig{});
    ~TcpClient();

    // Non-copyable, movable
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) noexcept;
    TcpClient& operator=(TcpClient&&) noexcept;

    /**
     * @brief Connect to a server synchronously
     */
    bool connect(
        const std::string& host, int port,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Connect to a server asynchronously
     * @return Future that resolves when connection completes
     */
    std::future<bool> connectAsync(const std::string& host, int port);

    /**
     * @brief Disconnect from the server
     */
    void disconnect();

    /**
     * @brief Configure reconnection behavior
     */
    void configureReconnection(
        int attempts,
        std::chrono::milliseconds delay = std::chrono::seconds(1));

    /**
     * @brief Set the heartbeat interval
     */
    void setHeartbeatInterval(std::chrono::milliseconds interval,
                              const std::vector<char>& data = {});

    /**
     * @brief Send raw data to the server
     */
    bool send(std::span<const char> data);

    /**
     * @brief Send string data to the server
     */
    bool sendString(std::string_view data);

    /**
     * @brief Send data with timeout
     */
    bool sendWithTimeout(std::span<const char> data,
                         std::chrono::milliseconds timeout);

    /**
     * @brief Receive specific amount of data asynchronously
     */
    std::future<std::vector<char>> receive(
        size_t size,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Receive data until delimiter is found
     */
    std::future<std::string> receiveUntil(
        char delimiter,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Perform a request-response cycle
     */
    std::future<std::vector<char>> requestResponse(
        std::span<const char> request, size_t response_size,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Set proxy configuration
     */
    void setProxyConfig(const ProxyConfig& config);

    /**
     * @brief Configure SSL certificates
     */
    void configureSslCertificates(const std::string& cert_path,
                                  const std::string& key_path,
                                  const std::string& ca_path);

    // Getters
    [[nodiscard]] ConnectionState getConnectionState() const;
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] std::string getErrorMessage() const;
    [[nodiscard]] ConnectionStats getStats() const;
    [[nodiscard]] std::string getRemoteAddress() const;
    [[nodiscard]] int getRemotePort() const;
    [[nodiscard]] std::string getProperty(const std::string& key) const;

    void resetStats();
    void setProperty(const std::string& key, const std::string& value);

    // Callback setters
    void setOnConnectingCallback(OnConnectingCallback callback);
    void setOnConnectedCallback(OnConnectedCallback callback);
    void setOnDisconnectedCallback(OnDisconnectedCallback callback);
    void setOnDataReceivedCallback(OnDataReceivedCallback callback);
    void setOnErrorCallback(OnErrorCallback callback);
    void setOnStateChangedCallback(OnStateChangedCallback callback);
    void setOnHeartbeatCallback(OnHeartbeatCallback callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::async::connection

#endif  // ATOM_CONNECTION_ASYNC_TCPCLIENT_HPP
