#ifndef ATOM_CONNECTION_ASYNC_TCPCLIENT_HPP
#define ATOM_CONNECTION_ASYNC_TCPCLIENT_HPP

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace atom::async::connection {

/**
 * @brief Enum representing different connection states
 */
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

/**
 * @brief Struct for connection statistics
 */
struct ConnectionStats {
    std::size_t total_bytes_sent{0};
    std::size_t total_bytes_received{0};
    std::size_t connection_attempts{0};
    std::size_t successful_connections{0};
    std::size_t failed_connections{0};
    std::chrono::steady_clock::time_point last_connected_time{};
    std::chrono::steady_clock::time_point last_activity_time{};
    std::chrono::milliseconds average_latency{0};
};

/**
 * @brief Struct for connection configuration
 */
struct ConnectionConfig {
    bool use_ssl{false};
    bool verify_ssl{true};
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds read_timeout{5000};
    std::chrono::milliseconds write_timeout{5000};
    bool keep_alive{true};
    int reconnect_attempts{3};
    std::chrono::milliseconds reconnect_delay{1000};
    std::chrono::milliseconds heartbeat_interval{5000};
    size_t receive_buffer_size{4096};
    bool auto_reconnect{true};
    std::string ssl_certificate_path{};
    std::string ssl_private_key_path{};
    std::string ca_certificate_path{};
};

/**
 * @brief Struct for proxy configuration
 */
struct ProxyConfig {
    std::string host;
    int port{0};
    std::string username;
    std::string password;
    bool enabled{false};
};

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

    /**
     * @brief Construct a new TCP Client
     *
     * @param config Connection configuration
     */
    explicit TcpClient(const ConnectionConfig& config = ConnectionConfig{});

    /**
     * @brief Destructor
     */
    ~TcpClient();

    /**
     * @brief Connect to a server
     *
     * @param host Server hostname or IP address
     * @param port Server port
     * @param timeout Connection timeout (overrides config timeout if specified)
     * @return true if connection initiated successfully
     */
    bool connect(
        const std::string& host, int port,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Connect asynchronously to a server
     *
     * @param host Server hostname or IP address
     * @param port Server port
     * @return std::future<bool> Future with connection result
     */
    std::future<bool> connectAsync(const std::string& host, int port);

    /**
     * @brief Disconnect from the server
     */
    void disconnect();

    /**
     * @brief Configure reconnection behavior
     *
     * @param attempts Number of reconnection attempts (0 to disable)
     * @param delay Delay between attempts
     */
    void configureReconnection(int attempts, std::chrono::milliseconds delay =
                                                 std::chrono::seconds(1));

    /**
     * @brief Set the heartbeat interval
     *
     * @param interval Interval between heartbeats
     * @param data Optional heartbeat data to send
     */
    void setHeartbeatInterval(std::chrono::milliseconds interval,
                              const std::vector<char>& data = {});

    /**
     * @brief Send raw data to the server
     *
     * @param data Data to send
     * @return true if send successful
     */
    bool send(const std::vector<char>& data);

    /**
     * @brief Send string data to the server
     *
     * @param data String data to send
     * @return true if send successful
     */
    bool sendString(const std::string& data);

    /**
     * @brief Send data with timeout
     *
     * @param data Data to send
     * @param timeout Send timeout
     * @return true if send successful
     */
    bool sendWithTimeout(const std::vector<char>& data,
                         std::chrono::milliseconds timeout);

    /**
     * @brief Receive specific amount of data
     *
     * @param size Amount of data to receive
     * @param timeout Receive timeout
     * @return std::future<std::vector<char>> Future with received data
     */
    std::future<std::vector<char>> receive(
        size_t size,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Receive data until delimiter is found
     *
     * @param delimiter Delimiter to stop at
     * @param timeout Receive timeout
     * @return std::future<std::string> Future with received string
     */
    std::future<std::string> receiveUntil(
        char delimiter,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Perform a request-response cycle
     *
     * @param request Data to send
     * @param response_size Expected response size
     * @param timeout Operation timeout
     * @return std::future<std::vector<char>> Future with response
     */
    std::future<std::vector<char>> requestResponse(
        const std::vector<char>& request, size_t response_size,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Set proxy configuration
     *
     * @param config Proxy configuration
     */
    void setProxyConfig(const ProxyConfig& config);

    /**
     * @brief Configure SSL certificates
     *
     * @param cert_path Path to certificate file
     * @param key_path Path to private key file
     * @param ca_path Path to CA certificate file
     */
    void configureSslCertificates(const std::string& cert_path,
                                  const std::string& key_path,
                                  const std::string& ca_path);

    /**
     * @brief Get the current connection state
     *
     * @return ConnectionState Current state
     */
    [[nodiscard]] ConnectionState getConnectionState() const;

    /**
     * @brief Check if client is connected
     *
     * @return true if connected
     */
    [[nodiscard]] bool isConnected() const;

    /**
     * @brief Get the most recent error message
     *
     * @return std::string Error message
     */
    [[nodiscard]] std::string getErrorMessage() const;

    /**
     * @brief Get connection statistics
     *
     * @return const ConnectionStats& Statistics
     */
    [[nodiscard]] const ConnectionStats& getStats() const;

    /**
     * @brief Reset connection statistics
     */
    void resetStats();

    /**
     * @brief Get the remote endpoint address
     *
     * @return std::string Remote address
     */
    [[nodiscard]] std::string getRemoteAddress() const;

    /**
     * @brief Get the remote endpoint port
     *
     * @return int Remote port
     */
    [[nodiscard]] int getRemotePort() const;

    /**
     * @brief Set a property for this connection
     *
     * @param key Property key
     * @param value Property value
     */
    void setProperty(const std::string& key, const std::string& value);

    /**
     * @brief Get a connection property
     *
     * @param key Property key
     * @return std::string Property value or empty string if not found
     */
    [[nodiscard]] std::string getProperty(const std::string& key) const;

    /**
     * @brief Set callback for connection initiation
     */
    void setOnConnectingCallback(const OnConnectingCallback& callback);

    /**
     * @brief Set callback for successful connection
     */
    void setOnConnectedCallback(const OnConnectedCallback& callback);

    /**
     * @brief Set callback for disconnection
     */
    void setOnDisconnectedCallback(const OnDisconnectedCallback& callback);

    /**
     * @brief Set callback for data reception
     */
    void setOnDataReceivedCallback(const OnDataReceivedCallback& callback);

    /**
     * @brief Set callback for error reporting
     */
    void setOnErrorCallback(const OnErrorCallback& callback);

    /**
     * @brief Set callback for state changes
     */
    void setOnStateChangedCallback(const OnStateChangedCallback& callback);

    /**
     * @brief Set callback for heartbeat events
     */
    void setOnHeartbeatCallback(const OnHeartbeatCallback& callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::async::connection

#endif  // ATOM_CONNECTION_ASYNC_TCPCLIENT_HPP