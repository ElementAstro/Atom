/*
 * fifoclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Client

*************************************************/

#ifndef ATOM_CONNECTION_FIFOCLIENT_HPP
#define ATOM_CONNECTION_FIFOCLIENT_HPP

#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "atom/type/expected.hpp"

namespace atom::connection {

// Error codes specific to FIFO operations
enum class FifoError {
    OpenFailed,
    ReadFailed,
    WriteFailed,
    Timeout,
    InvalidOperation,
    NotOpen,
    ConnectionLost,
    MessageTooLarge,
    CompressionFailed,
    EncryptionFailed,
    DecryptionFailed
};

/**
 * @brief Enum representing message priority levels
 */
enum class MessagePriority { Low, Normal, High, Critical };

/**
 * @brief Structure to hold client statistics
 */
struct ClientStats {
    size_t messages_sent = 0;
    size_t messages_failed = 0;
    size_t bytes_sent = 0;
    size_t bytes_received = 0;
    double avg_write_latency_ms = 0.0;
    double avg_read_latency_ms = 0.0;
    size_t reconnect_attempts = 0;
    size_t successful_reconnects = 0;
    size_t avg_compression_ratio = 0;
};

/**
 * @brief Configuration options for the FIFO client
 */
struct ClientConfig {
    // Buffer size for read operations
    size_t read_buffer_size = 4096;

    // Maximum message size that can be sent
    size_t max_message_size = 1024 * 1024;  // 1MB default

    // Enable automatic reconnection on connection loss
    bool auto_reconnect = true;

    // Maximum reconnection attempts before giving up
    int max_reconnect_attempts = 5;

    // Delay between reconnection attempts
    std::chrono::milliseconds reconnect_delay{500};

    // Default timeout for operations if not specified
    std::optional<std::chrono::milliseconds> default_timeout{5000};

    // Enable data compression for large messages
    bool enable_compression = false;

    // Minimum message size for compression to be applied
    size_t compression_threshold = 1024;

    // Enable data encryption
    bool enable_encryption = false;
};

/**
 * @brief Type for operation completion callbacks
 */
using OperationCallback = std::function<void(
    bool success, std::error_code error_code, size_t bytes_transferred)>;

/**
 * @brief Type for connection status callbacks
 */
using ConnectionCallback =
    std::function<void(bool connected, std::error_code error_code)>;

/**
 * @brief A concept for types that can be written to a FIFO
 *
 * Requires that the type provides contiguous data storage and has a size
 */
template <typename T>
concept WritableData = requires(T data) {
    { std::span(data) } -> std::convertible_to<std::span<const std::byte>>;
    { data.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief A class for interacting with a FIFO (First In, First Out) pipe.
 *
 * This class provides methods to read from and write to a FIFO pipe,
 * handling timeouts and ensuring proper resource management.
 */
class FifoClient {
public:
    /**
     * @brief Constructs a FifoClient with the specified FIFO path.
     *
     * @param fifoPath The path to the FIFO file to be used for communication.
     * @throws std::system_error If the FIFO cannot be opened
     *
     * This constructor opens the FIFO and prepares the client for
     * reading and writing operations with default configuration.
     */
    explicit FifoClient(std::string_view fifoPath);

    /**
     * @brief Constructs a FifoClient with the specified FIFO path and custom
     * configuration.
     *
     * @param fifoPath The path to the FIFO file to be used for communication.
     * @param config Custom client configuration.
     * @throws std::system_error If the FIFO cannot be opened
     */
    FifoClient(std::string_view fifoPath, const ClientConfig& config);

    /**
     * @brief Move constructor
     *
     * @param other The FifoClient to move from
     */
    FifoClient(FifoClient&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other The FifoClient to move from
     * @return FifoClient& Reference to this object
     */
    FifoClient& operator=(FifoClient&& other) noexcept;

    // Disable copying
    FifoClient(const FifoClient&) = delete;
    FifoClient& operator=(const FifoClient&) = delete;

    /**
     * @brief Destroys the FifoClient and closes the FIFO if it is open.
     *
     * This destructor ensures that all resources are released and the FIFO
     * is properly closed to avoid resource leaks.
     */
    ~FifoClient();

    /**
     * @brief Writes data to the FIFO.
     *
     * @tparam T Type of data that satisfies WritableData concept
     * @param data The data to be written to the FIFO
     * @param timeout Optional timeout for the write operation, in milliseconds.
     *                If not provided, the default from configuration is used.
     * @return type::expected<std::size_t, std::error_code> The number of bytes
     * written or an error
     *
     * This method will attempt to write the specified data to the FIFO.
     * If a timeout is specified, the operation will fail if it cannot complete
     * within the given duration.
     */
    template <WritableData T>
    auto write(const T& data,
               std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::size_t, std::error_code>;

    /**
     * @brief Writes string data to the FIFO.
     *
     * @param data The string data to be written to the FIFO
     * @param timeout Optional timeout for the write operation, in milliseconds.
     * @return type::expected<std::size_t, std::error_code> The number of bytes
     * written or an error
     */
    auto write(std::string_view data,
               std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::size_t, std::error_code>;

    /**
     * @brief Writes data to the FIFO with a specified priority.
     *
     * @param data The string data to be written to the FIFO
     * @param priority Priority level for the message
     * @param timeout Optional timeout for the write operation, in milliseconds.
     * @return type::expected<std::size_t, std::error_code> The number of bytes
     * written or an error
     */
    auto write(std::string_view data, MessagePriority priority,
               std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::size_t, std::error_code>;

    /**
     * @brief Writes data to the FIFO asynchronously.
     *
     * @param data The string data to be written to the FIFO
     * @param callback Function to call when the write completes or fails
     * @param timeout Optional timeout for the write operation
     * @return int Identifier for the asynchronous operation
     */
    int writeAsync(
        std::string_view data, OperationCallback callback,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Writes data to the FIFO asynchronously with future.
     *
     * @param data The string data to be written to the FIFO
     * @param timeout Optional timeout for the write operation
     * @return std::future<type::expected<std::size_t, std::error_code>> Future
     * with result
     */
    std::future<type::expected<std::size_t, std::error_code>>
    writeAsyncWithFuture(
        std::string_view data,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Writes multiple messages to the FIFO.
     *
     * @param messages Vector of messages to send
     * @param timeout Optional timeout for each write operation
     * @return type::expected<std::size_t, std::error_code> Total number of
     * bytes written or error
     */
    auto writeMultiple(
        const std::vector<std::string>& messages,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::size_t, std::error_code>;

    /**
     * @brief Reads data from the FIFO.
     *
     * @param maxSize Maximum number of bytes to read
     * @param timeout Optional timeout for the read operation, in milliseconds.
     *                If not provided, the default from configuration is used.
     * @return type::expected<std::string, std::error_code> The data read or an
     * error
     *
     * This method will read data from the FIFO. If a timeout is specified,
     * it will return an error if the operation cannot complete within the
     * specified time.
     */
    auto read(std::size_t maxSize = 0,  // 0 means use config.read_buffer_size
              std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<std::string, std::error_code>;

    /**
     * @brief Reads data from the FIFO asynchronously.
     *
     * @param callback Function to call when the read completes or fails
     * @param maxSize Maximum number of bytes to read
     * @param timeout Optional timeout for the read operation
     * @return int Identifier for the asynchronous operation
     */
    int readAsync(
        OperationCallback callback,
        std::size_t maxSize = 0,  // 0 means use config.read_buffer_size
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Reads data from the FIFO asynchronously with future.
     *
     * @param maxSize Maximum number of bytes to read
     * @param timeout Optional timeout for the read operation
     * @return std::future<type::expected<std::string, std::error_code>> Future
     * with result
     */
    std::future<type::expected<std::string, std::error_code>>
    readAsyncWithFuture(
        std::size_t maxSize = 0,  // 0 means use config.read_buffer_size
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Checks if the FIFO is currently open.
     *
     * @return true if the FIFO is open, false otherwise.
     *
     * This method can be used to determine if the FIFO client is ready for
     * operations.
     */
    [[nodiscard]] auto isOpen() const noexcept -> bool;

    /**
     * @brief Gets the path of the FIFO.
     *
     * @return std::string_view The path of the FIFO.
     */
    [[nodiscard]] auto getPath() const noexcept -> std::string_view;

    /**
     * @brief Opens the FIFO connection.
     *
     * @param timeout Optional timeout for the open operation
     * @return type::expected<void, std::error_code> Success or error
     *
     * This method is useful for reconnecting after a connection was closed.
     */
    auto open(std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        -> type::expected<void, std::error_code>;

    /**
     * @brief Closes the FIFO.
     *
     * This method closes the FIFO and releases any associated resources.
     * It is good practice to call this when you are done using the FIFO
     * to ensure proper cleanup.
     */
    void close() noexcept;

    /**
     * @brief Registers a callback for connection status changes.
     *
     * @param callback Function to call when connection status changes
     * @return int Identifier for the callback registration
     */
    int registerConnectionCallback(ConnectionCallback callback);

    /**
     * @brief Unregisters a previously registered connection callback.
     *
     * @param id The identifier returned by registerConnectionCallback
     * @return bool True if callback was successfully unregistered
     */
    bool unregisterConnectionCallback(int id);

    /**
     * @brief Gets the current client configuration.
     *
     * @return The current client configuration
     */
    [[nodiscard]] ClientConfig getConfig() const;

    /**
     * @brief Updates the client configuration.
     *
     * @param config New configuration settings
     * @return True if configuration was updated successfully
     */
    bool updateConfig(const ClientConfig& config);

    /**
     * @brief Gets current client statistics.
     *
     * @return Statistics about client operation
     */
    [[nodiscard]] ClientStats getStatistics() const;

    /**
     * @brief Resets client statistics.
     */
    void resetStatistics();

    /**
     * @brief Cancels an ongoing asynchronous operation.
     *
     * @param id The identifier of the operation to cancel
     * @return bool True if operation was successfully canceled
     */
    bool cancelOperation(int id);

private:
    struct Impl;  ///< Forward declaration of the implementation details.
    std::unique_ptr<Impl> m_impl;  ///< Pointer to the implementation, using
                                   ///< PImpl idiom for encapsulation.
};

// Template implementation
template <WritableData T>
auto FifoClient::write(const T& data,
                       std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::size_t, std::error_code> {
    return write(std::string_view(reinterpret_cast<const char*>(data.data()),
                                  data.size()),
                 timeout);
}

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFOCLIENT_HPP