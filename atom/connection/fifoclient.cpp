/*
 * fifoclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Client

*************************************************/

#include "fifoclient.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <ctime>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
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

// Create a custom error category for FIFO operations
class FifoErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "fifo_error";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<FifoError>(ev)) {
            case FifoError::OpenFailed:
                return "Failed to open FIFO pipe";
            case FifoError::ReadFailed:
                return "Failed to read from FIFO pipe";
            case FifoError::WriteFailed:
                return "Failed to write to FIFO pipe";
            case FifoError::Timeout:
                return "Operation timed out";
            case FifoError::InvalidOperation:
                return "Invalid operation on FIFO pipe";
            case FifoError::NotOpen:
                return "FIFO pipe is not open";
            case FifoError::ConnectionLost:
                return "Connection to FIFO pipe was lost";
            case FifoError::MessageTooLarge:
                return "Message exceeds maximum allowed size";
            case FifoError::CompressionFailed:
                return "Failed to compress message";
            case FifoError::EncryptionFailed:
                return "Failed to encrypt message";
            case FifoError::DecryptionFailed:
                return "Failed to decrypt message";
            default:
                return "Unknown FIFO error";
        }
    }
};

// Global instance of the FIFO error category
const FifoErrorCategory theFifoErrorCategory{};

// Helper function to create an error code from a FifoError
[[nodiscard]] std::error_code make_error_code(FifoError e) {
    return {static_cast<int>(e), theFifoErrorCategory};
}

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

            std::cerr << std::format("[{}] FIFO Client {} - {}\n", timestamp,
                                     level_str, message);
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

// Structure to track async operations
struct AsyncOperation {
    enum class Type { Read, Write };
    Type type;
    int id;
    OperationCallback callback;
    std::chrono::steady_clock::time_point start_time;
    std::optional<std::chrono::milliseconds> timeout;
    bool canceled = false;

    AsyncOperation(Type type_, int id_, OperationCallback callback_,
                   std::optional<std::chrono::milliseconds> timeout_)
        : type(type_),
          id(id_),
          callback(std::move(callback_)),
          start_time(std::chrono::steady_clock::now()),
          timeout(timeout_) {}
};

struct FifoClient::Impl {
#ifdef _WIN32
    HANDLE fifoHandle{INVALID_HANDLE_VALUE};
#else
    int fifoFd{-1};
#endif
    std::string fifoPath;
    ClientConfig config;
    ClientStats stats;
    Logger logger;

    // Thread-safety
    mutable std::mutex operationMutex;  // Mutex for synchronous operations
    std::mutex asyncMutex;              // Mutex for async operations
    std::mutex callbackMutex;           // Mutex for callback management

    // Async operation management
    std::atomic<int> nextOperationId{0};
    std::unordered_map<int, AsyncOperation> pendingOperations;
    std::jthread asyncThread;
    std::atomic_bool stopAsyncThread{false};
    std::condition_variable asyncCondition;

    // Connection management
    std::atomic_bool isConnected{false};
    std::atomic<int> reconnectAttempts{0};

    // Connection callbacks
    std::atomic<int> nextCallbackId{0};
    std::unordered_map<int, ConnectionCallback> connectionCallbacks;

    explicit Impl(std::string_view path, const ClientConfig& clientConfig = {})
        : fifoPath(path), config(clientConfig), logger(clientConfig.log_level) {
        try {
            // Initialize statistics
            stats = ClientStats{};

            // Try to open the FIFO
            openFifo();

            // Start the async operation processing thread
            startAsyncThread();

        } catch (const std::exception& e) {
            // Close any resources that might have been opened
            close();
            throw;  // Re-throw the exception
        }
    }

    ~Impl() {
        // Stop the async thread first
        stopAsyncThread = true;
        asyncCondition.notify_all();

        if (asyncThread.joinable()) {
            asyncThread.join();
        }

        // Then close the FIFO
        close();
    }

    void openFifo() {
#ifdef _WIN32
        fifoHandle =
            CreateFileA(fifoPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

        if (fifoHandle == INVALID_HANDLE_VALUE) {
            auto error = GetLastError();
            if (isConnected) {
                isConnected = false;
                notifyConnectionChange(
                    false, std::error_code(error, std::system_category()));
            }

            logger.error("Failed to open FIFO pipe: {}", error);
            throw std::system_error(error, std::system_category(),
                                    "Failed to open FIFO pipe: " + fifoPath);
        }
#else
        // Try to create the FIFO if it doesn't exist
        struct stat st;
        if (stat(fifoPath.c_str(), &st) == -1) {
            if (mkfifo(fifoPath.c_str(), 0666) == -1 && errno != EEXIST) {
                logger.error("Failed to create FIFO pipe: {}", strerror(errno));
                throw std::system_error(
                    errno, std::system_category(),
                    "Failed to create FIFO pipe: " + fifoPath);
            }
        } else if (!S_ISFIFO(st.st_mode)) {
            logger.error("Path exists but is not a FIFO: {}", fifoPath);
            throw std::system_error(
                ENOTSUP, std::system_category(),
                "Path exists but is not a FIFO: " + fifoPath);
        }

        fifoFd = open(fifoPath.c_str(), O_RDWR | O_NONBLOCK);
        if (fifoFd == -1) {
            auto error = errno;
            if (isConnected) {
                isConnected = false;
                notifyConnectionChange(
                    false, std::error_code(error, std::system_category()));
            }

            logger.error("Failed to open FIFO pipe: {}", strerror(error));
            throw std::system_error(error, std::system_category(),
                                    "Failed to open FIFO pipe: " + fifoPath);
        }
#endif

        if (!isConnected) {
            isConnected = true;
            reconnectAttempts = 0;
            notifyConnectionChange(true, {});
            logger.info("Successfully connected to FIFO pipe: {}", fifoPath);
        }
    }

    [[nodiscard]] bool isOpen() const noexcept {
#ifdef _WIN32
        return fifoHandle != INVALID_HANDLE_VALUE;
#else
        return fifoFd != -1;
#endif
    }

    void close() noexcept {
        std::lock_guard<std::mutex> lock(operationMutex);

#ifdef _WIN32
        if (fifoHandle != INVALID_HANDLE_VALUE) {
            // Cancel any pending I/O operations
            CancelIo(fifoHandle);
            CloseHandle(fifoHandle);
            fifoHandle = INVALID_HANDLE_VALUE;
            logger.info("Closed FIFO pipe: {}", fifoPath);
        }
#else
        if (fifoFd != -1) {
            ::close(fifoFd);
            fifoFd = -1;
            logger.info("Closed FIFO pipe: {}", fifoPath);
        }
#endif

        if (isConnected) {
            isConnected = false;
            notifyConnectionChange(false, {});
        }

        // Clear any pending async operations
        {
            std::lock_guard<std::mutex> async_lock(asyncMutex);
            for (auto& [id, op] : pendingOperations) {
                if (op.callback) {
                    try {
                        op.callback(false, make_error_code(FifoError::NotOpen),
                                    0);
                    } catch (const std::exception& e) {
                        logger.error("Exception in async callback: {}",
                                     e.what());
                    }
                }
            }
            pendingOperations.clear();
        }
    }

    auto attemptReconnect(std::optional<std::chrono::milliseconds> timeout)
        -> type::expected<void, std::error_code> {
        if (!config.auto_reconnect) {
            return type::unexpected(make_error_code(FifoError::NotOpen));
        }

        if (reconnectAttempts >= config.max_reconnect_attempts) {
            logger.error("Maximum reconnection attempts reached ({}).",
                         config.max_reconnect_attempts);
            return type::unexpected(make_error_code(FifoError::ConnectionLost));
        }

        logger.info("Attempting to reconnect (attempt {}/{})...",
                    reconnectAttempts.load() + 1,
                    config.max_reconnect_attempts);

        // Close the current connection if it's open
        if (isOpen()) {
            close();
        }

        reconnectAttempts++;
        stats.reconnect_attempts++;

        try {
            // Use timeout if provided, otherwise use default reconnect delay
            auto delay = config.reconnect_delay;

            // If timeout is provided, ensure we don't exceed it
            if (timeout) {
                if (*timeout < delay) {
                    // If timeout is less than the reconnect delay, use timeout
                    delay = *timeout;
                    logger.warning(
                        "Using shorter timeout value ({} ms) for reconnection "
                        "instead of configured delay",
                        delay.count());
                }

                // Reduce the remaining timeout for future operations
                if (*timeout > delay) {
                    *timeout -= delay;
                } else {
                    // If timeout is exhausted, set it to zero
                    *timeout = std::chrono::milliseconds(0);
                }
            }

            // Wait before trying to reconnect
            std::this_thread::sleep_for(delay);

            // Check if we've exhausted the timeout
            if (timeout && timeout->count() <= 0) {
                logger.error("Reconnection timed out");
                return type::unexpected(make_error_code(FifoError::Timeout));
            }

            // Try to open the FIFO again
            openFifo();

            // If we get here, the reconnection was successful
            stats.successful_reconnects++;
            logger.info("Reconnection successful.");
            return {};
        } catch (const std::exception& e) {
            logger.error("Reconnection failed: {}", e.what());
            return type::unexpected(make_error_code(FifoError::ConnectionLost));
        }
    }

    type::expected<std::size_t, std::error_code> write(
        std::string_view data,
        MessagePriority priority = MessagePriority::Normal,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        // Use default timeout from config if not specified
        if (!timeout && config.default_timeout) {
            timeout = config.default_timeout;
        }

        if (data.empty()) {
            return 0;  // Nothing to write
        }

        // Check message size limit
        if (data.size() > config.max_message_size) {
            logger.error("Message size exceeds limit ({} > {})", data.size(),
                         config.max_message_size);
            return type::unexpected(
                make_error_code(FifoError::MessageTooLarge));
        }

        if (!isOpen()) {
            logger.warning("Attempted to write to closed FIFO pipe");

            // Try to reconnect if configured to do so
            if (config.auto_reconnect) {
                auto result = attemptReconnect(timeout);
                if (!result) {
                    return type::unexpected(result.error().error());
                }
            } else {
                return type::unexpected(make_error_code(FifoError::NotOpen));
            }
        }

        // Process the message if needed
        std::string processedData;

        // Add priority header if not normal priority
        if (priority != MessagePriority::Normal) {
            std::string priorityStr;
            switch (priority) {
                case MessagePriority::Low:
                    priorityStr = "LOW";
                    break;
                case MessagePriority::High:
                    priorityStr = "HIGH";
                    break;
                case MessagePriority::Critical:
                    priorityStr = "CRITICAL";
                    break;
                default:
                    break;
            }

            if (!priorityStr.empty()) {
                processedData = "P:" + priorityStr + ":" + std::string(data);
            } else {
                processedData = std::string(data);
            }
        } else {
            processedData = std::string(data);
        }

        // Compress if needed
        if (config.enable_compression &&
            processedData.size() >= config.compression_threshold) {
            processedData = compressData(processedData);
        }

        // Encrypt if needed
        if (config.enable_encryption) {
            processedData = encryptData(processedData);
        }

        std::lock_guard<std::mutex> lock(operationMutex);
        auto start_time = std::chrono::steady_clock::now();

        try {
#ifdef _WIN32
            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (overlapped.hEvent == NULL) {
                auto error = GetLastError();
                logger.error("Failed to create event for FIFO write: {}",
                             error);
                return type::unexpected(
                    std::error_code(error, std::system_category()));
            }

            DWORD bytesWritten = 0;
            bool success = WriteFile(fifoHandle, processedData.data(),
                                     static_cast<DWORD>(processedData.size()),
                                     &bytesWritten, &overlapped);

            // Handle asynchronous operation
            if (!success && GetLastError() == ERROR_IO_PENDING) {
                DWORD waitTime =
                    timeout ? static_cast<DWORD>(timeout->count()) : INFINITE;
                DWORD waitResult =
                    WaitForSingleObject(overlapped.hEvent, waitTime);

                if (waitResult == WAIT_TIMEOUT) {
                    CancelIo(fifoHandle);
                    CloseHandle(overlapped.hEvent);
                    logger.warning("Write operation timed out after {} ms",
                                   timeout ? timeout->count() : 0);
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (waitResult != WAIT_OBJECT_0) {
                    auto error = GetLastError();
                    CloseHandle(overlapped.hEvent);
                    logger.error("Write operation failed: {}", error);
                    return type::unexpected(
                        std::error_code(error, std::system_category()));
                }

                // Get the result of the operation
                if (!GetOverlappedResult(fifoHandle, &overlapped, &bytesWritten,
                                         FALSE)) {
                    auto error = GetLastError();
                    CloseHandle(overlapped.hEvent);

                    // Check if this is a connection loss
                    if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                        isConnected = false;
                        notifyConnectionChange(
                            false,
                            std::error_code(error, std::system_category()));
                        logger.warning(
                            "Connection lost during write operation");

                        // Try to reconnect if configured to do so
                        if (config.auto_reconnect) {
                            auto reconnect_result = attemptReconnect(timeout);
                            if (reconnect_result) {
                                // Try the write operation again
                                CloseHandle(overlapped.hEvent);
                                return write(data, priority, timeout);
                            }
                        }

                        return type::unexpected(
                            make_error_code(FifoError::ConnectionLost));
                    }

                    logger.error("Failed to get overlapped result: {}", error);
                    return type::unexpected(
                        std::error_code(error, std::system_category()));
                }
            } else if (!success) {
                auto error = GetLastError();
                CloseHandle(overlapped.hEvent);

                // Check if this is a connection loss
                if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                    isConnected = false;
                    notifyConnectionChange(
                        false, std::error_code(error, std::system_category()));
                    logger.warning("Connection lost during write operation");

                    // Try to reconnect if configured to do so
                    if (config.auto_reconnect) {
                        auto reconnect_result = attemptReconnect(timeout);
                        if (reconnect_result) {
                            // Try the write operation again
                            return write(data, priority, timeout);
                        }
                    }

                    return type::unexpected(
                        make_error_code(FifoError::ConnectionLost));
                }

                logger.error("Write operation failed: {}", error);
                return type::unexpected(
                    std::error_code(error, std::system_category()));
            }

            CloseHandle(overlapped.hEvent);

            // Update statistics
            updateWriteStats(data.size(), bytesWritten, start_time);

            return static_cast<std::size_t>(bytesWritten);
#else
            if (timeout) {
                pollfd pfd{};
                pfd.fd = fifoFd;
                pfd.events = POLLOUT;

                int pollResult =
                    poll(&pfd, 1, static_cast<int>(timeout->count()));

                if (pollResult == 0) {
                    logger.warning("Write operation timed out after {} ms",
                                   timeout->count());
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (pollResult < 0) {
                    auto error = errno;
                    logger.error("Poll operation failed: {}", strerror(error));

                    // Check if this is a connection-related error
                    if (error == EPIPE || error == ECONNRESET ||
                        error == EBADF) {
                        isConnected = false;
                        notifyConnectionChange(
                            false,
                            std::error_code(error, std::system_category()));
                        logger.warning("Connection lost during poll operation");

                        // Try to reconnect if configured to do so
                        if (config.auto_reconnect) {
                            auto reconnect_result = attemptReconnect(timeout);
                            if (reconnect_result) {
                                // Try the write operation again
                                return write(data, priority, timeout);
                            }
                        }

                        return type::unexpected(
                            make_error_code(FifoError::ConnectionLost));
                    }

                    return type::unexpected(
                        std::error_code(error, std::system_category()));
                }

                if (!(pfd.revents & POLLOUT)) {
                    logger.error("File descriptor not ready for writing");
                    return type::unexpected(
                        make_error_code(FifoError::WriteFailed));
                }
            }

            ssize_t bytesWritten =
                ::write(fifoFd, processedData.data(), processedData.size());

            if (bytesWritten == -1) {
                auto error = errno;
                logger.error("Write operation failed: {}", strerror(error));

                // Check if this is a connection-related error
                if (error == EPIPE || error == ECONNRESET) {
                    isConnected = false;
                    notifyConnectionChange(
                        false, std::error_code(error, std::system_category()));
                    logger.warning("Connection lost during write operation");

                    // Try to reconnect if configured to do so
                    if (config.auto_reconnect) {
                        auto reconnect_result = attemptReconnect(timeout);
                        if (reconnect_result) {
                            // Try the write operation again
                            return write(data, priority, timeout);
                        }
                    }

                    return type::unexpected(
                        make_error_code(FifoError::ConnectionLost));
                }

                return type::unexpected(
                    std::error_code(error, std::system_category()));
            }

            // Update statistics
            updateWriteStats(data.size(), bytesWritten, start_time);

            return static_cast<std::size_t>(bytesWritten);
#endif
        } catch (const std::exception& e) {
            logger.error("Exception during write operation: {}", e.what());
            stats.messages_failed++;
            return type::unexpected(make_error_code(FifoError::WriteFailed));
        }
    }

    type::expected<std::size_t, std::error_code> writeMultiple(
        const std::vector<std::string>& messages,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        if (messages.empty()) {
            return 0;  // Nothing to write
        }

        // Check if we're connected
        if (!isOpen()) {
            logger.warning(
                "Attempted to write multiple messages to closed FIFO pipe");

            // Try to reconnect if configured to do so
            if (config.auto_reconnect) {
                auto result = attemptReconnect(timeout);
                if (!result) {
                    return type::unexpected(result.error().error());
                }
            } else {
                return type::unexpected(make_error_code(FifoError::NotOpen));
            }
        }

        // Write messages one by one
        std::size_t totalBytesWritten = 0;

        for (const auto& message : messages) {
            auto result = write(message, MessagePriority::Normal, timeout);

            if (!result) {
                // If we've written some messages successfully, return partial
                // success
                if (totalBytesWritten > 0) {
                    logger.warning(
                        "Partial success writing multiple messages: {} of {} "
                        "sent",
                        totalBytesWritten, messages.size());
                    return totalBytesWritten;
                }

                return type::unexpected(result.error().error());
            }

            totalBytesWritten += result.value();
        }

        return totalBytesWritten;
    }

    int writeAsync(
        std::string_view data, OperationCallback callback,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        int operationId = nextOperationId++;

        // Store the operation information
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            pendingOperations.emplace(
                operationId,
                AsyncOperation(AsyncOperation::Type::Write, operationId,
                               std::move(callback), timeout));
        }

        // Make a copy of the data for the async thread
        std::string dataCopy(data);

        // Queue the operation in the async thread
        std::thread([this, operationId, dataCopy = std::move(dataCopy),
                     timeout]() {
            // Check if the operation has been canceled
            {
                std::lock_guard<std::mutex> lock(asyncMutex);
                auto it = pendingOperations.find(operationId);
                if (it == pendingOperations.end() || it->second.canceled) {
                    return;  // Operation was canceled
                }
            }

            // Perform the write operation
            auto result =
                this->write(dataCopy, MessagePriority::Normal, timeout);

            // Find the callback and invoke it
            OperationCallback callbackCopy;
            {
                std::lock_guard<std::mutex> lock(asyncMutex);
                auto it = pendingOperations.find(operationId);
                if (it != pendingOperations.end()) {
                    callbackCopy = it->second.callback;
                    pendingOperations.erase(it);
                }
            }

            if (callbackCopy) {
                try {
                    if (result) {
                        callbackCopy(true, {}, result.value());
                    } else {
                        if (callbackCopy) {
                            callbackCopy(
                                false, std::error_code(result.error().error()),
                                0);
                        }
                    }
                } catch (const std::exception& e) {
                    logger.error("Exception in async write callback: {}",
                                 e.what());
                }
            }
        }).detach();

        return operationId;
    }

    std::future<type::expected<std::size_t, std::error_code>>
    writeAsyncWithFuture(
        std::string_view data,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        // Create a promise and future
        auto promise = std::make_shared<
            std::promise<type::expected<std::size_t, std::error_code>>>();
        auto future = promise->get_future();

        // Use a callback to set the promise value
        writeAsync(
            data,
            [promise](bool success, std::error_code error, size_t bytes) {
                if (success) {
                    promise->set_value(bytes);
                } else {
                    promise->set_value(type::unexpected(error));
                }
            },
            timeout);

        return future;
    }

    type::expected<std::string, std::error_code> read(
        std::size_t maxSize,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        // Use default timeout from config if not specified
        if (!timeout && config.default_timeout) {
            timeout = config.default_timeout;
        }

        // Use configured buffer size if not specified or if specified size is
        // too large
        if (maxSize == 0 || maxSize > 1024 * 1024) {
            maxSize = config.read_buffer_size;
        }

        if (!isOpen()) {
            logger.warning("Attempted to read from closed FIFO pipe");

            // Try to reconnect if configured to do so
            if (config.auto_reconnect) {
                auto result = attemptReconnect(timeout);
                if (!result) {
                    return type::unexpected(result.error().error());
                }
            } else {
                return type::unexpected(make_error_code(FifoError::NotOpen));
            }
        }

        std::lock_guard<std::mutex> lock(operationMutex);
        auto start_time = std::chrono::steady_clock::now();

        try {
            std::string result;
            result.reserve(maxSize);  // Pre-allocate memory

            // Buffer allocation strategy: use stack for small reads, heap for
            // larger ones
            std::vector<char> buffer(maxSize);

#ifdef _WIN32
            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (overlapped.hEvent == NULL) {
                auto error = GetLastError();
                logger.error("Failed to create event for FIFO read: {}", error);
                return type::unexpected(
                    std::error_code(error, std::system_category()));
            }

            DWORD bytesRead = 0;
            bool success = ReadFile(fifoHandle, buffer.data(),
                                    static_cast<DWORD>(buffer.size()),
                                    &bytesRead, &overlapped);

            // Handle asynchronous operation
            if (!success && GetLastError() == ERROR_IO_PENDING) {
                DWORD waitTime =
                    timeout ? static_cast<DWORD>(timeout->count()) : INFINITE;
                DWORD waitResult =
                    WaitForSingleObject(overlapped.hEvent, waitTime);

                if (waitResult == WAIT_TIMEOUT) {
                    CancelIo(fifoHandle);
                    CloseHandle(overlapped.hEvent);
                    logger.warning("Read operation timed out after {} ms",
                                   timeout ? timeout->count() : 0);
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (waitResult != WAIT_OBJECT_0) {
                    auto error = GetLastError();
                    CloseHandle(overlapped.hEvent);
                    logger.error("Wait for read operation failed: {}", error);
                    return type::unexpected(
                        std::error_code(error, std::system_category()));
                }

                // Get the result of the operation
                if (!GetOverlappedResult(fifoHandle, &overlapped, &bytesRead,
                                         FALSE)) {
                    auto error = GetLastError();
                    CloseHandle(overlapped.hEvent);

                    // Check if this is a connection loss
                    if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                        isConnected = false;
                        notifyConnectionChange(
                            false,
                            std::error_code(error, std::system_category()));
                        logger.warning("Connection lost during read operation");

                        // Try to reconnect if configured to do so
                        if (config.auto_reconnect) {
                            auto reconnect_result = attemptReconnect(timeout);
                            if (reconnect_result) {
                                // Try the read operation again
                                CloseHandle(overlapped.hEvent);
                                return read(maxSize, timeout);
                            }
                        }

                        return type::unexpected(
                            make_error_code(FifoError::ConnectionLost));
                    }

                    logger.error("Failed to get overlapped result for read: {}",
                                 error);
                    return type::unexpected(
                        std::error_code(error, std::system_category()));
                }
            } else if (!success) {
                auto error = GetLastError();
                CloseHandle(overlapped.hEvent);

                // Check if this is a connection loss
                if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                    isConnected = false;
                    notifyConnectionChange(
                        false, std::error_code(error, std::system_category()));
                    logger.warning("Connection lost during read operation");

                    // Try to reconnect if configured to do so
                    if (config.auto_reconnect) {
                        auto reconnect_result = attemptReconnect(timeout);
                        if (reconnect_result) {
                            // Try the read operation again
                            return read(maxSize, timeout);
                        }
                    }

                    return type::unexpected(
                        make_error_code(FifoError::ConnectionLost));
                }

                logger.error("Read operation failed: {}", error);
                return type::unexpected(
                    std::error_code(error, std::system_category()));
            }

            CloseHandle(overlapped.hEvent);

            if (bytesRead > 0) {
                result.append(buffer.data(), bytesRead);

                // Update statistics
                updateReadStats(bytesRead, start_time);

                // Process the received data if needed
                if (config.enable_encryption) {
                    try {
                        result = decryptData(result);
                    } catch (const std::exception& e) {
                        logger.error("Failed to decrypt data: {}", e.what());
                        return type::unexpected(
                            make_error_code(FifoError::DecryptionFailed));
                    }
                }

                // Check for compression marker and decompress if needed
                if (!result.empty() && result.substr(0, 2) == "C:") {
                    try {
                        result = decompressData(result.substr(2));
                    } catch (const std::exception& e) {
                        logger.error("Failed to decompress data: {}", e.what());
                        return type::unexpected(
                            make_error_code(FifoError::CompressionFailed));
                    }
                } else if (!result.empty() && result.substr(0, 3) == "NC:") {
                    // Remove the "not compressed" marker
                    result = result.substr(3);
                }

                // Extract priority if present
                if (!result.empty() && result.substr(0, 2) == "P:") {
                    size_t secondColon = result.find(':', 2);
                    if (secondColon != std::string::npos) {
                        // Remove the priority marker
                        result = result.substr(secondColon + 1);
                    }
                }

                return result;
            } else {
                // Zero bytes read could mean no data available or EOF
                return std::string();
            }
#else
            if (timeout) {
                pollfd pfd{};
                pfd.fd = fifoFd;
                pfd.events = POLLIN;

                int pollResult =
                    poll(&pfd, 1, static_cast<int>(timeout->count()));

                if (pollResult == 0) {
                    logger.warning("Read operation timed out after {} ms",
                                   timeout->count());
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (pollResult < 0) {
                    auto error = errno;
                    logger.error("Poll operation failed: {}", strerror(error));

                    // Check if this is a connection-related error
                    if (error == EPIPE || error == ECONNRESET ||
                        error == EBADF) {
                        isConnected = false;
                        notifyConnectionChange(
                            false,
                            std::error_code(error, std::system_category()));
                        logger.warning("Connection lost during poll operation");

                        // Try to reconnect if configured to do so
                        if (config.auto_reconnect) {
                            auto reconnect_result = attemptReconnect(timeout);
                            if (reconnect_result) {
                                // Try the read operation again
                                return read(maxSize, timeout);
                            }
                        }

                        return type::unexpected(
                            make_error_code(FifoError::ConnectionLost));
                    }

                    return type::unexpected(
                        std::error_code(error, std::system_category()));
                }

                if (!(pfd.revents & POLLIN)) {
                    logger.error("File descriptor not ready for reading");
                    return type::unexpected(
                        make_error_code(FifoError::ReadFailed));
                }
            }

            ssize_t bytesRead = ::read(fifoFd, buffer.data(), buffer.size());

            if (bytesRead == -1) {
                auto error = errno;
                logger.error("Read operation failed: {}", strerror(error));

                // Check if this is a connection-related error
                if (error == EPIPE || error == ECONNRESET) {
                    isConnected = false;
                    notifyConnectionChange(
                        false, std::error_code(error, std::system_category()));
                    logger.warning("Connection lost during read operation");

                    // Try to reconnect if configured to do so
                    if (config.auto_reconnect) {
                        auto reconnect_result = attemptReconnect(timeout);
                        if (reconnect_result) {
                            // Try the read operation again
                            return read(maxSize, timeout);
                        }
                    }

                    return type::unexpected(
                        make_error_code(FifoError::ConnectionLost));
                }

                return type::unexpected(
                    std::error_code(error, std::system_category()));
            }

            if (bytesRead > 0) {
                result.append(buffer.data(), bytesRead);

                // Update statistics
                updateReadStats(bytesRead, start_time);

                // Process the received data if needed
                if (config.enable_encryption) {
                    try {
                        result = decryptData(result);
                    } catch (const std::exception& e) {
                        logger.error("Failed to decrypt data: {}", e.what());
                        return type::unexpected(
                            make_error_code(FifoError::DecryptionFailed));
                    }
                }

                // Check for compression marker and decompress if needed
                if (!result.empty() && result.substr(0, 2) == "C:") {
                    try {
                        result = decompressData(result.substr(2));
                    } catch (const std::exception& e) {
                        logger.error("Failed to decompress data: {}", e.what());
                        return type::unexpected(
                            make_error_code(FifoError::CompressionFailed));
                    }
                } else if (!result.empty() && result.substr(0, 3) == "NC:") {
                    // Remove the "not compressed" marker
                    result = result.substr(3);
                }

                // Extract priority if present
                if (!result.empty() && result.substr(0, 2) == "P:") {
                    size_t secondColon = result.find(':', 2);
                    if (secondColon != std::string::npos) {
                        // Remove the priority marker
                        result = result.substr(secondColon + 1);
                    }
                }

                return result;
            } else {
                // Zero bytes read means EOF in Unix
                return std::string();
            }
#endif
        } catch (const std::exception& e) {
            logger.error("Exception during read operation: {}", e.what());
            return type::unexpected(make_error_code(FifoError::ReadFailed));
        }
    }

    int readAsync(
        OperationCallback callback, std::size_t maxSize = 0,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        int operationId = nextOperationId++;

        // Store the operation information
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            pendingOperations.emplace(
                operationId,
                AsyncOperation(AsyncOperation::Type::Read, operationId,
                               std::move(callback), timeout));
        }

        // Queue the operation in the async thread
        std::thread([this, operationId, maxSize, timeout]() {
            // Check if the operation has been canceled
            {
                std::lock_guard<std::mutex> lock(asyncMutex);
                auto it = pendingOperations.find(operationId);
                if (it == pendingOperations.end() || it->second.canceled) {
                    return;  // Operation was canceled
                }
            }

            // Perform the read operation
            auto result = this->read(maxSize, timeout);

            // Find the callback and invoke it
            OperationCallback callbackCopy;
            {
                std::lock_guard<std::mutex> lock(asyncMutex);
                auto it = pendingOperations.find(operationId);
                if (it != pendingOperations.end()) {
                    callbackCopy = it->second.callback;
                    pendingOperations.erase(it);
                }
            }

            if (callbackCopy) {
                try {
                    if (result) {
                        callbackCopy(true, {}, result.value().size());
                    } else {
                        callbackCopy(false, result.error().error(), 0);
                    }
                } catch (const std::exception& e) {
                    logger.error("Exception in async read callback: {}",
                                 e.what());
                }
            }
        }).detach();

        return operationId;
    }

    std::future<type::expected<std::string, std::error_code>>
    readAsyncWithFuture(
        std::size_t maxSize = 0,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        // Create a promise and future
        auto promise = std::make_shared<
            std::promise<type::expected<std::string, std::error_code>>>();
        auto future = promise->get_future();

        // Read asynchronously in a separate thread
        std::thread([this, promise, maxSize, timeout]() {
            auto result = this->read(maxSize, timeout);
            promise->set_value(std::move(result));
        }).detach();

        return future;
    }

    bool cancelOperation(int id) {
        std::lock_guard<std::mutex> lock(asyncMutex);
        auto it = pendingOperations.find(id);
        if (it != pendingOperations.end()) {
            it->second.canceled = true;
            logger.info("Operation {} canceled", id);
            return true;
        }
        return false;
    }

    int registerConnectionCallback(ConnectionCallback callback) {
        if (!callback) {
            logger.warning("Attempted to register null connection callback");
            return -1;
        }

        std::lock_guard<std::mutex> lock(callbackMutex);
        int id = nextCallbackId++;
        connectionCallbacks[id] = std::move(callback);

        // Immediately notify the callback of the current connection state
        if (isConnected && connectionCallbacks[id]) {
            try {
                connectionCallbacks[id](true, {});
            } catch (const std::exception& e) {
                logger.error("Exception in connection callback: {}", e.what());
            }
        }

        return id;
    }

    bool unregisterConnectionCallback(int id) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        return connectionCallbacks.erase(id) > 0;
    }

    void notifyConnectionChange(bool connected, std::error_code ec) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        for (auto& [id, callback] : connectionCallbacks) {
            if (callback) {
                try {
                    callback(connected, ec);
                } catch (const std::exception& e) {
                    logger.error("Exception in connection callback: {}",
                                 e.what());
                }
            }
        }
    }

    void updateWriteStats(
        size_t dataSize, size_t bytesWritten,
        const std::chrono::steady_clock::time_point& start_time) {
        auto end_time = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                           end_time - start_time)
                           .count();

        if (bytesWritten > 0) {
            stats.messages_sent++;
            stats.bytes_sent += bytesWritten;

            // Update compression ratio statistics
            if (dataSize > 0) {
                double ratio = static_cast<double>(bytesWritten) / dataSize;
                if (stats.messages_sent == 1) {
                    stats.avg_compression_ratio = ratio;
                } else {
                    stats.avg_compression_ratio =
                        ((stats.avg_compression_ratio *
                          (stats.messages_sent - 1)) +
                         ratio) /
                        stats.messages_sent;
                }
            }

            // Update average write latency
            if (stats.messages_sent == 1) {
                stats.avg_write_latency_ms = static_cast<double>(latency);
            } else {
                stats.avg_write_latency_ms =
                    ((stats.avg_write_latency_ms * (stats.messages_sent - 1)) +
                     latency) /
                    stats.messages_sent;
            }
        } else {
            stats.messages_failed++;
        }
    }

    void updateReadStats(
        size_t bytesRead,
        const std::chrono::steady_clock::time_point& start_time) {
        auto end_time = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                           end_time - start_time)
                           .count();

        stats.bytes_received += bytesRead;

        // Update average read latency
        if (stats.bytes_received == bytesRead) {
            stats.avg_read_latency_ms = static_cast<double>(latency);
        } else {
            // Use a weighted average based on bytes received
            double weight =
                static_cast<double>(bytesRead) / stats.bytes_received;
            stats.avg_read_latency_ms =
                (stats.avg_read_latency_ms * (1.0 - weight)) +
                (latency * weight);
        }
    }

    std::string compressData(const std::string& data) {
#ifdef ENABLE_COMPRESSION
        // Skip compression for small messages
        if (data.size() < config.compression_threshold) {
            // Add a marker to indicate not compressed
            return "NC:" + data;
        }

        z_stream zs{};
        if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
            logger.error("Failed to initialize zlib");
            return "NC:" + data;  // Return uncompressed with marker
        }

        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zs.avail_in = static_cast<uInt>(data.size());

        // Estimate the size needed for compressed data
        size_t outsize = data.size() * 1.1 + 12;
        std::string outstring(outsize, '\0');

        zs.next_out = reinterpret_cast<Bytef*>(outstring.data());
        zs.avail_out = static_cast<uInt>(outsize);

        int result = deflate(&zs, Z_FINISH);
        deflateEnd(&zs);

        if (result != Z_STREAM_END) {
            logger.error("Error during compression: {}", result);
            return "NC:" + data;  // Return uncompressed with marker
        }

        // Resize to actual compressed size
        outstring.resize(zs.total_out);

        // Add a marker to indicate compressed
        return "C:" + outstring;
#else
        // Compression not enabled, return as-is
        return data;
#endif
    }

    std::string decompressData(const std::string& data) {
#ifdef ENABLE_COMPRESSION
        z_stream zs{};
        if (inflateInit(&zs) != Z_OK) {
            logger.error("Failed to initialize zlib for decompression");
            throw std::runtime_error("Failed to initialize zlib");
        }

        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zs.avail_in = static_cast<uInt>(data.size());

        // Estimate the size needed for decompressed data
        // Start with a reasonable size and grow if needed
        size_t outsize = data.size() * 2;
        std::string outstring(outsize, '\0');

        zs.next_out = reinterpret_cast<Bytef*>(outstring.data());
        zs.avail_out = static_cast<uInt>(outsize);

        int result = inflate(&zs, Z_FINISH);

        // If we need more space, realloc and continue
        while (result == Z_BUF_ERROR) {
            // Double the buffer size
            outsize *= 2;
            outstring.resize(outsize);

            // Update zlib stream
            zs.next_out =
                reinterpret_cast<Bytef*>(outstring.data() + zs.total_out);
            zs.avail_out = static_cast<uInt>(outsize - zs.total_out);

            // Continue decompression
            result = inflate(&zs, Z_FINISH);
        }

        inflateEnd(&zs);

        if (result != Z_STREAM_END) {
            logger.error("Error during decompression: {}", result);
            throw std::runtime_error("Error during decompression");
        }

        // Resize to actual decompressed size
        outstring.resize(zs.total_out);

        return outstring;
#else
        // Compression not enabled, return as-is
        return data;
#endif
    }

    std::string encryptData(const std::string& data) {
#ifdef ENABLE_ENCRYPTION
        // Simple XOR encryption as a placeholder
        // In a real application, use a proper cryptographic library

        // Generate a random key
        std::string key(16, '\0');
        RAND_bytes(reinterpret_cast<unsigned char*>(key.data()), key.size());

        // Encrypt the message
        std::string encrypted(data.size(), '\0');
        for (size_t i = 0; i < data.size(); ++i) {
            encrypted[i] = data[i] ^ key[i % key.size()];
        }

        // Prepend the key to the encrypted message
        return "E:" + key + encrypted;
#else
        // Encryption not enabled
        return data;
#endif
    }

    std::string decryptData(const std::string& data) {
#ifdef ENABLE_ENCRYPTION
        // Check for encryption marker
        if (data.substr(0, 2) != "E:") {
            return data;  // Not encrypted
        }

        // Extract the key (fixed size of 16 bytes)
        if (data.size() < 18) {  // 2 for "E:" + 16 for key
            logger.error("Encrypted data too short");
            throw std::runtime_error("Encrypted data too short");
        }

        std::string key = data.substr(2, 16);
        std::string encryptedContent = data.substr(18);

        // Decrypt the message using XOR
        std::string decrypted(encryptedContent.size(), '\0');
        for (size_t i = 0; i < encryptedContent.size(); ++i) {
            decrypted[i] = encryptedContent[i] ^ key[i % key.size()];
        }

        return decrypted;
#else
        // Check if data appears to be encrypted
        if (data.substr(0, 2) == "E:") {
            logger.error(
                "Received encrypted data but encryption is not enabled");
            throw std::runtime_error("Cannot decrypt: encryption not enabled");
        }

        // Not encrypted or encryption not enabled
        return data;
#endif
    }

    void startAsyncThread() {
        stopAsyncThread = false;
        asyncThread = std::jthread([this](std::stop_token stop_token) {
            logger.debug("Async operation thread started");

            while (!stop_token.stop_requested() && !stopAsyncThread) {
                // Check for timed-out operations
                std::vector<int> timedOutOps;
                {
                    std::lock_guard<std::mutex> lock(asyncMutex);
                    auto now = std::chrono::steady_clock::now();

                    for (auto& [id, op] : pendingOperations) {
                        if (op.timeout && !op.canceled) {
                            auto elapsed = std::chrono::duration_cast<
                                std::chrono::milliseconds>(now - op.start_time);

                            if (elapsed > *op.timeout) {
                                // Operation timed out
                                timedOutOps.push_back(id);
                                logger.warning(
                                    "Async operation {} timed out after {} ms",
                                    id, elapsed.count());
                            }
                        }
                    }

                    // Handle timed-out operations
                    for (int id : timedOutOps) {
                        auto it = pendingOperations.find(id);
                        if (it != pendingOperations.end()) {
                            // Call the callback with timeout error
                            if (it->second.callback) {
                                try {
                                    it->second.callback(
                                        false,
                                        make_error_code(FifoError::Timeout), 0);
                                } catch (const std::exception& e) {
                                    logger.error(
                                        "Exception in timeout callback: {}",
                                        e.what());
                                }
                            }

                            // Remove the operation
                            pendingOperations.erase(it);
                        }
                    }
                }

                // Sleep for a short time to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            logger.debug("Async operation thread stopped");
        });
    }

    ClientConfig getConfig() const { return config; }

    bool updateConfig(const ClientConfig& newConfig) {
        std::lock_guard<std::mutex> lock(operationMutex);

        // Update configuration
        config.read_buffer_size = newConfig.read_buffer_size;
        config.max_message_size = newConfig.max_message_size;
        config.auto_reconnect = newConfig.auto_reconnect;
        config.max_reconnect_attempts = newConfig.max_reconnect_attempts;
        config.reconnect_delay = newConfig.reconnect_delay;
        config.default_timeout = newConfig.default_timeout;
        config.compression_threshold = newConfig.compression_threshold;

        // Update log level
        if (config.log_level != newConfig.log_level) {
            config.log_level = newConfig.log_level;
            logger.setLevel(newConfig.log_level);
        }

        // Encryption and compression may require reinitialization in a real
        // implementation
        config.enable_compression = newConfig.enable_compression;
        config.enable_encryption = newConfig.enable_encryption;

        logger.info("Client configuration updated");
        return true;
    }

    ClientStats getStatistics() const {
        std::lock_guard<std::mutex> lock(operationMutex);
        return stats;
    }

    void resetStatistics() {
        std::lock_guard<std::mutex> lock(operationMutex);
        stats = ClientStats{};
        logger.info("Client statistics reset");
    }
};

// FifoClient implementation

FifoClient::FifoClient(std::string_view fifoPath)
    : m_impl(std::make_unique<Impl>(fifoPath)) {}

FifoClient::FifoClient(std::string_view fifoPath, const ClientConfig& config)
    : m_impl(std::make_unique<Impl>(fifoPath, config)) {}

FifoClient::FifoClient(FifoClient&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}

FifoClient& FifoClient::operator=(FifoClient&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

FifoClient::~FifoClient() = default;

auto FifoClient::write(std::string_view data,
                       std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::size_t, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    return m_impl->write(data, MessagePriority::Normal, timeout);
}

auto FifoClient::write(std::string_view data, MessagePriority priority,
                       std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::size_t, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    return m_impl->write(data, priority, timeout);
}

int FifoClient::writeAsync(std::string_view data, OperationCallback callback,
                           std::optional<std::chrono::milliseconds> timeout) {
    if (!m_impl) {
        if (callback) {
            callback(false, make_error_code(FifoError::NotOpen), 0);
        }
        return -1;
    }
    return m_impl->writeAsync(data, std::move(callback), timeout);
}

std::future<type::expected<std::size_t, std::error_code>>
FifoClient::writeAsyncWithFuture(
    std::string_view data, std::optional<std::chrono::milliseconds> timeout) {
    if (!m_impl) {
        auto promise = std::make_shared<
            std::promise<type::expected<std::size_t, std::error_code>>>();
        promise->set_value(
            type::unexpected(make_error_code(FifoError::NotOpen)));
        return promise->get_future();
    }
    return m_impl->writeAsyncWithFuture(data, timeout);
}

auto FifoClient::writeMultiple(const std::vector<std::string>& messages,
                               std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::size_t, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    return m_impl->writeMultiple(messages, timeout);
}

auto FifoClient::read(std::size_t maxSize,
                      std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<std::string, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    return m_impl->read(maxSize, timeout);
}

int FifoClient::readAsync(OperationCallback callback, std::size_t maxSize,
                          std::optional<std::chrono::milliseconds> timeout) {
    if (!m_impl) {
        if (callback) {
            callback(false, make_error_code(FifoError::NotOpen), 0);
        }
        return -1;
    }
    return m_impl->readAsync(std::move(callback), maxSize, timeout);
}

std::future<type::expected<std::string, std::error_code>>
FifoClient::readAsyncWithFuture(
    std::size_t maxSize, std::optional<std::chrono::milliseconds> timeout) {
    if (!m_impl) {
        auto promise = std::make_shared<
            std::promise<type::expected<std::string, std::error_code>>>();
        promise->set_value(
            type::unexpected(make_error_code(FifoError::NotOpen)));
        return promise->get_future();
    }
    return m_impl->readAsyncWithFuture(maxSize, timeout);
}

auto FifoClient::open(std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<void, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::InvalidOperation));
    }

    try {
        auto start_time = std::chrono::steady_clock::now();
        bool success = false;
        std::error_code last_error;

        do {
            try {
                m_impl->openFifo();
                success = true;
                break;
            } catch (const std::system_error& e) {
                last_error = e.code();

                // If timeout specified, check if we should try again
                if (timeout) {
                    auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time);

                    if (elapsed >= *timeout) {
                        // Timeout reached
                        m_impl->logger.warning(
                            "Open operation timed out after {} ms",
                            timeout->count());
                        return type::unexpected(
                            make_error_code(FifoError::Timeout));
                    }

                    // Wait a bit before retrying
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                } else {
                    // No timeout, don't retry
                    throw;
                }
            }
        } while (timeout);  // Only loop if timeout is specified

        if (success) {
            return {};
        } else {
            return type::unexpected(last_error);
        }
    } catch (const std::system_error& e) {
        return type::unexpected(e.code());
    } catch (const std::exception& e) {
        return type::unexpected(make_error_code(FifoError::OpenFailed));
    }
}

bool FifoClient::isOpen() const noexcept { return m_impl && m_impl->isOpen(); }

std::string_view FifoClient::getPath() const noexcept {
    if (!m_impl) {
        static const std::string empty;
        return empty;
    }
    return m_impl->fifoPath;
}

void FifoClient::close() noexcept {
    if (m_impl) {
        m_impl->close();
    }
}

int FifoClient::registerConnectionCallback(ConnectionCallback callback) {
    if (!m_impl) {
        return -1;
    }
    return m_impl->registerConnectionCallback(std::move(callback));
}

bool FifoClient::unregisterConnectionCallback(int id) {
    if (!m_impl) {
        return false;
    }
    return m_impl->unregisterConnectionCallback(id);
}

ClientConfig FifoClient::getConfig() const {
    if (!m_impl) {
        return {};
    }
    return m_impl->getConfig();
}

bool FifoClient::updateConfig(const ClientConfig& config) {
    if (!m_impl) {
        return false;
    }
    return m_impl->updateConfig(config);
}

ClientStats FifoClient::getStatistics() const {
    if (!m_impl) {
        return {};
    }
    return m_impl->getStatistics();
}

void FifoClient::resetStatistics() {
    if (m_impl) {
        m_impl->resetStatistics();
    }
}

void FifoClient::setLogLevel(LogLevel level) {
    if (m_impl) {
        m_impl->logger.setLevel(level);
        if (auto config = m_impl->getConfig(); config.log_level != level) {
            config.log_level = level;
            m_impl->updateConfig(config);
        }
    }
}

bool FifoClient::cancelOperation(int id) {
    if (!m_impl) {
        return false;
    }
    return m_impl->cancelOperation(id);
}

}  // namespace atom::connection