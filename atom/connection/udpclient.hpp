/*
 * udpclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: UDP Client Class

*************************************************/

#ifndef ATOM_CONNECTION_UDPCLIENT_HPP
#define ATOM_CONNECTION_UDPCLIENT_HPP

#include <chrono>
#include <coroutine>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"

namespace atom::connection {

/**
 * @brief Error codes for UDP operations
 */
enum class UdpError {
    None,
    SocketCreationFailed,
    BindFailed,
    SendFailed,
    ReceiveFailed,
    HostNotFound,
    Timeout,
    InvalidParameter,
    InternalError
};

/**
 * @brief UDP client result type that can contain either a value or an error
 */
template <typename T>
using UdpResult = type::expected<T, UdpError>;

/**
 * @brief Structure holding information about the remote endpoint
 */
struct RemoteEndpoint {
    std::string host;
    uint16_t port;

    bool operator==(const RemoteEndpoint&) const = default;
};

/**
 * @brief Callback concept for data received events
 */
template <typename T>
concept DataReceivedHandler = requires(T callback, std::span<const char> data,
                                       const RemoteEndpoint& endpoint) {
    { callback(data, endpoint) } -> std::same_as<void>;
};

/**
 * @brief Callback concept for error events
 */
template <typename T>
concept ErrorHandler =
    requires(T callback, UdpError error, const std::string& message) {
        { callback(error, message) } -> std::same_as<void>;
    };
/**
 * @class UdpClient
 * @brief Represents a UDP client for sending and receiving datagrams with
 * modern C++20 features.
 */
class UdpClient {
public:
    /**
     * @brief Default constructor
     * @throws std::runtime_error if the socket creation fails
     */
    UdpClient();

    /**
     * @brief Constructor with specific local port
     * @param port Local port to bind to
     * @throws std::runtime_error if the socket creation or binding fails
     */
    explicit UdpClient(uint16_t port);

    /**
     * @brief Destructor
     */
    ~UdpClient();

    /**
     * @brief Deleted copy constructor to prevent copying
     */
    UdpClient(const UdpClient&) = delete;

    /**
     * @brief Deleted copy assignment operator to prevent copying
     */
    UdpClient& operator=(const UdpClient&) = delete;

    /**
     * @brief Move constructor
     */
    UdpClient(UdpClient&&) noexcept;

    /**
     * @brief Move assignment operator
     */
    UdpClient& operator=(UdpClient&&) noexcept;

    /**
     * @brief Binds the client to a specific port for receiving data
     * @param port The port number to bind to
     * @return Result containing success or error code
     */
    [[nodiscard]] UdpResult<bool> bind(uint16_t port) noexcept;

    /**
     * @brief Sends data to a specified host and port
     * @param endpoint The destination endpoint (host and port)
     * @param data The data to be sent
     * @return Result containing bytes sent or error code
     */
    [[nodiscard]] UdpResult<size_t> send(const RemoteEndpoint& endpoint,
                                         std::span<const char> data) noexcept;

    /**
     * @brief Convenience overload for sending string data
     */
    [[nodiscard]] UdpResult<size_t> send(const RemoteEndpoint& endpoint,
                                         const std::string& data) noexcept;

    /**
     * @brief Receives data from a remote host
     * @param maxSize Maximum number of bytes to receive
     * @param timeout The receive timeout duration
     * @return Result containing received data and endpoint or error code
     */
    [[nodiscard]] UdpResult<std::pair<std::vector<char>, RemoteEndpoint>>
    receive(size_t maxSize, std::chrono::milliseconds timeout =
                                std::chrono::milliseconds::zero()) noexcept;

    /**
     * @brief Awaitable coroutine for asynchronous receiving
     */
    struct [[nodiscard]] ReceiveAwaitable {
        UdpClient& client;
        size_t maxSize;
        std::chrono::milliseconds timeout;

        ReceiveAwaitable(UdpClient& client, size_t maxSize,
                         std::chrono::milliseconds timeout)
            : client(client), maxSize(maxSize), timeout(timeout) {}

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h);
        UdpResult<std::pair<std::vector<char>, RemoteEndpoint>> await_resume();

    private:
        UdpResult<std::pair<std::vector<char>, RemoteEndpoint>> result_;
    };

    /**
     * @brief Create an awaitable for asynchronous receiving
     */
    [[nodiscard]] ReceiveAwaitable receiveAsync(
        size_t maxSize, std::chrono::milliseconds timeout =
                            std::chrono::milliseconds::zero()) noexcept {
        return ReceiveAwaitable(*this, maxSize, timeout);
    }

    /**
     * @brief Sets the callback function to be called when data is received
     * @param callback The callback function
     */
    template <typename Handler>
        requires DataReceivedHandler<Handler>
    void setOnDataReceivedCallback(Handler&& callback) {
        onDataReceivedCallback_ = std::forward<Handler>(callback);
    }

    /**
     * @brief Sets the callback function to be called when an error occurs
     * @param callback The callback function
     */
    template <typename Handler>
        requires ErrorHandler<Handler>
    void setOnErrorCallback(Handler&& callback) {
        onErrorCallback_ = std::forward<Handler>(callback);
    }

    /**
     * @brief Starts receiving data asynchronously
     * @param bufferSize The size of the receive buffer
     * @return Result containing success or error code
     */
    [[nodiscard]] UdpResult<bool> startReceiving(size_t bufferSize) noexcept;

    /**
     * @brief Stops receiving data
     */
    void stopReceiving() noexcept;

    /**
     * @brief Check if the client is currently receiving data
     */
    [[nodiscard]] bool isReceiving() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Store callbacks at this level to enforce concept constraints
    std::function<void(std::span<const char>, const RemoteEndpoint&)>
        onDataReceivedCallback_;
    std::function<void(UdpError, const std::string&)> onErrorCallback_;
};

}  // namespace atom::connection
#endif  // ATOM_CONNECTION_UDPCLIENT_HPP