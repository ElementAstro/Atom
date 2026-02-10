/*
 * async_udpserver.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4
Revision Date: 2024-05-22

Description: A high-performance, Asio-based asynchronous
             UDP server utilizing modern C++ concurrency.

*************************************************/

#ifndef ATOM_CONNECTION_ASYNC_UDPSERVER_HPP
#define ATOM_CONNECTION_ASYNC_UDPSERVER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include "udp_common.hpp"

namespace atom::async::connection {

// Import common types from udp namespace
using atom::connection::udp::SocketOption;
using atom::connection::udp::UdpError;
using atom::connection::udp::UdpResult;
using atom::connection::udp::UdpStatistics;

/**
 * @class AsyncUdpServer
 * @brief Represents a high-performance, asynchronous UDP server.
 *
 * This class provides a robust and scalable interface for UDP communication,
 * supporting asynchronous operations, multicast, broadcast, and fine-grained
 * configuration. It leverages modern C++ concurrency primitives for lock-free
 * reads and high throughput in multi-core environments.
 */
class AsyncUdpServer {
public:
    /**
     * @brief Callback function for handling incoming messages.
     * @param message The received data as a string.
     * @param senderIp The IP address of the sender.
     * @param senderPort The port of the sender.
     */
    using MessageHandler = std::function<void(const std::string& message,
                                              const std::string& senderIp,
                                              unsigned short senderPort)>;

    /**
     * @brief Callback function for handling errors.
     * @param errorMessage A descriptive error message.
     * @param errorCode The system error code associated with the error.
     */
    using ErrorHandler = std::function<void(const std::string& errorMessage,
                                            const std::error_code& errorCode)>;

    /// Backward compatibility alias for Statistics
    using Statistics = UdpStatistics;

    /**
     * @brief Constructs an async UDP server with a single worker thread.
     */
    AsyncUdpServer();

    /**
     * @brief Constructs an async UDP server with a specific number of worker
     * threads.
     * @param numThreads The number of worker threads for processing I/O events.
     */
    explicit AsyncUdpServer(unsigned int numThreads);

    /**
     * @brief Destructor. Stops the server if it is running.
     */
    ~AsyncUdpServer();

    AsyncUdpServer(const AsyncUdpServer&) = delete;
    AsyncUdpServer& operator=(const AsyncUdpServer&) = delete;
    AsyncUdpServer(AsyncUdpServer&&) = delete;
    AsyncUdpServer& operator=(AsyncUdpServer&&) = delete;

    /**
     * @brief Starts the UDP server on a specified port.
     * @param port The port number to listen on.
     * @param ipv6 Set to true to use IPv6, false for IPv4 (default).
     * @return true if the server started successfully, false otherwise.
     */
    [[nodiscard]] bool start(unsigned short port, bool ipv6 = false);

    /**
     * @brief Stops the UDP server gracefully.
     */
    void stop();

    /**
     * @brief Checks if the server is currently running.
     * @return true if the server is running, false otherwise.
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Adds a message handler to be called upon message reception.
     * This operation is thread-safe.
     * @param handler The callback function to add.
     */
    void addMessageHandler(MessageHandler handler);

    /**
     * @brief Removes a message handler.
     * @param handler The handler to remove. Note: Relies on std::function
     * target comparison, which may be unreliable for complex callables like
     * lambdas not stored in a variable.
     */
    void removeMessageHandler(MessageHandler handler);

    /**
     * @brief Adds an error handler to be called when an error occurs.
     * This operation is thread-safe.
     * @param handler The callback function to add.
     */
    void addErrorHandler(ErrorHandler handler);

    /**
     * @brief Removes an error handler.
     * @param handler The handler to remove. Note: Relies on std::function
     * target comparison.
     */
    void removeErrorHandler(ErrorHandler handler);

    /**
     * @brief Sends a message to a specific unicast destination.
     * @param message The data to send.
     * @param ipAddress The destination IP address.
     * @param port The destination port.
     * @return true if the message was successfully queued for sending, false
     * otherwise.
     */
    [[nodiscard]] bool sendTo(const std::string& message,
                              const std::string& ipAddress,
                              unsigned short port);

    /**
     * @brief Broadcasts a message to all devices on the local network.
     * @param message The data to broadcast.
     * @param port The destination port.
     * @return true if the message was successfully queued for broadcasting,
     * false otherwise.
     */
    [[nodiscard]] bool broadcast(const std::string& message,
                                 unsigned short port);

    /**
     * @brief Joins a multicast group to receive messages sent to that group.
     * @param multicastAddress The IP address of the multicast group.
     * @return true if the group was joined successfully, false otherwise.
     */
    [[nodiscard]] bool joinMulticastGroup(const std::string& multicastAddress);

    /**
     * @brief Leaves a multicast group.
     * @param multicastAddress The IP address of the multicast group.
     * @return true if the group was left successfully, false otherwise.
     */
    [[nodiscard]] bool leaveMulticastGroup(const std::string& multicastAddress);

    /**
     * @brief Sends a message to a specific multicast group.
     * @param message The data to send.
     * @param multicastAddress The destination multicast group IP address.
     * @param port The destination port.
     * @return true if the message was successfully queued for sending, false
     * otherwise.
     */
    [[nodiscard]] bool sendToMulticast(const std::string& message,
                                       const std::string& multicastAddress,
                                       unsigned short port);

    /**
     * @brief Sets a low-level socket option.
     * This function is type-safe and will fail compilation for invalid
     * type/option pairs.
     * @tparam T The type of the option value (e.g., bool, int).
     * @param option The socket option to configure.
     * @param value The value to set for the option.
     * @return true if the option was set successfully, false otherwise.
     */
    template <typename T>
    [[nodiscard]] bool setSocketOption(SocketOption option, const T& value);

    /**
     * @brief Sets the size of the kernel's receive buffer for the socket.
     * @param size The desired buffer size in bytes.
     * @return true if the buffer size was set successfully, false otherwise.
     */
    [[nodiscard]] bool setReceiveBufferSize(std::size_t size);

    /**
     * @brief Sets a timeout for synchronous receive operations on the socket.
     * @param timeout The timeout duration.
     * @return true if the timeout was set successfully, false otherwise.
     */
    [[nodiscard]] bool setReceiveTimeout(
        const std::chrono::milliseconds& timeout);

    /**
     * @brief Retrieves the current communication statistics.
     * @return A copy of the Statistics struct.
     */
    [[nodiscard]] Statistics getStatistics() const;

    /**
     * @brief Resets all communication statistics to zero.
     */
    void resetStatistics();

    /**
     * @brief Adds an IP address to the whitelist. If the whitelist is enabled,
     * only messages from these IPs are processed. Reads from the whitelist are
     * lock-free.
     * @param ip The IP address to allow.
     */
    void addAllowedIp(const std::string& ip);

    /**
     * @brief Removes an IP address from the whitelist.
     * @param ip The IP address to remove.
     */
    void removeAllowedIp(const std::string& ip);

    /**
     * @brief Clears the IP whitelist, effectively disabling IP filtering.
     */
    void clearIpFilters();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Backward compatibility alias
using UdpSocketHub = AsyncUdpServer;

}  // namespace atom::async::connection

#endif  // ATOM_CONNECTION_ASYNC_UDPSERVER_HPP
