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
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

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

#include "spdlog/sinks/stdout_color_sinks.h"

namespace atom::connection {

/**
 * @brief Gets or creates the spdlog logger for the FIFO server.
 * @return A shared pointer to the spdlog logger.
 */
std::shared_ptr<spdlog::logger> get_server_logger() {
    static std::shared_ptr<spdlog::logger> server_logger;
    if (!server_logger) {
        auto console_sink =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        server_logger =
            std::make_shared<spdlog::logger>("fifo_server", console_sink);
        server_logger->set_level(spdlog::level::info);
    }
    return server_logger;
}

/**
 * @brief Converts LogLevel enum to spdlog level enum.
 * @param level The LogLevel value.
 * @return The corresponding spdlog level enum.
 */
spdlog::level::level_enum to_spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warning:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::None:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
    }
}

/**
 * @brief Structure representing a message with priority and timestamp.
 */
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

    /**
     * @brief Custom comparison for priority queue (higher priority first, then
     * older timestamp).
     */
    bool operator<(const Message& other) const {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return timestamp > other.timestamp;
    }

private:
    static size_t generateMessageId() {
        static std::atomic<size_t> nextId{0};
        return nextId++;
    }
};

/**
 * @brief A simple thread pool for executing tasks.
 */
class ThreadPool {
public:
    /**
     * @brief Constructs a ThreadPool with a specified number of threads.
     * @param num_threads The number of threads in the pool.
     */
    ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex_);
                        this->condition_.wait(lock, [this] {
                            return this->stop_ || !this->tasks_.empty();
                        });
                        if (this->stop_ && this->tasks_.empty())
                            return;
                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    /**
     * @brief Enqueues a task to be executed by the thread pool.
     * @tparam F The type of the function to enqueue.
     * @tparam Args The types of the arguments to the function.
     * @param f The function to enqueue.
     * @param args The arguments to pass to the function.
     * @return A future representing the result of the task.
     */
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return res;
    }

    /**
     * @brief Destroys the ThreadPool, waiting for all tasks to complete.
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::jthread& worker : workers_)
            worker.join();
    }

private:
    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

class FIFOServer::Impl {
public:
    /**
     * @brief Constructs a new FIFOServer object with default configuration.
     *
     * @param fifo_path The path to the FIFO pipe.
     * @param config Custom server configuration.
     * @throws std::invalid_argument If fifo_path is empty.
     * @throws std::runtime_error If FIFO creation fails.
     */
    explicit Impl(std::string_view fifo_path, const ServerConfig& config = {})
        : fifo_path_(fifo_path),
          config_(config),
          stop_server_(false),
          flush_before_stop_(false),
          is_connected_(false),
          reconnect_attempts_(0),
          logger_(get_server_logger()),
          next_callback_id_(0),
          io_pool_(std::thread::hardware_concurrency()) {
        if (fifo_path.empty()) {
            logger_->error("FIFO path cannot be empty");
            throw std::invalid_argument("FIFO path cannot be empty");
        }

        try {
            stats_ = ServerStats{};

            std::filesystem::path path(fifo_path_);
            if (auto parent = path.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }

#ifdef _WIN32
            pipe_handle_ = CreateNamedPipeA(
                fifo_path_.c_str(), PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);

            if (pipe_handle_ == INVALID_HANDLE_VALUE) {
                logger_->error("Failed to create named pipe {}: error {}",
                               fifo_path_, GetLastError());
                throw std::runtime_error(std::format(
                    "Failed to create named pipe: {}", GetLastError()));
            }
#elif __APPLE__ || __linux__
            if (mkfifo(fifo_path_.c_str(), 0666) != 0 && errno != EEXIST) {
                logger_->error("Failed to create FIFO {}: {}", fifo_path_,
                               strerror(errno));
                throw std::runtime_error(
                    std::format("Failed to create FIFO: {}", strerror(errno)));
            }
#endif

            logger_->info("FIFO server initialized at: {}", fifo_path_);
            logger_->set_level(to_spdlog_level(config_.log_level));
        } catch (const std::exception& e) {
            logger_->error("Error initializing FIFO server: {}", e.what());
            throw;
        }
    }

    /**
     * @brief Destroys the FIFOServer object.
     */
    ~Impl() {
        try {
            stop(config_.flush_on_stop);

#ifdef _WIN32
            if (pipe_handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(pipe_handle_);
                pipe_handle_ = INVALID_HANDLE_VALUE;
            }
            DeleteFileA(fifo_path_.c_str());
#elif __APPLE__ || __linux__
            std::filesystem::remove(fifo_path_);
#endif
        } catch (const std::exception& e) {
            logger_->error("Error during FIFO server cleanup: {}", e.what());
        }
    }

    /**
     * @brief Sends a message through the FIFO pipe.
     *
     * @param message The message to be sent.
     * @return True if message was queued successfully, false otherwise.
     */
    bool sendMessage(std::string message) {
        return sendMessage(std::move(message), MessagePriority::Normal);
    }

    /**
     * @brief Sends a message with specified priority.
     *
     * @param message The message to be sent.
     * @param priority The priority level for the message.
     * @return True if message was queued successfully, false otherwise.
     */
    bool sendMessage(std::string message, MessagePriority priority) {
        if (message.empty()) {
            logger_->warn("Attempted to send empty message, ignoring");
            return false;
        }

        if (message.size() > config_.max_message_size) {
            logger_->warn("Message size exceeds limit ({} > {}), rejecting",
                          message.size(), config_.max_message_size);
            return false;
        }

        if (!isRunning()) {
            logger_->warn(
                "Attempted to send message while server is not running");
            return false;
        }

        try {
            if (config_.enable_compression) {
                message = compressMessage(message);
            }

            if (config_.enable_encryption) {
                message = encryptMessage(message);
            }

            {
                std::scoped_lock lock(queue_mutex_);
                if (message_queue_.size() >= config_.max_queue_size) {
                    logger_->warn("Message queue overflow, dropping message");
                    std::scoped_lock stats_lock(stats_mutex_);
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
            logger_->error("Error queueing message: {}", e.what());
            std::scoped_lock stats_lock(stats_mutex_);
            stats_.messages_failed++;
            return false;
        }
    }

    /**
     * @brief Sends a message asynchronously.
     *
     * @param message The message to be sent.
     * @return A future that will contain the result of the send operation (true
     * if queued).
     */
    std::future<bool> sendMessageAsync(std::string message) {
        return sendMessageAsync(std::move(message), MessagePriority::Normal);
    }

    /**
     * @brief Sends a message asynchronously with the specified priority.
     *
     * @param message The message to be sent.
     * @param priority The priority level for the message.
     * @return A future that will contain the result of the send operation (true
     * if queued).
     */
    std::future<bool> sendMessageAsync(std::string message,
                                       MessagePriority priority) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        bool queued = sendMessage(std::move(message), priority);
        promise->set_value(queued);

        return future;
    }

    /**
     * @brief Sends multiple messages from a range
     *
     * @tparam R Range type containing messages
     * @param messages Range of messages to send
     * @return Number of messages successfully queued
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages);

    /**
     * @brief Sends multiple messages with the same priority
     *
     * @tparam R Range type containing messages
     * @param messages Range of messages to send
     * @param priority Priority level for all messages
     * @return Number of messages successfully queued
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages, MessagePriority priority);

    /**
     * @brief Registers a callback for message delivery status
     *
     * @param callback Function to call when a message delivery status changes
     * @return A unique identifier for the callback registration
     */
    int registerMessageCallback(MessageCallback callback) {
        if (!callback) {
            logger_->warn("Attempted to register null message callback");
            return -1;
        }

        std::scoped_lock lock(callback_mutex_);
        int id = next_callback_id_++;
        message_callbacks_[id] = std::move(callback);
        return id;
    }

    /**
     * @brief Unregisters a previously registered message callback
     *
     * @param id The identifier returned by registerMessageCallback
     * @return True if callback was successfully unregistered
     */
    bool unregisterMessageCallback(int id) {
        std::scoped_lock lock(callback_mutex_);
        return message_callbacks_.erase(id) > 0;
    }

    /**
     * @brief Registers a callback for server status changes
     *
     * @param callback Function to call when server status changes
     * @return A unique identifier for the callback registration
     */
    int registerStatusCallback(StatusCallback callback) {
        if (!callback) {
            logger_->warn("Attempted to register null status callback");
            return -1;
        }

        std::scoped_lock lock(callback_mutex_);
        int id = next_callback_id_++;
        status_callbacks_[id] = std::move(callback);
        return id;
    }

    /**
     * @brief Unregisters a previously registered status callback
     *
     * @param id The identifier returned by registerStatusCallback
     * @return True if callback was successfully unregistered
     */
    bool unregisterStatusCallback(int id) {
        std::scoped_lock lock(callback_mutex_);
        return status_callbacks_.erase(id) > 0;
    }

    /**
     * @brief Starts the server.
     *
     * @throws std::runtime_error If server fails to start
     */
    void start() {
        try {
            if (!server_thread_.joinable()) {
                stop_server_ = false;
                server_thread_ = std::jthread([this] { serverLoop(); });
                logger_->info("FIFO server started");

                notifyStatusChange(true);
            } else {
                logger_->warn("Server is already running");
            }
        } catch (const std::exception& e) {
            logger_->error("Failed to start server: {}", e.what());
            throw std::runtime_error(
                std::format("Failed to start server: {}", e.what()));
        }
    }

    /**
     * @brief Stops the server.
     *
     * @param flush_queue If true, processes remaining messages before stopping
     */
    void stop(bool flush_queue = true) {
        try {
            if (server_thread_.joinable()) {
                if (flush_queue) {
                    logger_->info("Flushing message queue before stopping...");
                    std::unique_lock lock(queue_mutex_);
                    flush_before_stop_ = true;
                }

                stop_server_ = true;
                message_cv_.notify_all();
                server_thread_.join();

                flush_before_stop_ = false;
                logger_->info("FIFO server stopped");

                notifyStatusChange(false);
            }
        } catch (const std::exception& e) {
            logger_->error("Error stopping server: {}", e.what());
        }
    }

    /**
     * @brief Clears all pending messages from the queue.
     *
     * @return Number of messages cleared
     */
    size_t clearQueue() {
        std::scoped_lock lock(queue_mutex_);
        size_t count = message_queue_.size();

        std::priority_queue<Message> empty_queue;
        std::swap(message_queue_, empty_queue);

        stats_.current_queue_size = 0;
        logger_->info("Message queue cleared, {} messages removed", count);

        return count;
    }

    /**
     * @brief Checks if the server is running.
     *
     * @return True if the server is running, false otherwise.
     */
    [[nodiscard]] bool isRunning() const {
        return server_thread_.joinable() && !stop_server_;
    }

    /**
     * @brief Gets the path of the FIFO pipe.
     *
     * @return The FIFO path as a string
     */
    [[nodiscard]] std::string getFifoPath() const { return fifo_path_; }

    /**
     * @brief Gets the current configuration.
     *
     * @return The current server configuration
     */
    [[nodiscard]] ServerConfig getConfig() const {
        std::scoped_lock lock(config_mutex_);
        return config_;
    }

    /**
     * @brief Updates the server configuration.
     *
     * @param config New configuration settings
     * @return True if configuration was updated successfully
     */
    bool updateConfig(const ServerConfig& config) {
        std::scoped_lock lock(config_mutex_);

        config_.log_level = config.log_level;
        logger_->set_level(to_spdlog_level(config.log_level));

        config_.max_message_size = config.max_message_size;
        config_.enable_compression = config.enable_compression;
        config_.enable_encryption = config.enable_encryption;
        config_.auto_reconnect = config.auto_reconnect;
        config_.max_reconnect_attempts = config.max_reconnect_attempts;
        config_.reconnect_delay = config.reconnect_delay;
        config_.message_ttl = config.message_ttl;

        if (config.max_queue_size > config_.max_queue_size) {
            config_.max_queue_size = config.max_queue_size;
        } else if (config.max_queue_size < config_.max_queue_size) {
            logger_->warn(
                "Cannot decrease max_queue_size while server is running");
        }

        config_.flush_on_stop = config.flush_on_stop;

        logger_->info("Server configuration updated");
        return true;
    }

    /**
     * @brief Gets current server statistics.
     *
     * @return Statistics about server operation
     */
    [[nodiscard]] ServerStats getStatistics() const {
        std::scoped_lock lock(stats_mutex_);
        return stats_;
    }

    /**
     * @brief Resets server statistics.
     */
    void resetStatistics() {
        std::scoped_lock lock(stats_mutex_);
        stats_ = ServerStats{};
        std::scoped_lock queue_lock(queue_mutex_);
        stats_.current_queue_size = message_queue_.size();
        logger_->info("Server statistics reset");
    }

    /**
     * @brief Sets the log level for the server.
     *
     * @param level New log level
     */
    void setLogLevel(LogLevel level) {
        std::scoped_lock lock(config_mutex_);
        config_.log_level = level;
        logger_->set_level(to_spdlog_level(level));
    }

    /**
     * @brief Gets the current number of messages in the queue.
     *
     * @return Current queue size
     */
    [[nodiscard]] size_t getQueueSize() const {
        std::scoped_lock lock(queue_mutex_);
        return message_queue_.size();
    }

private:
    /**
     * @brief The main server loop that processes the message queue.
     */
    void serverLoop() {
        logger_->debug("Server loop started");

        while (!stop_server_ ||
               (flush_before_stop_ && !message_queue_.empty())) {
            Message message{""};
            bool has_message = false;

            {
                std::unique_lock lock(queue_mutex_);

                auto waitResult = message_cv_.wait_for(
                    lock, std::chrono::seconds(1),
                    [this] { return stop_server_ || !message_queue_.empty(); });

                if (!waitResult) {
                    continue;
                }

                if (!message_queue_.empty()) {
                    if (config_.message_ttl.has_value()) {
                        auto now = std::chrono::steady_clock::now();

                        while (!message_queue_.empty()) {
                            const auto& top = message_queue_.top();
                            auto age = std::chrono::duration_cast<
                                std::chrono::milliseconds>(now - top.timestamp);

                            if (age > config_.message_ttl.value()) {
                                logger_->debug(
                                    "Message expired, discarding (age: {} ms)",
                                    age.count());
                                message_queue_.pop();
                                std::scoped_lock stats_lock(stats_mutex_);
                                stats_.messages_failed++;
                                stats_.current_queue_size =
                                    message_queue_.size();
                            } else {
                                break;
                            }
                        }
                    }

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
                io_pool_.enqueue([this, msg = std::move(message)]() mutable {
                    auto start_time = std::chrono::steady_clock::now();
                    bool success = false;

                    ServerConfig current_config = getConfig();

                    for (int retry = 0;
                         retry < current_config.max_reconnect_attempts;
                         ++retry) {
                        try {
#ifdef _WIN32
                            HANDLE pipe =
                                CreateFileA(fifo_path_.c_str(), GENERIC_WRITE,
                                            0, NULL, OPEN_EXISTING, 0, NULL);
                            if (pipe != INVALID_HANDLE_VALUE) {
                                if (!is_connected_) {
                                    is_connected_ = true;
                                    reconnect_attempts_ = 0;
                                    notifyStatusChange(true);
                                }

                                DWORD bytes_written = 0;
                                BOOL write_success = WriteFile(
                                    pipe, msg.content.c_str(),
                                    static_cast<DWORD>(msg.content.length()),
                                    &bytes_written, NULL);
                                CloseHandle(pipe);

                                if (!write_success) {
                                    throw std::system_error(
                                        GetLastError(), std::system_category(),
                                        "Failed to write to pipe");
                                }

                                if (static_cast<size_t>(bytes_written) !=
                                    msg.content.length()) {
                                    logger_->warn(
                                        "Partial write to pipe: {} of {} bytes",
                                        bytes_written, msg.content.length());
                                }

                                success = true;
                                break;
                            } else {
                                auto error = GetLastError();
                                if (is_connected_) {
                                    is_connected_ = false;
                                    notifyStatusChange(false);
                                }

                                throw std::system_error(
                                    error, std::system_category(),
                                    "Failed to open pipe for writing");
                            }
#elif __APPLE__ || __linux__
                            int fd =
                                open(fifo_path_.c_str(), O_WRONLY | O_NONBLOCK);
                            if (fd == -1) {
                                fd = open(fifo_path_.c_str(), O_WRONLY);
                            }

                            if (fd != -1) {
                                if (!is_connected_) {
                                    is_connected_ = true;
                                    reconnect_attempts_ = 0;
                                    notifyStatusChange(true);
                                }

                                ssize_t bytes_written =
                                    write(fd, msg.content.c_str(),
                                          msg.content.length());
                                close(fd);

                                if (bytes_written == -1) {
                                    throw std::system_error(
                                        errno, std::system_category(),
                                        "Failed to write to FIFO");
                                }

                                if (static_cast<size_t>(bytes_written) !=
                                    msg.content.length()) {
                                    logger_->warn(
                                        "Partial write to FIFO: {} of {} bytes",
                                        bytes_written, msg.content.length());
                                }

                                success = true;
                                break;
                            } else {
                                if (is_connected_) {
                                    is_connected_ = false;
                                    notifyStatusChange(false);
                                }

                                throw std::system_error(
                                    errno, std::system_category(),
                                    "Failed to open FIFO for writing");
                            }
#endif
                        } catch (const std::exception& e) {
                            logger_->warn(
                                "Error writing message (attempt {} of {}): {}",
                                retry + 1,
                                current_config.max_reconnect_attempts,
                                e.what());

                            reconnect_attempts_++;

                            if (retry <
                                    current_config.max_reconnect_attempts - 1 &&
                                current_config.auto_reconnect) {
                                std::this_thread::sleep_for(
                                    current_config.reconnect_delay);
                            }
                        }
                    }

                    auto end_time = std::chrono::steady_clock::now();
                    auto latency =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time)
                            .count();

                    {
                        std::scoped_lock stats_lock(stats_mutex_);
                        if (success) {
                            stats_.messages_sent++;
                            stats_.bytes_sent += msg.content.size();

                            if (stats_.messages_sent == 1) {
                                stats_.avg_message_size =
                                    static_cast<double>(msg.content.size());
                            } else {
                                stats_.avg_message_size =
                                    ((stats_.avg_message_size *
                                      (stats_.messages_sent - 1)) +
                                     msg.content.size()) /
                                    stats_.messages_sent;
                            }

                            if (stats_.messages_sent == 1) {
                                stats_.avg_latency_ms =
                                    static_cast<double>(latency);
                            } else {
                                stats_.avg_latency_ms =
                                    ((stats_.avg_latency_ms *
                                      (stats_.messages_sent - 1)) +
                                     latency) /
                                    stats_.messages_sent;
                            }

                        } else {
                            stats_.messages_failed++;
                        }
                    }

                    notifyMessageStatus(msg.content, success);
                });
            }
        }

        logger_->debug("Server loop exited");
    }

    /**
     * @brief Notifies registered message status callbacks.
     * @param message The message content.
     * @param success True if the message was sent successfully, false
     * otherwise.
     */
    void notifyMessageStatus(const std::string& message, bool success) {
        std::scoped_lock lock(callback_mutex_);
        for (const auto& [id, callback] : message_callbacks_) {
            try {
                callback(message, success);
            } catch (const std::exception& e) {
                logger_->error("Error in message callback {}: {}", id,
                               e.what());
            }
        }
    }

    /**
     * @brief Notifies registered server status callbacks.
     * @param connected True if the server is connected, false otherwise.
     */
    void notifyStatusChange(bool connected) {
        std::scoped_lock lock(callback_mutex_);
        for (const auto& [id, callback] : status_callbacks_) {
            try {
                callback(connected);
            } catch (const std::exception& e) {
                logger_->error("Error in status callback {}: {}", id, e.what());
            }
        }
    }

    /**
     * @brief Compresses the message content if compression is enabled.
     * @param message The message content to compress.
     * @return The compressed or original message content.
     */
    std::string compressMessage(const std::string& message) {
#ifdef ENABLE_COMPRESSION
        if (message.empty())
            return "";
        if (message.size() < 128) {
            return "NC:" + message;
        }

        z_stream zs{};
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;

        if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
            logger_->error("Failed to initialize zlib for compression");
            return "NC:" + message;
        }

        zs.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(message.data()));
        zs.avail_in = static_cast<uInt>(message.size());

        size_t outsize = deflateBound(&zs, message.size());
        std::string outstring(outsize, '\0');

        zs.next_out = reinterpret_cast<Bytef*>(outstring.data());
        zs.avail_out = static_cast<uInt>(outsize);

        int result = deflate(&zs, Z_FINISH);
        deflateEnd(&zs);

        if (result != Z_STREAM_END) {
            logger_->error("Error during compression: {}",
                           zs.msg ? zs.msg : "unknown error");
            return "NC:" + message;
        }

        outstring.resize(zs.total_out);

        if (outstring.size() < message.size()) {
            logger_->debug("Compressed message from {} to {} bytes",
                           message.size(), outstring.size());
            return "C:" + outstring;
        } else {
            logger_->debug(
                "Compression did not reduce size ({} vs {}), sending "
                "uncompressed",
                message.size(), outstring.size());
            return "NC:" + message;
        }

#else
        return message;
#endif
    }

    /**
     * @brief Encrypts the message content if encryption is enabled.
     * @param message The message content to encrypt.
     * @return The encrypted or original message content.
     */
    std::string encryptMessage(const std::string& message) {
#ifdef ENABLE_ENCRYPTION
        logger_->warn(
            "Encryption is enabled but using a placeholder implementation.");

        std::string key = "ThisIsASecretKey";

        std::string encrypted(message.size(), '\0');
        for (size_t i = 0; i < message.size(); ++i) {
            encrypted[i] = message[i] ^ key[i % key.size()];
        }

        return "E:" + encrypted;
#else
        return message;
#endif
    }

    std::string fifo_path_;
    ServerConfig config_;
    mutable std::mutex config_mutex_;
    std::atomic_bool stop_server_;
    std::atomic_bool flush_before_stop_{false};
    std::atomic_bool is_connected_;
    std::atomic<int> reconnect_attempts_;
    std::jthread server_thread_;
    std::priority_queue<Message> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable message_cv_;
    ServerStats stats_;
    mutable std::mutex stats_mutex_;
    std::shared_ptr<spdlog::logger> logger_;

    std::mutex callback_mutex_;
    std::unordered_map<int, MessageCallback> message_callbacks_;
    std::unordered_map<int, StatusCallback> status_callbacks_;
    std::atomic<int> next_callback_id_;

    ThreadPool io_pool_;

#ifdef _WIN32
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
#endif
};

template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
size_t FIFOServer::Impl::sendMessages(R&& messages) {
    return sendMessages(std::forward<R>(messages), MessagePriority::Normal);
}

template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
size_t FIFOServer::Impl::sendMessages(R&& messages, MessagePriority priority) {
    size_t count = 0;
    if (!isRunning()) {
        logger_->warn("Attempted to send messages while server is not running");
        return 0;
    }

    try {
        std::vector<Message> prepared_messages;
        prepared_messages.reserve(std::ranges::distance(messages));

        for (auto&& msg_val : messages) {
            std::string msg = std::string(msg_val);

            if (msg.empty()) {
                continue;
            }

            if (msg.size() > config_.max_message_size) {
                logger_->warn("Message size exceeds limit ({} > {}), skipping",
                              msg.size(), config_.max_message_size);
                std::scoped_lock stats_lock(stats_mutex_);
                stats_.messages_failed++;
                continue;
            }

            std::string processed_msg = msg;

            if (config_.enable_compression) {
                processed_msg = compressMessage(processed_msg);
            }

            if (config_.enable_encryption) {
                processed_msg = encryptMessage(processed_msg);
            }

            prepared_messages.emplace_back(std::move(processed_msg), priority);
        }

        std::scoped_lock lock(queue_mutex_);

        size_t space_available = config_.max_queue_size - message_queue_.size();
        size_t msgs_to_queue =
            std::min(prepared_messages.size(), space_available);

        if (msgs_to_queue < prepared_messages.size()) {
            logger_->warn("Message queue near capacity, dropping {} messages",
                          prepared_messages.size() - msgs_to_queue);
            std::scoped_lock stats_lock(stats_mutex_);
            stats_.messages_failed +=
                (prepared_messages.size() - msgs_to_queue);
        }

        for (size_t i = 0; i < msgs_to_queue; ++i) {
            message_queue_.push(std::move(prepared_messages[i]));
            count++;
        }

        stats_.current_queue_size = message_queue_.size();
        stats_.queue_high_watermark =
            std::max(stats_.queue_high_watermark, stats_.current_queue_size);

        if (count > 0) {
            message_cv_.notify_one();
        }
    } catch (const std::exception& e) {
        logger_->error("Error queueing messages: {}", e.what());
    }
    return count;
}

FIFOServer::FIFOServer(std::string_view fifo_path)
    : impl_(std::make_unique<Impl>(fifo_path)) {}

FIFOServer::FIFOServer(std::string_view fifo_path, const ServerConfig& config)
    : impl_(std::make_unique<Impl>(fifo_path, config)) {}

FIFOServer::~FIFOServer() = default;

FIFOServer::FIFOServer(FIFOServer&& other) noexcept = default;
FIFOServer& FIFOServer::operator=(FIFOServer&& other) noexcept = default;

bool FIFOServer::sendMessage(std::string message) {
    if (!impl_)
        return false;
    return impl_->sendMessage(std::move(message));
}

bool FIFOServer::sendMessage(std::string message, MessagePriority priority) {
    if (!impl_)
        return false;
    return impl_->sendMessage(std::move(message), priority);
}

std::future<bool> FIFOServer::sendMessageAsync(std::string message) {
    if (!impl_) {
        auto promise = std::promise<bool>();
        promise.set_value(false);
        return promise.get_future();
    }
    return impl_->sendMessageAsync(std::move(message));
}

std::future<bool> FIFOServer::sendMessageAsync(std::string message,
                                               MessagePriority priority) {
    if (!impl_) {
        auto promise = std::promise<bool>();
        promise.set_value(false);
        return promise.get_future();
    }
    return impl_->sendMessageAsync(std::move(message), priority);
}

int FIFOServer::registerMessageCallback(MessageCallback callback) {
    if (!impl_)
        return -1;
    return impl_->registerMessageCallback(std::move(callback));
}

bool FIFOServer::unregisterMessageCallback(int id) {
    if (!impl_)
        return false;
    return impl_->unregisterMessageCallback(id);
}

int FIFOServer::registerStatusCallback(StatusCallback callback) {
    if (!impl_)
        return -1;
    return impl_->registerStatusCallback(std::move(callback));
}

bool FIFOServer::unregisterStatusCallback(int id) {
    if (!impl_)
        return false;
    return impl_->unregisterStatusCallback(id);
}

void FIFOServer::start() {
    if (impl_)
        impl_->start();
}

void FIFOServer::stop(bool flush_queue) {
    if (impl_)
        impl_->stop(flush_queue);
}

size_t FIFOServer::clearQueue() {
    if (!impl_)
        return 0;
    return impl_->clearQueue();
}

bool FIFOServer::isRunning() const { return impl_ && impl_->isRunning(); }

std::string FIFOServer::getFifoPath() const {
    if (!impl_)
        return "";
    return impl_->getFifoPath();
}

ServerConfig FIFOServer::getConfig() const {
    if (!impl_)
        return {};
    return impl_->getConfig();
}

bool FIFOServer::updateConfig(const ServerConfig& config) {
    if (!impl_)
        return false;
    return impl_->updateConfig(config);
}

ServerStats FIFOServer::getStatistics() const {
    if (!impl_)
        return {};
    return impl_->getStatistics();
}

void FIFOServer::resetStatistics() {
    if (impl_)
        impl_->resetStatistics();
}

void FIFOServer::setLogLevel(LogLevel level) {
    if (impl_)
        impl_->setLogLevel(level);
}

size_t FIFOServer::getQueueSize() const {
    if (!impl_)
        return 0;
    return impl_->getQueueSize();
}

}  // namespace atom::connection
