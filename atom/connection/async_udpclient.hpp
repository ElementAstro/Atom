/*
 * async_udpclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************
Date: 2024-5-24
Description: UDP Client Class
*************************************************/

#ifndef ATOM_CONNECTION_ASYNC_UDPCLIENT_HPP
#define ATOM_CONNECTION_ASYNC_UDPCLIENT_HPP

#include <asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace atom::async::connection {

/**
 * @class UdpClient
 * @brief Represents a UDP client for sending and receiving datagrams.
 */
class UdpClient {
public:
    enum class SocketOption {
        Broadcast,
        ReuseAddress,
        ReceiveBufferSize,
        SendBufferSize,
        ReceiveTimeout,
        SendTimeout
    };

    struct Statistics {
        std::size_t packets_sent{0};
        std::size_t packets_received{0};
        std::size_t bytes_sent{0};
        std::size_t bytes_received{0};
        std::chrono::steady_clock::time_point start_time;

        Statistics() : start_time(std::chrono::steady_clock::now()) {}

        void reset() {
            packets_sent = 0;
            packets_received = 0;
            bytes_sent = 0;
            bytes_received = 0;
            start_time = std::chrono::steady_clock::now();
        }
    };

    using OnDataReceivedCallback =
        std::function<void(const std::vector<char>&, const std::string&, int)>;
    using OnErrorCallback = std::function<void(const std::string&, int)>;
    using OnStatusCallback = std::function<void(const std::string&)>;

    /**
     * @brief Constructs a new UDP client.
     */
    UdpClient();

    /**
     * @brief Constructs a new UDP client with specified IP version.
     * @param use_ipv6 Whether to use IPv6 (true) or IPv4 (false)
     */
    explicit UdpClient(bool use_ipv6);

    /**
     * @brief Destructor
     */
    ~UdpClient();

    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;

    // Move constructor and assignment
    UdpClient(UdpClient&&) noexcept;
    UdpClient& operator=(UdpClient&&) noexcept;

    /**
     * @brief Binds the socket to a specific port.
     * @param port The port to bind to
     * @param address Optional address to bind to (default: any)
     * @return true if successful, false otherwise
     */
    bool bind(int port, const std::string& address = "");

    /**
     * @brief Sends data to a specified host and port.
     * @param host The target host
     * @param port The target port
     * @param data The data to send
     * @return true if successful, false otherwise
     */
    bool send(const std::string& host, int port, const std::vector<char>& data);

    /**
     * @brief Sends string data to a specified host and port.
     * @param host The target host
     * @param port The target port
     * @param data The string data to send
     * @return true if successful, false otherwise
     */
    bool send(const std::string& host, int port, const std::string& data);

    /**
     * @brief Sends data with timeout.
     * @param host The target host
     * @param port The target port
     * @param data The data to send
     * @param timeout Timeout duration
     * @return true if successful, false otherwise
     */
    bool sendWithTimeout(const std::string& host, int port,
                         const std::vector<char>& data,
                         std::chrono::milliseconds timeout);

    /**
     * @brief Batch sends data to multiple destinations.
     * @param destinations Vector of host:port pairs
     * @param data The data to send
     * @return Number of successful transmissions
     */
    int batchSend(const std::vector<std::pair<std::string, int>>& destinations,
                  const std::vector<char>& data);

    /**
     * @brief Receives data synchronously.
     * @param size Buffer size for received data
     * @param remoteHost Will store the sender's host
     * @param remotePort Will store the sender's port
     * @param timeout Optional timeout (zero means no timeout)
     * @return The received data
     */
    std::vector<char> receive(
        size_t size, std::string& remoteHost, int& remotePort,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /**
     * @brief Sets callback for data reception.
     * @param callback The callback function
     */
    void setOnDataReceivedCallback(const OnDataReceivedCallback& callback);

    /**
     * @brief Sets callback for errors.
     * @param callback The callback function
     */
    void setOnErrorCallback(const OnErrorCallback& callback);

    /**
     * @brief Sets callback for status updates.
     * @param callback The callback function
     */
    void setOnStatusCallback(const OnStatusCallback& callback);

    /**
     * @brief Starts asynchronous data reception.
     * @param bufferSize Size of the receive buffer
     */
    void startReceiving(size_t bufferSize = 4096);

    /**
     * @brief Stops asynchronous data reception.
     */
    void stopReceiving();

    /**
     * @brief Sets a socket option.
     * @param option The option to set
     * @param value The option value
     * @return true if successful, false otherwise
     */
    bool setSocketOption(SocketOption option, int value);

    /**
     * @brief Sets the Time To Live (TTL) value.
     * @param ttl The TTL value
     * @return true if successful, false otherwise
     */
    bool setTTL(int ttl);

    /**
     * @brief Joins a multicast group.
     * @param multicastAddress The multicast group address
     * @param interfaceAddress The local interface address (optional)
     * @return true if successful, false otherwise
     */
    bool joinMulticastGroup(const std::string& multicastAddress,
                            const std::string& interfaceAddress = "");

    /**
     * @brief Leaves a multicast group.
     * @param multicastAddress The multicast group address
     * @param interfaceAddress The local interface address (optional)
     * @return true if successful, false otherwise
     */
    bool leaveMulticastGroup(const std::string& multicastAddress,
                             const std::string& interfaceAddress = "");

    /**
     * @brief Gets the local endpoint information.
     * @return Pair of address and port
     */
    std::pair<std::string, int> getLocalEndpoint() const;

    /**
     * @brief Checks if the socket is open.
     * @return true if open, false otherwise
     */
    bool isOpen() const;

    /**
     * @brief Closes the socket.
     */
    void close();

    /**
     * @brief Gets current statistics.
     * @return The statistics
     */
    Statistics getStatistics() const;

    /**
     * @brief Resets statistics.
     */
    void resetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::async::connection
#endif  // ATOM_CONNECTION_ASYNC_UDPCLIENT_HPP
