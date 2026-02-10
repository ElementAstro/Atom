/*
 * async_udpclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************
Date: 2024-5-24
Description: Asynchronous UDP Client Class using Asio
*************************************************/

#ifndef ATOM_CONNECTION_ASYNC_UDPCLIENT_HPP
#define ATOM_CONNECTION_ASYNC_UDPCLIENT_HPP

#include <asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "udp_common.hpp"

namespace atom::async::connection {

// Import common types from udp namespace
using atom::connection::udp::RemoteEndpoint;
using atom::connection::udp::SocketOption;
using atom::connection::udp::UdpError;
using atom::connection::udp::UdpResult;
using atom::connection::udp::UdpStatistics;

/**
 * @class AsyncUdpClient
 * @brief Represents an asynchronous UDP client for sending and receiving
 * datagrams. This class provides a high-performance, thread-safe UDP client
 * implementation using Asio for asynchronous I/O operations.
 */
class AsyncUdpClient {
public:
    /// Backward compatibility alias for Statistics
    using Statistics = UdpStatistics;

    using OnDataReceivedCallback =
        std::function<void(const std::vector<char>&, const std::string&, int)>;
    using OnErrorCallback = std::function<void(const std::string&, int)>;
    using OnStatusCallback = std::function<void(const std::string&)>;

    /**
     * @brief Constructs a new async UDP client using IPv4.
     */
    AsyncUdpClient();

    /**
     * @brief Constructs a new async UDP client with a specified IP version.
     * @param use_ipv6 Set to true to use IPv6, false for IPv4.
     */
    explicit AsyncUdpClient(bool use_ipv6);

    /**
     * @brief Destructor.
     */
    ~AsyncUdpClient();

    AsyncUdpClient(const AsyncUdpClient&) = delete;
    AsyncUdpClient& operator=(const AsyncUdpClient&) = delete;

    // Move constructor and assignment operator
    AsyncUdpClient(AsyncUdpClient&&) noexcept;
    AsyncUdpClient& operator=(AsyncUdpClient&&) noexcept;

    /**
     * @brief Binds the socket to a specific local port and address.
     * @param port The port number to bind to.
     * @param address The local IP address to bind to. If empty, binds to all
     * available interfaces.
     * @return true if the bind operation was successful, false otherwise.
     */
    [[nodiscard]] bool bind(int port, const std::string& address = "");

    /**
     * @brief Sends a block of data to a specified destination.
     * @param host The hostname or IP address of the recipient.
     * @param port The port number of the recipient.
     * @param data A vector of characters containing the data to send.
     * @return true if the data was sent successfully, false otherwise.
     */
    [[nodiscard]] bool send(const std::string& host, int port,
                            const std::vector<char>& data);

    /**
     * @brief Sends a string to a specified destination.
     * @param host The hostname or IP address of the recipient.
     * @param port The port number of the recipient.
     * @param data The string data to send.
     * @return true if the data was sent successfully, false otherwise.
     */
    [[nodiscard]] bool send(const std::string& host, int port,
                            const std::string& data);

    /**
     * @brief Sends data with a specified timeout.
     * @param host The hostname or IP address of the recipient.
     * @param port The port number of the recipient.
     * @param data The data to send.
     * @param timeout The maximum time to wait for the send operation to
     * complete.
     * @return true if the data was sent within the timeout, false otherwise.
     */
    [[nodiscard]] bool sendWithTimeout(const std::string& host, int port,
                                       const std::vector<char>& data,
                                       std::chrono::milliseconds timeout);

    /**
     * @brief Sends the same data packet to multiple destinations.
     * @param destinations A vector of host-port pairs.
     * @param data The data to send.
     * @return The number of destinations to which the data was sent
     * successfully.
     */
    [[nodiscard]] int batchSend(
        const std::vector<std::pair<std::string, int>>& destinations,
        const std::vector<char>& data);

    /**
     * @brief Receives data synchronously with an optional timeout.
     * @param size The maximum number of bytes to receive.
     * @param[out] remoteHost The IP address of the sender.
     * @param[out] remotePort The port of the sender.
     * @param timeout The maximum time to wait for data. If zero, waits
     * indefinitely.
     * @return A vector containing the received data. Returns an empty vector on
     * timeout or error.
     */
    std::vector<char> receive(
        size_t size, std::string& remoteHost, int& remotePort,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /**
     * @brief Registers a callback function to be invoked when data is received
     * asynchronously.
     * @param callback The function to call with received data, sender host, and
     * port.
     */
    void setOnDataReceivedCallback(const OnDataReceivedCallback& callback);

    /**
     * @brief Registers a callback function for handling errors.
     * @param callback The function to call with an error message and error
     * code.
     */
    void setOnErrorCallback(const OnErrorCallback& callback);

    /**
     * @brief Registers a callback function for status updates.
     * @param callback The function to call with a status message.
     */
    void setOnStatusCallback(const OnStatusCallback& callback);

    /**
     * @brief Starts asynchronous data reception.
     * Once started, the client listens for incoming data and invokes the
     * OnDataReceivedCallback.
     * @param bufferSize The size of the internal buffer for incoming data.
     */
    void startReceiving(size_t bufferSize = 4096);

    /**
     * @brief Stops asynchronous data reception.
     */
    void stopReceiving();

    /**
     * @brief Configures a socket option.
     * @param option The socket option to configure.
     * @param value The value to set for the option.
     * @return true if the option was set successfully, false otherwise.
     */
    [[nodiscard]] bool setSocketOption(SocketOption option, int value);

    /**
     * @brief Sets the Time-To-Live (TTL) for unicast packets.
     * @param ttl The TTL value.
     * @return true if successful, false otherwise.
     */
    [[nodiscard]] bool setTTL(int ttl);

    /**
     * @brief Joins a multicast group.
     * @param multicastAddress The IP address of the multicast group to join.
     * @param interfaceAddress The local interface address to use. If empty, the
     * OS chooses.
     * @return true if the group was joined successfully, false otherwise.
     */
    [[nodiscard]] bool joinMulticastGroup(
        const std::string& multicastAddress,
        const std::string& interfaceAddress = "");

    /**
     * @brief Leaves a multicast group.
     * @param multicastAddress The IP address of the multicast group to leave.
     * @param interfaceAddress The local interface address used to join.
     * @return true if the group was left successfully, false otherwise.
     */
    [[nodiscard]] bool leaveMulticastGroup(
        const std::string& multicastAddress,
        const std::string& interfaceAddress = "");

    /**
     * @brief Gets the local address and port the socket is bound to.
     * @return A pair containing the local IP address and port.
     */
    [[nodiscard]] std::pair<std::string, int> getLocalEndpoint() const;

    /**
     * @brief Checks if the socket is currently open.
     * @return true if the socket is open, false otherwise.
     */
    [[nodiscard]] bool isOpen() const noexcept;

    /**
     * @brief Closes the socket, stopping all operations.
     */
    void close();

    /**
     * @brief Retrieves the current communication statistics.
     * @return A copy of the Statistics struct.
     */
    [[nodiscard]] Statistics getStatistics() const;

    /**
     * @brief Resets all communication statistics to zero.
     */
    void resetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Backward compatibility alias
using UdpClient = AsyncUdpClient;

}  // namespace atom::async::connection
#endif  // ATOM_CONNECTION_ASYNC_UDPCLIENT_HPP
