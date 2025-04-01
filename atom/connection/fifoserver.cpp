/*
 * fifoserver.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Server

*************************************************/

#include "fifoserver.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <format>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef ENABLE_COMPRESSION
#include <zlib.h>
#endif

#ifdef ENABLE_ENCRYPTION
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace atom::connection {

// Message structure with priority
struct Message {
    std::string content;
    MessagePriority priority;
    std::chrono::steady_clock::time_point timestamp;
    size_t id;

    Message(std::string content_, MessagePriority priority_)
        : content(std::move(content_)),
          priority(priority_),
          timestamp(std::chrono::steady_clock::now()),
          id(generateMessageId()) {}

    Message(std::string content_)
        : Message(std::move(content_), MessagePriority::Normal) {}

    // Custom comparison for priority queue
    bool operator<(const Message& other) const {
        // First compare by priority (higher priority comes first)
        if (priority != other.priority) {
            return priority < other.priority;
        }
        // Then compare by timestamp (older messages come first)
        return timestamp > other.timestamp;
    }

private:
    static size_t generateMessageId() {
        static std::atomic<size_t> nextId{0};
        return nextId++;
    }
};

// Helper class for logging
class Logger {
public:
    explicit Logger(LogLevel level) : level_(level) {}

    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) const {
        log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) const {
        log(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args) const {
        log(LogLevel::Warning, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) const {
        log(LogLevel::Error, fmt, std::forward<Args>(args)...);
    }

    void setLevel(LogLevel level) { level_ = level; }

private:
    template <typename... Args>
    void log(LogLevel msg_level, std::format_string<Args...> fmt,
             Args&&... args) const {
        if (msg_level >= level_) {
            auto timestamp = getCurrentTimeString();
            auto level_str = levelToString(msg_level);
            auto message = std::format(fmt, std::forward<Args>(args)...);

            std::cerr << std::format("[{}] {} - {}\n", timestamp, level_str,
                                     message);
        }
    }

    std::string getCurrentTimeString() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::array<char, 24> buffer{};
        std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&time_t_now));

        return std::format("{}.{:03d}", buffer.data(), ms.count());
    }

    const char* levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::Debug:
                return "DEBUG";
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warning:
                return "WARNING";
            case LogLevel::Error:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }

    LogLevel level_;
};

class FIFOServer::Impl {
public:
    explicit Impl(std::string_view fifo_path, const ServerConfig& config = {})
        : fifo_path_(fifo_path),
          config_(config),
          stop_server_(false),
          is_connected_(false),
          reconnect_attempts_(0),
          logger_(config.log_level),
          next_callback_id_(0) {
        if (fifo_path.empty()) {
            throw std::invalid_argument("FIFO path cannot be empty");
        }

        try {
            // Initialize statistics
            stats_ = ServerStats{};

            // Create directory path if it doesn't exist
            std::filesystem::path path(fifo_path_);
            if (auto parent = path.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }

            // Create FIFO file with error handling
#ifdef _WIN32
            pipe_handle_ = CreateNamedPipeA(
                fifo_path_.c_str(), PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);

            if (pipe_handle_ == INVALID_HANDLE_VALUE) {
                throw std::runtime_error(std::format(
                    "Failed to create named pipe: {}", GetLastError()));
            }
#elif __APPLE__ || __linux__
            if (mkfifo(fifo_path_.c_str(), 0666) != 0 && errno != EEXIST) {
                throw std::runtime_error(
                    std::format("Failed to create FIFO: {}", strerror(errno)));
            }
#endif

            logger_.info("FIFO server initialized at: {}", fifo_path_);
        } catch (const std::exception& e) {
            logger_.error("Error initializing FIFO server: {}", e.what());
            throw;  // Re-throw to notify client code
        }
    }

    ~Impl() {
        try {
            stop(config_.flush_on_stop);

#ifdef _WIN32
            if (pipe_handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe_handle_);
                pipe_handle_ = INVALID_HANDLE_VALUE;
            }
            // Attempt to delete the named pipe
            DeleteFileA(fifo_path_.c_str());
#elif __APPLE__ || __linux__
            // Remove the FIFO file if it exists
            std::filesystem::remove(fifo_path_);
#endif
        } catch (const std::exception& e) {
            logger_.error("Error during FIFO server cleanup: {}", e.what());
        }
    }

    bool sendMessage(std::string message) {
        return sendMessage(std::move(message), MessagePriority::Normal);
    }

    bool sendMessage(std::string message, MessagePriority priority) {
        // Validate message
        if (message.empty()) {
            logger_.warning("Attempted to send empty message, ignoring");
            return false;
        }

        if (message.size() > config_.max_message_size) {
            logger_.warning("Message size exceeds limit ({} > {}), rejecting",
                            message.size(), config_.max_message_size);
            return false;
        }

        if (!isRunning()) {
            logger_.warning(
                "Attempted to send message while server is not running");
            return false;
        }

        try {
            // Process message before queuing if needed
            if (config_.enable_compression) {
                message = compressMessage(message);
            }

            if (config_.enable_encryption) {
                message = encryptMessage(message);
            }

            // Use move semantics consistently
            {
                std::scoped_lock lock(queue_mutex_);
                // Limit queue size to prevent memory issues
                if (message_queue_.size() >= config_.max_queue_size) {
                    logger_.warning("Message queue overflow, dropping message");
                    stats_.messages_failed++;
                    return false;
                }

                message_queue_.emplace(std::move(message), priority);
                stats_.current_queue_size = message_queue_.size();
                stats_.queue_high_watermark = std::max(
                    stats_.queue_high_watermark, stats_.current_queue_size);
            }
            message_cv_.notify_one();
            return true;
        } catch (const std::exception& e) {
            logger_.error("Error queueing message: {}", e.what());
            stats_.messages_failed++;
            return false;
        }
    }

    std::future<bool> sendMessageAsync(std::string message) {
        return sendMessageAsync(std::move(message), MessagePriority::Normal);
    }

    std::future<bool> sendMessageAsync(std::string message,
                                       MessagePriority priority) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        // Use a separate thread to send the message
        std::thread([this, message = std::move(message), priority,
                     promise]() mutable {
            bool result = this->sendMessage(std::move(message), priority);
            promise->set_value(result);
        }).detach();

        return future;
    }

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages) {
        return sendMessages(std::forward<R>(messages), MessagePriority::Normal);
    }

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages, MessagePriority priority) {
        size_t count = 0;
        try {
            // Prepare all messages first
            std::vector<Message> prepared_messages;
            prepared_messages.reserve(
                std::distance(std::begin(messages), std::end(messages)));

            for (auto&& msg : messages) {
                // Skip empty messages
                if (msg.empty()) {
                    continue;
                }

                // Skip messages that are too large
                if (msg.size() > config_.max_message_size) {
                    logger_.warning(
                        "Message size exceeds limit ({} > {}), skipping",
                        msg.size(), config_.max_message_size);
                    continue;
                }

                std::string processed_msg = std::string(msg);

                // Process message if needed
                if (config_.enable_compression) {
                    processed_msg = compressMessage(processed_msg);
                }

                if (config_.enable_encryption) {
                    processed_msg = encryptMessage(processed_msg);
                }

                prepared_messages.emplace_back(std::move(processed_msg),
                                               priority);
            }

            // Now queue all the messages at once under a single lock
            std::scoped_lock lock(queue_mutex_);

            // Check how many messages we can actually queue
            size_t space_available =
                config_.max_queue_size - message_queue_.size();
            size_t msgs_to_queue =
                std::min(prepared_messages.size(), space_available);

            if (msgs_to_queue < prepared_messages.size()) {
                logger_.warning(
                    "Message queue near capacity, dropping {} messages",
                    prepared_messages.size() - msgs_to_queue);
                stats_.messages_failed +=
                    (prepared_messages.size() - msgs_to_queue);
            }

            // Queue the messages
            for (size_t i = 0; i < msgs_to_queue; ++i) {
                message_queue_.push(std::move(prepared_messages[i]));
                count++;
            }

            stats_.current_queue_size = message_queue_.size();
            stats_.queue_high_watermark = std::max(stats_.queue_high_watermark,
                                                   stats_.current_queue_size);

            if (count > 0) {
                message_cv_.notify_one();
            }
        } catch (const std::exception& e) {
            logger_.error("Error queueing messages: {}", e.what());
        }
        return count;
    }

    int registerMessageCallback(MessageCallback callback) {
        if (!callback) {
            logger_.warning("Attempted to register null message callback");
            return -1;
        }

        std::scoped_lock lock(callback_mutex_);
        int id = next_callback_id_++;
        message_callbacks_[id] = std::move(callback);
        return id;
    }

    bool unregisterMessageCallback(int id) {
        std::scoped_lock lock(callback_mutex_);
        return message_callbacks_.erase(id) > 0;
    }

    int registerStatusCallback(StatusCallback callback) {
        if (!callback) {
            logger_.warning("Attempted to register null status callback");
            return -1;
        }

        std::scoped_lock lock(callback_mutex_);
        int id = next_callback_id_++;
        status_callbacks_[id] = std::move(callback);
        return id;
    }

    bool unregisterStatusCallback(int id) {
        std::scoped_lock lock(callback_mutex_);
        return status_callbacks_.erase(id) > 0;
    }

    void start() {
        try {
            if (!server_thread_.joinable()) {
                stop_server_ = false;
                server_thread_ = std::jthread([this] { serverLoop(); });
                logger_.info("FIFO server started");

                // Notify status listeners
                notifyStatusChange(true);
            } else {
                logger_.warning("Server is already running");
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::format("Failed to start server: {}", e.what()));
        }
    }

    void stop(bool flush_queue = true) {
        try {
            if (server_thread_.joinable()) {
                if (flush_queue) {
                    logger_.info("Flushing message queue before stopping...");
                    // Set the stop flag but allow the queue to be processed
                    std::unique_lock lock(queue_mutex_);
                    flush_before_stop_ = true;
                }

                stop_server_ = true;
                message_cv_.notify_all();
                server_thread_.join();

                // Reset the flag for next start
                flush_before_stop_ = false;
                logger_.info("FIFO server stopped");

                // Notify status listeners
                notifyStatusChange(false);
            }
        } catch (const std::exception& e) {
            logger_.error("Error stopping server: {}", e.what());
        }
    }

    size_t clearQueue() {
        std::scoped_lock lock(queue_mutex_);
        size_t count = message_queue_.size();

        // Create an empty priority queue with the same comparison
        std::priority_queue<Message> empty_queue;
        std::swap(message_queue_, empty_queue);

        stats_.current_queue_size = 0;
        logger_.info("Message queue cleared, {} messages removed", count);

        return count;
    }

    [[nodiscard]] bool isRunning() const {
        return server_thread_.joinable() && !stop_server_;
    }

    [[nodiscard]] std::string getFifoPath() const { return fifo_path_; }

    [[nodiscard]] ServerConfig getConfig() const { return config_; }

    bool updateConfig(const ServerConfig& config) {
        // Some config options can be updated while running
        config_.log_level = config.log_level;
        logger_.setLevel(config.log_level);

        config_.max_message_size = config.max_message_size;
        config_.enable_compression = config.enable_compression;
        config_.enable_encryption = config.enable_encryption;
        config_.auto_reconnect = config.auto_reconnect;
        config_.max_reconnect_attempts = config.max_reconnect_attempts;
        config_.reconnect_delay = config.reconnect_delay;
        config_.message_ttl = config.message_ttl;

        // The max_queue_size can only be increased while running, not decreased
        if (config.max_queue_size > config_.max_queue_size) {
            config_.max_queue_size = config.max_queue_size;
        } else if (config.max_queue_size < config_.max_queue_size) {
            logger_.warning(
                "Cannot decrease max_queue_size while server is running");
        }

        // flush_on_stop can be updated anytime
        config_.flush_on_stop = config.flush_on_stop;

        logger_.info("Server configuration updated");
        return true;
    }

    [[nodiscard]] ServerStats getStatistics() const {
        std::scoped_lock lock(queue_mutex_);
        return stats_;
    }

    void resetStatistics() {
        std::scoped_lock lock(queue_mutex_);
        stats_ = ServerStats{};
        stats_.current_queue_size = message_queue_.size();
        logger_.info("Server statistics reset");
    }

    void setLogLevel(LogLevel level) {
        config_.log_level = level;
        logger_.setLevel(level);
    }

    [[nodiscard]] size_t getQueueSize() const {
        std::scoped_lock lock(queue_mutex_);
        return message_queue_.size();
    }

private:
    void serverLoop() {
        logger_.debug("Server loop started");

        while (!stop_server_ ||
               (flush_before_stop_ && !message_queue_.empty())) {
            Message message{""};
            bool has_message = false;

            {
                std::unique_lock lock(queue_mutex_);

                // Wait for a message or timeout
                auto waitResult = message_cv_.wait_for(
                    lock, std::chrono::seconds(1),
                    [this] { return stop_server_ || !message_queue_.empty(); });

                if (!waitResult) {
                    // Timeout occurred, loop back to check stop_server_ again
                    continue;
                }

                if (!message_queue_.empty()) {
                    // If we have a TTL configured, check for expired messages
                    if (config_.message_ttl.has_value()) {
                        auto now = std::chrono::steady_clock::now();

                        // Keep popping expired messages
                        while (!message_queue_.empty()) {
                            const auto& top = message_queue_.top();
                            auto age = std::chrono::duration_cast<
                                std::chrono::milliseconds>(now - top.timestamp);

                            if (age > config_.message_ttl.value()) {
                                logger_.debug(
                                    "Message expired, discarding (age: {} ms)",
                                    age.count());
                                message_queue_.pop();
                                stats_.messages_failed++;
                                stats_.current_queue_size =
                                    message_queue_.size();
                            } else {
                                break;
                            }
                        }
                    }

                    // Check again if we have messages after TTL processing
                    if (!message_queue_.empty()) {
                        message = std::move(
                            const_cast<Message&>(message_queue_.top()));
                        message_queue_.pop();
                        stats_.current_queue_size = message_queue_.size();
                        has_message = true;
                    }
                }
            }

            if (has_message && !message.content.empty()) {
                bool success = writeMessage(message.content);

                // Update statistics
                if (success) {
                    stats_.messages_sent++;
                    stats_.bytes_sent += message.content.size();

                    // Update average message size
                    if (stats_.messages_sent == 1) {
                        stats_.avg_message_size =
                            static_cast<double>(message.content.size());
                    } else {
                        stats_.avg_message_size =
                            ((stats_.avg_message_size *
                              (stats_.messages_sent - 1)) +
                             message.content.size()) /
                            stats_.messages_sent;
                    }
                } else {
                    stats_.messages_failed++;
                }

                // Notify callbacks about message status
                notifyMessageStatus(message.content, success);
            }
        }

        logger_.debug("Server loop exited");
    }

    bool writeMessage(const std::string& message) {
        auto start_time = std::chrono::steady_clock::now();
        bool success = false;

        for (int retry = 0; retry < config_.max_reconnect_attempts; ++retry) {
            try {
#ifdef _WIN32
                HANDLE pipe = CreateFileA(fifo_path_.c_str(), GENERIC_WRITE, 0,
                                          NULL, OPEN_EXISTING, 0, NULL);
                if (pipe != INVALID_HANDLE_VALUE) {
                    if (!is_connected_) {
                        is_connected_ = true;
                        reconnect_attempts_ = 0;
                        notifyStatusChange(true);
                    }

                    DWORD bytes_written = 0;
                    BOOL write_success =
                        WriteFile(pipe, message.c_str(),
                                  static_cast<DWORD>(message.length()),
                                  &bytes_written, NULL);
                    CloseHandle(pipe);

                    if (!write_success) {
                        throw std::system_error(GetLastError(),
                                                std::system_category(),
                                                "Failed to write to pipe");
                    }

                    if (bytes_written != message.length()) {
                        logger_.warning("Partial write to pipe: {} of {} bytes",
                                        bytes_written, message.length());
                    }

                    success = true;
                    break;
                } else {
                    auto error = GetLastError();
                    if (is_connected_) {
                        is_connected_ = false;
                        notifyStatusChange(false);
                    }

                    throw std::system_error(error, std::system_category(),
                                            "Failed to open pipe for writing");
                }
#elif __APPLE__ || __linux__
                // Try with non-blocking first, then blocking if needed
                int fd = open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK);
                if (fd == -1) {
                    // If no reader is available, non-blocking open might fail
                    fd = open(fifo_path_.c_str(), O_WRONLY);
                }

                if (fd != -1) {
                    if (!is_connected_) {
                        is_connected_ = true;
                        reconnect_attempts_ = 0;
                        notifyStatusChange(true);
                    }

                    ssize_t bytes_written =
                        write(fd, message.c_str(), message.length());
                    close(fd);

                    if (bytes_written == -1) {
                        throw std::system_error(errno, std::system_category(),
                                                "Failed to write to FIFO");
                    }

                    if (static_cast<size_t>(bytes_written) !=
                        message.length()) {
                        logger_.warning("Partial write to FIFO: {} of {} bytes",
                                        bytes_written, message.length());
                    }

                    success = true;
                    break;
                } else {
                    if (is_connected_) {
                        is_connected_ = false;
                        notifyStatusChange(false);
                    }

                    throw std::system_error(errno, std::system_category(),
                                            "Failed to open FIFO for writing");
                }
#endif
            } catch (const std::exception& e) {
                logger_.warning("Error writing message (attempt {} of {}): {}",
                                retry + 1, config_.max_reconnect_attempts,
                                e.what());

                reconnect_attempts_++;

                if (retry < config_.max_reconnect_attempts - 1 &&
                    config_.auto_reconnect) {
                    // Wait before retrying
                    std::this_thread::sleep_for(config_.reconnect_delay);
                }
            }
        }

        // Calculate and update latency statistics
        auto end_time = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                           end_time - start_time)
                           .count();

        if (success) {
            // Update average latency
            if (stats_.messages_sent == 1) {
                stats_.avg_latency_ms = static_cast<double>(latency);
            } else {
                stats_.avg_latency_ms =
                    ((stats_.avg_latency_ms * (stats_.messages_sent - 1)) +
                     latency) /
                    stats_.messages_sent;
            }
        }

        return success;
    }

    void notifyMessageStatus(const std::string& message, bool success) {
        std::scoped_lock lock(callback_mutex_);
        for (const auto& [id, callback] : message_callbacks_) {
            try {
                callback(message, success);
            } catch (const std::exception& e) {
                logger_.error("Error in message callback {}: {}", id, e.what());
            }
        }
    }

    void notifyStatusChange(bool connected) {
        std::scoped_lock lock(callback_mutex_);
        for (const auto& [id, callback] : status_callbacks_) {
            try {
                callback(connected);
            } catch (const std::exception& e) {
                logger_.error("Error in status callback {}: {}", id, e.what());
            }
        }
    }

    std::string compressMessage(const std::string& message) {
#ifdef ENABLE_COMPRESSION
        // Skip compression for small messages
        if (message.size() < 128) {
            // Add a marker to indicate not compressed
            return "NC:" + message;
        }

        z_stream zs{};
        if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
            logger_.error("Failed to initialize zlib");
            return message;
        }

        zs.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(message.data()));
        zs.avail_in = static_cast<uInt>(message.size());

        // Estimate the size needed for compressed data
        size_t outsize = message.size() * 1.1 + 12;
        std::string outstring(outsize, '\0');

        zs.next_out = reinterpret_cast<Bytef*>(outstring.data());
        zs.avail_out = static_cast<uInt>(outsize);

        int result = deflate(&zs, Z_FINISH);
        deflateEnd(&zs);

        if (result != Z_STREAM_END) {
            logger_.error("Error during compression: {}", result);
            return message;
        }

        // Resize to actual compressed size
        outstring.resize(zs.total_out);

        // Add a marker to indicate compressed
        return "C:" + outstring;
#else
        // Compression not enabled
        return message;
#endif
    }

    std::string encryptMessage(const std::string& message) {
#ifdef ENABLE_ENCRYPTION
        // Simple XOR encryption as a placeholder
        // In a real application, use a proper cryptographic library

        // Generate a random key
        std::string key(16, '\0');
        RAND_bytes(reinterpret_cast<unsigned char*>(key.data()), key.size());

        // Encrypt the message
        std::string encrypted(message.size(), '\0');
        for (size_t i = 0; i < message.size(); ++i) {
            encrypted[i] = message[i] ^ key[i % key.size()];
        }

        // Prepend the key to the encrypted message
        return "E:" + key + encrypted;
#else
        // Encryption not enabled
        return message;
#endif
    }

    std::string fifo_path_;
    ServerConfig config_;
    std::atomic_bool stop_server_;
    std::atomic_bool flush_before_stop_{false};
    std::atomic_bool is_connected_;
    std::atomic<int> reconnect_attempts_;
    std::jthread server_thread_;
    std::priority_queue<Message> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable message_cv_;
    ServerStats stats_;
    Logger logger_;

    std::mutex callback_mutex_;
    std::unordered_map<int, MessageCallback> message_callbacks_;
    std::unordered_map<int, StatusCallback> status_callbacks_;
    std::atomic<int> next_callback_id_;

#ifdef _WIN32
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
#endif
};

// FIFOServer implementation

FIFOServer::FIFOServer(std::string_view fifo_path)
    : impl_(std::make_unique<Impl>(fifo_path)) {}

FIFOServer::FIFOServer(std::string_view fifo_path, const ServerConfig& config)
    : impl_(std::make_unique<Impl>(fifo_path, config)) {}

FIFOServer::~FIFOServer() = default;

// Move operations
FIFOServer::FIFOServer(FIFOServer&&) noexcept = default;
FIFOServer& FIFOServer::operator=(FIFOServer&&) noexcept = default;

bool FIFOServer::sendMessage(std::string message) {
    return impl_->sendMessage(std::move(message));
}

bool FIFOServer::sendMessage(std::string message, MessagePriority priority) {
    return impl_->sendMessage(std::move(message), priority);
}

std::future<bool> FIFOServer::sendMessageAsync(std::string message) {
    return impl_->sendMessageAsync(std::move(message));
}

std::future<bool> FIFOServer::sendMessageAsync(std::string message,
                                               MessagePriority priority) {
    return impl_->sendMessageAsync(std::move(message), priority);
}

template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
size_t FIFOServer::sendMessages(R&& messages) {
    return impl_->sendMessages(std::forward<R>(messages));
}

template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
size_t FIFOServer::sendMessages(R&& messages, MessagePriority priority) {
    return impl_->sendMessages(std::forward<R>(messages), priority);
}

// Explicit instantiation of common template instances
template size_t FIFOServer::sendMessages(std::vector<std::string>&);
template size_t FIFOServer::sendMessages(const std::vector<std::string>&);
template size_t FIFOServer::sendMessages(std::vector<std::string>&&);

template size_t FIFOServer::sendMessages(std::vector<std::string>&,
                                         MessagePriority);
template size_t FIFOServer::sendMessages(const std::vector<std::string>&,
                                         MessagePriority);
template size_t FIFOServer::sendMessages(std::vector<std::string>&&,
                                         MessagePriority);

int FIFOServer::registerMessageCallback(MessageCallback callback) {
    return impl_->registerMessageCallback(std::move(callback));
}

bool FIFOServer::unregisterMessageCallback(int id) {
    return impl_->unregisterMessageCallback(id);
}

int FIFOServer::registerStatusCallback(StatusCallback callback) {
    return impl_->registerStatusCallback(std::move(callback));
}

bool FIFOServer::unregisterStatusCallback(int id) {
    return impl_->unregisterStatusCallback(id);
}

void FIFOServer::start() { impl_->start(); }

void FIFOServer::stop(bool flush_queue) { impl_->stop(flush_queue); }

size_t FIFOServer::clearQueue() { return impl_->clearQueue(); }

bool FIFOServer::isRunning() const { return impl_->isRunning(); }

std::string FIFOServer::getFifoPath() const { return impl_->getFifoPath(); }

ServerConfig FIFOServer::getConfig() const { return impl_->getConfig(); }

bool FIFOServer::updateConfig(const ServerConfig& config) {
    return impl_->updateConfig(config);
}

ServerStats FIFOServer::getStatistics() const { return impl_->getStatistics(); }

void FIFOServer::resetStatistics() { impl_->resetStatistics(); }

void FIFOServer::setLogLevel(LogLevel level) { impl_->setLogLevel(level); }

size_t FIFOServer::getQueueSize() const { return impl_->getQueueSize(); }

}  // namespace atom::connection