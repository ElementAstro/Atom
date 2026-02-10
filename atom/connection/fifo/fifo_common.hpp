/*
 * fifo_common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Common types and utilities for FIFO operations

*************************************************/

#ifndef ATOM_CONNECTION_FIFO_COMMON_HPP
#define ATOM_CONNECTION_FIFO_COMMON_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <system_error>

#include "atom/type/expected.hpp"

namespace atom::connection {

/**
 * @brief Error codes specific to FIFO operations
 */
enum class FifoError {
    Success = 0,
    OpenFailed,
    ReadFailed,
    WriteFailed,
    Timeout,
    InvalidOperation,
    NotOpen,
    ConnectionLost,
    MessageTooLarge,
    CompressionFailed,
    DecompressionFailed,
    EncryptionFailed,
    DecryptionFailed,
    QueueFull,
    InvalidArgument,
    NotRunning,
    AlreadyRunning
};

/**
 * @brief Custom error category for FIFO errors
 */
class FifoErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "fifo"; }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<FifoError>(ev)) {
            case FifoError::Success:
                return "Success";
            case FifoError::OpenFailed:
                return "Failed to open FIFO";
            case FifoError::ReadFailed:
                return "Failed to read from FIFO";
            case FifoError::WriteFailed:
                return "Failed to write to FIFO";
            case FifoError::Timeout:
                return "Operation timed out";
            case FifoError::InvalidOperation:
                return "Invalid operation";
            case FifoError::NotOpen:
                return "FIFO is not open";
            case FifoError::ConnectionLost:
                return "Connection lost";
            case FifoError::MessageTooLarge:
                return "Message too large";
            case FifoError::CompressionFailed:
                return "Compression failed";
            case FifoError::DecompressionFailed:
                return "Decompression failed";
            case FifoError::EncryptionFailed:
                return "Encryption failed";
            case FifoError::DecryptionFailed:
                return "Decryption failed";
            case FifoError::QueueFull:
                return "Message queue is full";
            case FifoError::InvalidArgument:
                return "Invalid argument";
            case FifoError::NotRunning:
                return "Server is not running";
            case FifoError::AlreadyRunning:
                return "Server is already running";
            default:
                return "Unknown FIFO error";
        }
    }
};

/**
 * @brief Get the global FIFO error category instance
 */
inline const FifoErrorCategory& fifoErrorCategory() noexcept {
    static FifoErrorCategory instance;
    return instance;
}

/**
 * @brief Create an error_code from FifoError
 */
[[nodiscard]] inline std::error_code make_error_code(FifoError e) noexcept {
    return {static_cast<int>(e), fifoErrorCategory()};
}

/**
 * @brief Enum representing message priority levels
 */
enum class MessagePriority { Low = 0, Normal = 1, High = 2, Critical = 3 };

/**
 * @brief Enum representing different log levels
 */
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3, None = 4 };

/**
 * @brief Base statistics structure for FIFO operations
 */
struct FifoStats {
    size_t messages_sent = 0;
    size_t messages_received = 0;
    size_t messages_failed = 0;
    size_t bytes_sent = 0;
    size_t bytes_received = 0;
    double avg_write_latency_ms = 0.0;
    double avg_read_latency_ms = 0.0;
    double avg_latency_ms = 0.0;
    double avg_message_size = 0.0;
    double avg_compression_ratio = 0.0;
    size_t reconnect_attempts = 0;
    size_t successful_reconnects = 0;
    size_t compression_ratio = 0;
    size_t queue_high_watermark = 0;
    size_t current_queue_size = 0;

    void reset() noexcept { *this = FifoStats{}; }

    void updateWriteLatency(double latency_ms) noexcept {
        if (messages_sent == 0) {
            avg_write_latency_ms = latency_ms;
        } else {
            avg_write_latency_ms =
                (avg_write_latency_ms * (messages_sent - 1) + latency_ms) /
                messages_sent;
        }
    }

    void updateReadLatency(double latency_ms) noexcept {
        if (messages_received == 0) {
            avg_read_latency_ms = latency_ms;
        } else {
            avg_read_latency_ms =
                (avg_read_latency_ms * (messages_received - 1) + latency_ms) /
                messages_received;
        }
    }
};

/**
 * @brief Base configuration for FIFO operations
 */
struct FifoBaseConfig {
    size_t max_message_size = 1024 * 1024;  // 1MB
    size_t buffer_size = 4096;
    bool enable_compression = false;
    size_t compression_threshold = 1024;
    bool enable_encryption = false;
    bool auto_reconnect = true;
    int max_reconnect_attempts = 5;
    std::chrono::milliseconds reconnect_delay{500};
    std::optional<std::chrono::milliseconds> default_timeout{5000};
    LogLevel log_level = LogLevel::Info;
};

/**
 * @brief Client-specific configuration
 */
struct ClientConfig : FifoBaseConfig {
    size_t read_buffer_size = 4096;
};

/**
 * @brief Server-specific configuration
 */
struct ServerConfig : FifoBaseConfig {
    size_t max_queue_size = 1000;
    bool flush_on_stop = true;
    std::optional<std::chrono::milliseconds> message_ttl{};
};

/**
 * @brief Type alias for client statistics (for backward compatibility)
 */
using ClientStats = FifoStats;

/**
 * @brief Type alias for server statistics (for backward compatibility)
 */
using ServerStats = FifoStats;

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
 * @brief Type for message handling callbacks
 */
using MessageCallback = std::function<void(const std::string&, bool success)>;

/**
 * @brief Type for server status change callbacks
 */
using StatusCallback = std::function<void(bool running)>;

/**
 * @brief Type for message receive callbacks (async server)
 */
using MessageHandler = std::function<void(std::string_view data)>;

/**
 * @brief Type for error handling callbacks
 */
using ErrorHandler = std::function<void(const std::error_code& ec)>;

/**
 * @brief Client events for async operations
 */
enum class ClientEvent {
    Connected,
    Disconnected,
};

/**
 * @brief Type for client event callbacks
 */
using ClientHandler = std::function<void(ClientEvent event)>;

/**
 * @brief Result type for FIFO operations
 */
template <typename T>
using FifoResult = type::expected<T, std::error_code>;

/**
 * @brief Void result type for FIFO operations
 */
using FifoVoidResult = type::expected<void, std::error_code>;

}  // namespace atom::connection

// Enable std::error_code integration
namespace std {
template <>
struct is_error_code_enum<atom::connection::FifoError> : true_type {};
}  // namespace std

#endif  // ATOM_CONNECTION_FIFO_COMMON_HPP
