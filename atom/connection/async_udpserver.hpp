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

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace atom::async::connection {

/**
 * @enum SocketOption
 * @brief Defines socket options that can be configured for the UDP server.
 */
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
 * @brief Represents a high-performance, asynchronous UDP server hub.
 *
 * This class provides a robust and scalable interface for UDP communication,
 * supporting asynchronous operations, multicast, broadcast, and fine-grained
 * configuration. It is designed for thread safety and high throughput in
 * multi-core environments.
 */
class UdpSocketHub {
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

    /**
     * @struct Statistics
     * @brief Holds performance and usage statistics for the UDP server.
     * All counters are atomic to ensure thread-safe, lock-free updates.
     */
    struct Statistics {
        std::atomic<std::size_t> bytesReceived{0};
        std::atomic<std::size_t> bytesSent{0};
        std::atomic<std::size_t> messagesReceived{0};
        std::atomic<std::size_t> messagesSent{0};
        std::atomic<std::size_t> errors{0};

        Statistics() = default;

        Statistics(const Statistics& other)
            : bytesReceived(other.bytesReceived.load()),
              bytesSent(other.bytesSent.load()),
              messagesReceived(other.messagesReceived.load()),
              messagesSent(other.messagesSent.load()),
              errors(other.errors.load()) {}

        Statistics& operator=(const Statistics& other) {
            if (this != &other) {
                bytesReceived = other.bytesReceived.load();
                bytesSent = other.bytesSent.load();
                messagesReceived = other.messagesReceived.load();
                messagesSent = other.messagesSent.load();
                errors = other.errors.load();
            }
            return *this;
        }

        /**
         * @brief Resets all statistical counters to zero.
         */
        void reset() {
            bytesReceived.store(0, std::memory_order_relaxed);
            bytesSent.store(0, std::memory_order_relaxed);
            messagesReceived.store(0, std::memory_order_relaxed);
            messagesSent.store(0, std::memory_order_relaxed);
            errors.store(0, std::memory_order_relaxed);
        }
    };

    /**
     * @brief Constructs a UDP socket hub with a single worker thread.
     */
    UdpSocketHub();

    /**
     * @brief Constructs a UDP socket hub with a specific number of worker
     * threads.
     * @param numThreads The number of worker threads for processing I/O events.
     */
    explicit UdpSocketHub(unsigned int numThreads);

    /**
     * @brief Destructor. Stops the server if it is running.
     */
    ~UdpSocketHub();

    UdpSocketHub(const UdpSocketHub&) = delete;
    UdpSocketHub& operator=(const UdpSocketHub&) = delete;
    UdpSocketHub(UdpSocketHub&&) = delete;
    UdpSocketHub& operator=(UdpSocketHub&&) = delete;

    /**
     * @brief Starts the UDP server on a specified port.
     * @param port The port number to listen on.
     * @param ipv6 Set to true to use IPv6, false for IPv4 (default).
     * @return true if the server started successfully, false otherwise.
     */
    bool start(unsigned short port, bool ipv6 = false);

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
     * @param handler The callback function to add.
     */
    void addMessageHandler(MessageHandler handler);

    /**
     * @brief Removes a message handler.
     * @param handler The handler to remove. Note: Relies on function target
     * comparison, which may be unreliable for complex callables.
     */
    void removeMessageHandler(MessageHandler handler);

    /**
     * @brief Adds an error handler to be called when an error occurs.
     * @param handler The callback function to add.
     */
    void addErrorHandler(ErrorHandler handler);

    /**
     * @brief Removes an error handler.
     * @param handler The handler to remove. Note: Relies on function target
     * comparison.
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
    bool sendTo(const std::string& message, const std::string& ipAddress,
                unsigned short port);

    /**
     * @brief Broadcasts a message to all devices on the local network.
     * @param message The data to broadcast.
     * @param port The destination port.
     * @return true if the message was successfully queued for broadcasting,
     * false otherwise.
     */
    bool broadcast(const std::string& message, unsigned short port);

    /**
     * @brief Joins a multicast group to receive messages sent to that group.
     * @param multicastAddress The IP address of the multicast group.
     * @return true if the group was joined successfully, false otherwise.
     */
    bool joinMulticastGroup(const std::string& multicastAddress);

    /**
     * @brief Leaves a multicast group.
     * @param multicastAddress The IP address of the multicast group.
     * @return true if the group was left successfully, false otherwise.
     */
    bool leaveMulticastGroup(const std::string& multicastAddress);

    /**
     * @brief Sends a message to a specific multicast group.
     * @param message The data to send.
     * @param multicastAddress The destination multicast group IP address.
     * @param port The destination port.
     * @return true if the message was successfully queued for sending, false
     * otherwise.
     */
    bool sendToMulticast(const std::string& message,
                         const std::string& multicastAddress,
                         unsigned short port);

    /**
     * @brief Sets a low-level socket option.
     * @tparam T The type of the option value.
     * @param option The socket option to configure.
     * @param value The value to set for the option.
     * @return true if the option was set successfully, false otherwise.
     */
    template <typename T>
    bool setSocketOption(SocketOption option, const T& value);

    /**
     * @brief Sets the size of the kernel's receive buffer for the socket.
     * @param size The desired buffer size in bytes.
     * @return true if the buffer size was set successfully, false otherwise.
     */
    bool setReceiveBufferSize(std::size_t size);

    /**
     * @brief Sets a timeout for synchronous receive operations on the socket.
     * @param timeout The timeout duration.
     * @return true if the timeout was set successfully, false otherwise.
     */
    bool setReceiveTimeout(const std::chrono::milliseconds& timeout);

    /**
     * @brief Retrieves the current communication statistics.
     * @return A copy of the Statistics struct.
     */
    Statistics getStatistics() const;

    /**
     * @brief Resets all communication statistics to zero.
     */
    void resetStatistics();

    /**
     * @brief Adds an IP address to the whitelist. If the whitelist is enabled,
     * only messages from these IPs are processed.
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

}  // namespace atom::async::connection

#endif