#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include "packet.hpp"
#include "protocol.hpp"
#include "types.hpp"


/**
 * @file client.hpp
 * @brief Defines the MQTT Client class, providing a modern C++20 MQTT client
 * implementation.
 */

namespace mqtt {

/**
 * @class Client
 * @brief Modern MQTT Client with C++20 Features.
 *
 * The Client class provides a full-featured, thread-safe, and asynchronous MQTT
 * client implementation using modern C++20 features and ASIO for networking. It
 * supports secure (TLS) and plain connections, automatic reconnection, QoS
 * management, event handlers, statistics, and advanced configuration.
 *
 * Key features:
 * - Asynchronous connect, publish, subscribe, and unsubscribe operations
 * - Support for both plain TCP and SSL/TLS transports
 * - Automatic reconnection with exponential backoff
 * - Keep-alive and ping management
 * - QoS 0/1/2 message tracking and retransmission
 * - User-defined event handlers for messages, connection, and disconnection
 * - Thread-safe statistics and monitoring
 * - Customizable client ID and connection options
 * - Move-only semantics (copy disabled)
 */
class Client {
private:
    // Core components
    asio::io_context
        io_context_;  ///< ASIO I/O context for all async operations.
    std::unique_ptr<asio::ssl::context>
        ssl_context_;  ///< SSL context for TLS connections.
    std::unique_ptr<ITransport>
        transport_;  ///< Network transport (TCP or TLS).
    std::unique_ptr<std::thread>
        io_thread_;  ///< Thread running the IO context.

    // Connection management
    std::atomic<ConnectionState> state_{
        ConnectionState::DISCONNECTED};     ///< Current connection state.
    ConnectionOptions connection_options_;  ///< Options for MQTT connection.
    std::string broker_host_;               ///< MQTT broker hostname or IP.
    uint16_t broker_port_{1883};            ///< MQTT broker port.

    // Packet handling
    std::atomic<uint16_t> next_packet_id_{
        1};  ///< Next packet identifier for outgoing packets.
    std::unordered_map<uint16_t, PendingOperation>
        pending_operations_;  ///< Map of packet ID to pending operation.
    std::mutex pending_operations_mutex_;  ///< Mutex for thread-safe access to
                                           ///< pending operations.

    // Message handling
    MessageHandler
        message_handler_;  ///< User-defined message handler callback.
    ConnectionHandler
        connection_handler_;  ///< User-defined connection handler callback.
    DisconnectionHandler
        disconnection_handler_;  ///< User-defined disconnection handler
                                 ///< callback.

    // Keep-alive mechanism
    std::unique_ptr<asio::steady_timer>
        keep_alive_timer_;  ///< Timer for keep-alive interval.
    std::unique_ptr<asio::steady_timer>
        ping_timeout_timer_;  ///< Timer for ping response timeout.
    std::chrono::steady_clock::time_point
        last_packet_received_;  ///< Timestamp of last received packet.

    // Statistics and monitoring
    ClientStats stats_;  ///< Client statistics (bytes sent/received, etc).
    mutable std::shared_mutex
        stats_mutex_;  ///< Mutex for thread-safe stats access.

    // Read buffer management
    static constexpr size_t READ_BUFFER_SIZE =
        8192;  ///< Size of the read buffer.
    std::array<uint8_t, READ_BUFFER_SIZE>
        read_buffer_;             ///< Buffer for incoming data.
    BinaryBuffer packet_buffer_;  ///< Buffer for assembling packets.

    // Reconnection logic
    std::unique_ptr<asio::steady_timer>
        reconnect_timer_;  ///< Timer for reconnection attempts.
    std::chrono::seconds reconnect_delay_{1};  ///< Current reconnect delay.
    static constexpr std::chrono::seconds MAX_RECONNECT_DELAY{
        60};                     ///< Maximum reconnect delay.
    bool auto_reconnect_{true};  ///< Whether to automatically reconnect.

    // Random number generation for client ID
    mutable std::random_device rd_;    ///< Random device for seeding.
    mutable std::mt19937 gen_{rd_()};  ///< Mersenne Twister RNG for client ID.

public:
    /**
     * @brief Construct a new MQTT Client.
     * @param auto_start_io If true, automatically starts the IO thread.
     */
    explicit Client(bool auto_start_io = true);

    /**
     * @brief Destructor. Cleans up resources and stops IO thread.
     */
    ~Client();

    // Disable copy semantics, enable move semantics
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /**
     * @name Connection Management
     * @{
     */

    /**
     * @brief Asynchronously connect to the MQTT broker.
     * @param host Broker hostname or IP address.
     * @param port Broker port.
     * @param options Connection options (client ID, credentials, etc).
     * @param callback Optional callback invoked on connection result.
     */
    void async_connect(const std::string& host, uint16_t port,
                       const ConnectionOptions& options,
                       ConnectionHandler callback = nullptr);

    /**
     * @brief Disconnect from the MQTT broker.
     * @param reason Error code for disconnect (default: SUCCESS).
     */
    void disconnect(ErrorCode reason = ErrorCode::SUCCESS);

    /**
     * @brief Check if the client is currently connected.
     * @return True if connected.
     */
    [[nodiscard]] bool is_connected() const noexcept {
        return state_.load() == ConnectionState::CONNECTED;
    }

    /**
     * @brief Get the current connection state.
     * @return ConnectionState enum value.
     */
    [[nodiscard]] ConnectionState get_state() const noexcept {
        return state_.load();
    }
    /** @} */

    /**
     * @name Publishing
     * @{
     */

    /**
     * @brief Asynchronously publish a message to a topic.
     * @tparam PayloadType Type convertible to std::span<const uint8_t> or
     * std::string_view.
     * @param topic Topic string.
     * @param payload Message payload.
     * @param qos Quality of Service level (default: AT_MOST_ONCE).
     * @param retain Retain flag (default: false).
     * @param callback Optional callback invoked on publish result.
     */
    template <typename PayloadType>
        requires std::convertible_to<PayloadType, std::span<const uint8_t>> ||
                 std::convertible_to<PayloadType, std::string_view>
    void async_publish(const std::string& topic, PayloadType&& payload,
                       QoS qos = QoS::AT_MOST_ONCE, bool retain = false,
                       std::function<void(ErrorCode)> callback = nullptr) {
        Message msg;
        msg.topic = topic;
        msg.qos = qos;
        msg.retain = retain;

        if constexpr (std::convertible_to<PayloadType, std::string_view>) {
            std::string_view sv = payload;
            msg.payload.assign(sv.begin(), sv.end());
        } else {
            std::span<const uint8_t> span = payload;
            msg.payload.assign(span.begin(), span.end());
        }

        async_publish(std::move(msg), std::move(callback));
    }

    /**
     * @brief Asynchronously publish a message object.
     * @param message Message to publish.
     * @param callback Optional callback invoked on publish result.
     */
    void async_publish(Message message,
                       std::function<void(ErrorCode)> callback = nullptr);

    /** @} */

    /**
     * @name Subscription Management
     * @{
     */

    /**
     * @brief Asynchronously subscribe to a topic filter.
     * @param topic_filter Topic filter string.
     * @param qos Quality of Service level (default: AT_MOST_ONCE).
     * @param callback Optional callback invoked on subscribe result.
     */
    void async_subscribe(const std::string& topic_filter,
                         QoS qos = QoS::AT_MOST_ONCE,
                         std::function<void(ErrorCode)> callback = nullptr);

    /**
     * @brief Asynchronously subscribe to multiple topic filters.
     * @param subscriptions Vector of Subscription objects.
     * @param callback Optional callback invoked with vector of result codes.
     */
    void async_subscribe(
        const std::vector<Subscription>& subscriptions,
        std::function<void(std::vector<ErrorCode>)> callback = nullptr);

    /**
     * @brief Asynchronously unsubscribe from a topic filter.
     * @param topic_filter Topic filter string.
     * @param callback Optional callback invoked on unsubscribe result.
     */
    void async_unsubscribe(const std::string& topic_filter,
                           std::function<void(ErrorCode)> callback = nullptr);

    /**
     * @brief Asynchronously unsubscribe from multiple topic filters.
     * @param topic_filters Vector of topic filter strings.
     * @param callback Optional callback invoked with vector of result codes.
     */
    void async_unsubscribe(
        const std::vector<std::string>& topic_filters,
        std::function<void(std::vector<ErrorCode>)> callback = nullptr);

    /** @} */

    /**
     * @name Event Handlers
     * @{
     */

    /**
     * @brief Set the message handler callback.
     * @param handler Function to call when a message is received.
     */
    void set_message_handler(MessageHandler handler) {
        message_handler_ = std::move(handler);
    }

    /**
     * @brief Set the connection handler callback.
     * @param handler Function to call on connection events.
     */
    void set_connection_handler(ConnectionHandler handler) {
        connection_handler_ = std::move(handler);
    }

    /**
     * @brief Set the disconnection handler callback.
     * @param handler Function to call on disconnection events.
     */
    void set_disconnection_handler(DisconnectionHandler handler) {
        disconnection_handler_ = std::move(handler);
    }

    /** @} */

    /**
     * @name Configuration
     * @{
     */

    /**
     * @brief Enable or disable automatic reconnection.
     * @param enable True to enable, false to disable.
     */
    void set_auto_reconnect(bool enable) noexcept { auto_reconnect_ = enable; }

    /**
     * @brief Get whether automatic reconnection is enabled.
     * @return True if enabled.
     */
    [[nodiscard]] bool get_auto_reconnect() const noexcept {
        return auto_reconnect_;
    }

    /** @} */

    /**
     * @name Statistics and Monitoring
     * @{
     */

    /**
     * @brief Get a snapshot of the current client statistics.
     * @return ClientStats structure.
     */
    [[nodiscard]] ClientStats get_stats() const {
        std::shared_lock lock(stats_mutex_);
        return stats_;
    }

    /**
     * @brief Reset the client statistics.
     */
    void reset_stats() {
        std::unique_lock lock(stats_mutex_);
        stats_ = ClientStats{};
        stats_.connected_since = std::chrono::steady_clock::now();
    }

    /** @} */

    /**
     * @name Advanced Features
     * @{
     */

    /**
     * @brief Run the IO context in the current thread.
     */
    void run() { io_context_.run(); }

    /**
     * @brief Stop the IO context and all asynchronous operations.
     */
    void stop() { io_context_.stop(); }

    /**
     * @brief Get a reference to the underlying ASIO IO context.
     * @return Reference to asio::io_context.
     */
    [[nodiscard]] asio::io_context& get_io_context() noexcept {
        return io_context_;
    }

    /** @} */

private:
    /**
     * @name Internal Implementation Methods
     * @{
     */

    /**
     * @brief Setup the SSL context based on connection options.
     * @param options Connection options.
     */
    void setup_ssl_context(const ConnectionOptions& options);

    /**
     * @brief Start the IO thread if not already running.
     */
    void start_io_thread();

    /**
     * @brief Stop the IO thread and join if running.
     */
    void stop_io_thread();

    /**
     * @brief Perform the actual connection logic.
     */
    void perform_connect();

    /**
     * @brief Handle the result of a connection attempt.
     * @param error Error code from connection attempt.
     */
    void handle_connect_result(ErrorCode error);

    /**
     * @brief Start reading from the transport.
     */
    void start_reading();

    /**
     * @brief Handle the result of a read operation.
     * @param error Error code from read.
     * @param bytes_transferred Number of bytes read.
     */
    void handle_read(ErrorCode error, size_t bytes_transferred);

    /**
     * @brief Process received data in the packet buffer.
     */
    void process_received_data();

    /**
     * @brief Handle a received MQTT packet.
     * @param header Parsed packet header.
     * @param payload Packet payload bytes.
     */
    void handle_packet(const PacketHeader& header,
                       std::span<const uint8_t> payload);

    /**
     * @brief Send a packet to the broker.
     * @param packet BinaryBuffer containing the packet data.
     */
    void send_packet(const BinaryBuffer& packet);

    /**
     * @brief Handle the result of a write operation.
     * @param error Error code from write.
     * @param bytes_transferred Number of bytes written.
     */
    void handle_write(ErrorCode error, size_t bytes_transferred);

    /**
     * @brief Start the keep-alive timer.
     */
    void start_keep_alive();

    /**
     * @brief Handle keep-alive timeout (send PINGREQ).
     */
    void handle_keep_alive_timeout();

    /**
     * @brief Send a PINGREQ packet to the broker.
     */
    void send_ping_request();

    /**
     * @brief Handle ping response timeout.
     */
    void handle_ping_timeout();

    /**
     * @brief Schedule a reconnection attempt.
     */
    void schedule_reconnect();

    /**
     * @brief Handle the reconnection timer event.
     */
    void handle_reconnect_timer();

    /**
     * @brief Generate a new packet identifier.
     * @return Packet ID.
     */
    uint16_t generate_packet_id() noexcept;

    /**
     * @brief Generate a random client ID.
     * @return Client ID string.
     */
    std::string generate_client_id() const;

    /**
     * @brief Handle a CONNACK packet from the broker.
     * @param data Packet data.
     */
    void handle_connack(std::span<const uint8_t> data);

    /**
     * @brief Handle a PUBLISH packet from the broker.
     * @param header Packet header.
     * @param data Packet data.
     */
    void handle_publish(const PacketHeader& header,
                        std::span<const uint8_t> data);

    /**
     * @brief Handle a PUBACK packet from the broker.
     * @param data Packet data.
     */
    void handle_puback(std::span<const uint8_t> data);

    /**
     * @brief Handle a PUBREC packet from the broker.
     * @param data Packet data.
     */
    void handle_pubrec(std::span<const uint8_t> data);

    /**
     * @brief Handle a PUBREL packet from the broker.
     * @param data Packet data.
     */
    void handle_pubrel(std::span<const uint8_t> data);

    /**
     * @brief Handle a PUBCOMP packet from the broker.
     * @param data Packet data.
     */
    void handle_pubcomp(std::span<const uint8_t> data);

    /**
     * @brief Handle a SUBACK packet from the broker.
     * @param data Packet data.
     */
    void handle_suback(std::span<const uint8_t> data);

    /**
     * @brief Handle an UNSUBACK packet from the broker.
     * @param data Packet data.
     */
    void handle_unsuback(std::span<const uint8_t> data);

    /**
     * @brief Handle a PINGRESP packet from the broker.
     */
    void handle_pingresp();

    /**
     * @brief Update statistics for bytes sent.
     * @param bytes Number of bytes sent.
     */
    void update_stats_sent(size_t bytes);

    /**
     * @brief Update statistics for bytes received.
     * @param bytes Number of bytes received.
     */
    void update_stats_received(size_t bytes);

    /**
     * @brief Clean up all pending operations (e.g., on disconnect).
     */
    void cleanup_pending_operations();

    /**
     * @brief Notify user of an error via handlers.
     * @param error Error code.
     */
    void notify_error(ErrorCode error);

    /**
     * @brief Handle transport-level errors.
     * @param error Error code.
     */
    void handle_transport_error(ErrorCode error);

    /** @} */
};

}  // namespace mqtt