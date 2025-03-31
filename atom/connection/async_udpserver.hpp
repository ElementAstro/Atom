/*
 * udp_server.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: A simple Asio-based UDP server.

*************************************************/

#ifndef ATOM_CONNECTION_ASYNC_UDPSERVER_HPP
#define ATOM_CONNECTION_ASYNC_UDPSERVER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace atom::async::connection {

// Forward declaration for socket options
enum class SocketOption {
    Broadcast,
    ReuseAddress,
    ReceiveBufferSize,
    SendBufferSize,
    ReceiveTimeout,
    SendTimeout
};

/**
 * @class UdpSocketHub
 * @brief Represents a hub for managing UDP sockets and message handling using
 * Asio.
 *
 * This class provides a high-level interface for UDP communication with
 * support for asynchronous operations, multicast, broadcast, and more.
 */
class UdpSocketHub {
public:
    using MessageHandler = std::function<void(
        const std::string&, const std::string&, unsigned short)>;

    using ErrorHandler =
        std::function<void(const std::string&, const std::error_code&)>;

    /**
     * @brief Statistics structure to track UDP communication metrics
     */
    struct Statistics {
        std::size_t bytesReceived = 0;
        std::size_t bytesSent = 0;
        std::size_t messagesReceived = 0;
        std::size_t messagesSent = 0;
        std::size_t errors = 0;
    };

    /**
     * @brief Constructs a UDP socket hub with default settings
     */
    UdpSocketHub();

    /**
     * @brief Constructs a UDP socket hub with a specific number of worker
     * threads
     * @param numThreads Number of worker threads for the I/O context
     */
    explicit UdpSocketHub(unsigned int numThreads);

    /**
     * @brief Destructor
     */
    ~UdpSocketHub();

    // Delete copy and move constructors/assignments
    UdpSocketHub(const UdpSocketHub&) = delete;
    UdpSocketHub& operator=(const UdpSocketHub&) = delete;
    UdpSocketHub(UdpSocketHub&&) = delete;
    UdpSocketHub& operator=(UdpSocketHub&&) = delete;

    /**
     * @brief Starts the UDP server on the specified port
     * @param port The port to listen on
     * @param ipv6 Whether to use IPv6 (defaults to false, using IPv4)
     * @return True if started successfully, false otherwise
     */
    bool start(unsigned short port, bool ipv6 = false);

    /**
     * @brief Stops the UDP server
     */
    void stop();

    /**
     * @brief Checks if the server is running
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Adds a message handler callback
     * @param handler Function to be called when a message is received
     */
    void addMessageHandler(MessageHandler handler);

    /**
     * @brief Removes a previously added message handler
     * @param handler The handler to remove
     */
    void removeMessageHandler(MessageHandler handler);

    /**
     * @brief Adds an error handler callback
     * @param handler Function to be called when an error occurs
     */
    void addErrorHandler(ErrorHandler handler);

    /**
     * @brief Removes a previously added error handler
     * @param handler The handler to remove
     */
    void removeErrorHandler(ErrorHandler handler);

    /**
     * @brief Sends a message to a specific endpoint
     * @param message The message to send
     * @param ip The destination IP address
     * @param port The destination port
     * @return True if the message was queued for sending, false otherwise
     */
    bool sendTo(const std::string& message, const std::string& ip,
                unsigned short port);

    /**
     * @brief Broadcasts a message to all devices on the network
     * @param message The message to broadcast
     * @param port The destination port
     * @return True if the message was queued for broadcasting, false otherwise
     */
    bool broadcast(const std::string& message, unsigned short port);

    /**
     * @brief Joins a multicast group
     * @param multicastAddress The multicast group address
     * @return True if joined successfully, false otherwise
     */
    bool joinMulticastGroup(const std::string& multicastAddress);

    /**
     * @brief Leaves a multicast group
     * @param multicastAddress The multicast group address
     * @return True if left successfully, false otherwise
     */
    bool leaveMulticastGroup(const std::string& multicastAddress);

    /**
     * @brief Sends a message to a multicast group
     * @param message The message to send
     * @param multicastAddress The multicast group address
     * @param port The destination port
     * @return True if the message was queued for sending, false otherwise
     */
    bool sendToMulticast(const std::string& message,
                         const std::string& multicastAddress,
                         unsigned short port);

    /**
     * @brief Sets a socket option
     * @param option The option to set
     * @param value The value to set the option to
     * @return True if the option was set successfully, false otherwise
     */
    template <typename T>
    bool setSocketOption(SocketOption option, const T& value);

    /**
     * @brief Sets the receive buffer size
     * @param size The buffer size in bytes
     * @return True if set successfully, false otherwise
     */
    bool setReceiveBufferSize(std::size_t size);

    /**
     * @brief Sets timeout for receive operations
     * @param timeout The timeout duration
     * @return True if set successfully, false otherwise
     */
    bool setReceiveTimeout(const std::chrono::milliseconds& timeout);

    /**
     * @brief Gets the current statistics for this socket hub
     * @return A Statistics object containing usage metrics
     */
    Statistics getStatistics() const;

    /**
     * @brief Resets the statistics counters to zero
     */
    void resetStatistics();

    /**
     * @brief Adds an IP filter to allow messages only from specific IPs
     * @param ip The IP address to allow
     */
    void addAllowedIp(const std::string& ip);

    /**
     * @brief Removes an IP from the allowed list
     * @param ip The IP address to remove
     */
    void removeAllowedIp(const std::string& ip);

    /**
     * @brief Clears all IP filters
     */
    void clearIpFilters();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::async::connection

#endif