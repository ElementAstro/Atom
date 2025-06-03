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
#include <functional>
#include <future>
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
#include <cstring>
#endif

#ifdef ENABLE_COMPRESSION
#include <zlib.h>
#endif

#ifdef ENABLE_ENCRYPTION
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include "spdlog/spdlog.h"

namespace atom::connection {

class FifoErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "fifo_client";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<FifoError>(ev)) {
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
            case FifoError::EncryptionFailed:
                return "Encryption failed";
            case FifoError::DecryptionFailed:
                return "Decryption failed";
            default:
                return "Unknown FIFO error";
        }
    }
};

const FifoErrorCategory theFifoErrorCategory{};

[[nodiscard]] std::error_code make_error_code(FifoError e) {
    return {static_cast<int>(e), theFifoErrorCategory};
}

struct AsyncOperation {
    enum class Type { Read, Write };
    Type type;
    int id;
    OperationCallback callback;
    std::chrono::steady_clock::time_point start_time;
    std::optional<std::chrono::milliseconds> timeout;
    std::atomic<bool> canceled = false;

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

    mutable std::mutex operationMutex;
    std::mutex asyncMutex;
    std::mutex callbackMutex;

    std::atomic<int> nextOperationId{1};
    std::unordered_map<int, std::unique_ptr<AsyncOperation>> pendingOperations;
    std::jthread asyncThread;
    std::atomic_bool stopAsyncThread{false};
    std::condition_variable asyncCondition;

    std::atomic_bool isConnected{false};
    std::atomic<int> reconnectAttempts{0};

    std::atomic<int> nextCallbackId{1};
    std::unordered_map<int, ConnectionCallback> connectionCallbacks;

    explicit Impl(std::string_view path, const ClientConfig& clientConfig = {})
        : fifoPath(path), config(clientConfig) {
        spdlog::info("Creating FIFO client for path: {}", fifoPath);
        startAsyncThread();
        openFifo();
    }

    ~Impl() {
        spdlog::debug("Destroying FIFO client");
        close();
        stopAsyncThread = true;
        if (asyncThread.joinable()) {
            asyncCondition.notify_all();
            asyncThread.join();
        }
    }

    void openFifo() {
        std::lock_guard<std::mutex> lock(operationMutex);

        if (isOpen()) {
            spdlog::debug("FIFO already open");
            return;
        }

        spdlog::info("Opening FIFO: {}", fifoPath);

#ifdef _WIN32
        fifoHandle = CreateFileA(fifoPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (fifoHandle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            spdlog::error("Failed to open FIFO {}: error {}", fifoPath, error);
            throw std::system_error(make_error_code(FifoError::OpenFailed));
        }
#else
        fifoFd = ::open(fifoPath.c_str(), O_RDWR | O_NONBLOCK);
        if (fifoFd == -1) {
            spdlog::error("Failed to open FIFO {}: {}", fifoPath,
                          strerror(errno));
            throw std::system_error(make_error_code(FifoError::OpenFailed));
        }
#endif

        isConnected = true;
        spdlog::info("FIFO opened successfully: {}", fifoPath);
        notifyConnectionChange(true, {});
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

        if (!isOpen()) {
            return;
        }

        spdlog::info("Closing FIFO: {}", fifoPath);

#ifdef _WIN32
        if (fifoHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(fifoHandle);
            fifoHandle = INVALID_HANDLE_VALUE;
        }
#else
        if (fifoFd != -1) {
            ::close(fifoFd);
            fifoFd = -1;
        }
#endif

        bool wasConnected = isConnected.exchange(false);
        if (wasConnected) {
            notifyConnectionChange(false, {});
        }

        std::lock_guard<std::mutex> asyncLock(asyncMutex);
        pendingOperations.clear();
    }

    auto attemptReconnect(std::optional<std::chrono::milliseconds> timeout)
        -> type::expected<void, std::error_code> {
        if (!config.auto_reconnect) {
            spdlog::debug("Auto-reconnect disabled");
            return type::unexpected(make_error_code(FifoError::ConnectionLost));
        }

        int attempts = reconnectAttempts.load();
        if (attempts >= config.max_reconnect_attempts) {
            spdlog::error("Maximum reconnection attempts ({}) exceeded",
                          config.max_reconnect_attempts);
            return type::unexpected(make_error_code(FifoError::ConnectionLost));
        }

        spdlog::info("Attempting to reconnect (attempt {}/{})", attempts + 1,
                     config.max_reconnect_attempts);

        reconnectAttempts++;
        std::this_thread::sleep_for(config.reconnect_delay);

        try {
            close();
            openFifo();
            stats.successful_reconnects++;
            reconnectAttempts = 0;
            spdlog::info("Reconnection successful");
            return {};
        } catch (const std::exception& e) {
            spdlog::error("Reconnection failed: {}", e.what());
            return type::unexpected(make_error_code(FifoError::ConnectionLost));
        }
    }

    type::expected<std::size_t, std::error_code> write(
        std::string_view data,
        MessagePriority priority = MessagePriority::Normal,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        if (data.size() > config.max_message_size) {
            spdlog::error("Message size {} exceeds maximum {}", data.size(),
                          config.max_message_size);
            return type::unexpected(
                make_error_code(FifoError::MessageTooLarge));
        }

        std::lock_guard<std::mutex> lock(operationMutex);
        auto startTime = std::chrono::steady_clock::now();

        if (!isOpen()) {
            auto reconnectResult = attemptReconnect(timeout);
            if (!reconnectResult) {
                stats.messages_failed++;
                return type::unexpected(reconnectResult.error().error());
            }
        }

        std::string processedData(data);

        if (config.enable_compression &&
            data.size() >= config.compression_threshold) {
            try {
                processedData = compressData(std::string(data));
                spdlog::debug("Compressed data from {} to {} bytes",
                              data.size(), processedData.size());
            } catch (const std::exception& e) {
                spdlog::error("Compression failed: {}", e.what());
                return type::unexpected(
                    make_error_code(FifoError::CompressionFailed));
            }
        }

        if (config.enable_encryption) {
            try {
                processedData = encryptData(processedData);
                spdlog::debug("Encrypted data: {} bytes", processedData.size());
            } catch (const std::exception& e) {
                spdlog::error("Encryption failed: {}", e.what());
                return type::unexpected(
                    make_error_code(FifoError::EncryptionFailed));
            }
        }

        size_t bytesWritten = 0;
        auto effectiveTimeout = timeout.value_or(
            config.default_timeout.value_or(std::chrono::milliseconds(5000)));

#ifdef _WIN32
        DWORD written;
        BOOL result = WriteFile(fifoHandle, processedData.data(),
                                processedData.size(), &written, nullptr);

        if (!result) {
            DWORD error = GetLastError();
            spdlog::error("Write failed: error {}", error);
            stats.messages_failed++;
            return type::unexpected(make_error_code(FifoError::WriteFailed));
        }
        bytesWritten = written;
#else
        ssize_t result =
            ::write(fifoFd, processedData.data(), processedData.size());

        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pollfd pfd{fifoFd, POLLOUT, 0};
                int pollResult = poll(&pfd, 1, effectiveTimeout.count());

                if (pollResult == 0) {
                    spdlog::warn("Write operation timed out");
                    stats.messages_failed++;
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (pollResult == -1) {
                    spdlog::error("Poll failed: {}", strerror(errno));
                    stats.messages_failed++;
                    return type::unexpected(
                        make_error_code(FifoError::WriteFailed));
                }

                result =
                    ::write(fifoFd, processedData.data(), processedData.size());
            }

            if (result == -1) {
                spdlog::error("Write failed: {}", strerror(errno));
                stats.messages_failed++;
                return type::unexpected(
                    make_error_code(FifoError::WriteFailed));
            }
        }
        bytesWritten = static_cast<size_t>(result);
#endif

        updateWriteStats(data.size(), bytesWritten, startTime);
        stats.messages_sent++;

        spdlog::debug("Successfully wrote {} bytes to FIFO", bytesWritten);
        return bytesWritten;
    }

    type::expected<std::size_t, std::error_code> writeMultiple(
        const std::vector<std::string>& messages,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        if (messages.empty()) {
            return 0;
        }

        size_t totalBytes = 0;
        for (const auto& msg : messages) {
            auto result = write(msg, MessagePriority::Normal, timeout);
            if (!result) {
                return result;
            }
            totalBytes += *result;
        }

        return totalBytes;
    }

    int writeAsync(
        std::string_view data, OperationCallback callback,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        std::lock_guard<std::mutex> lock(asyncMutex);

        int id = nextOperationId++;
        auto operation = std::make_unique<AsyncOperation>(
            AsyncOperation::Type::Write, id, std::move(callback), timeout);

        std::string dataCopy(data);

        pendingOperations[id] = std::move(operation);

        std::thread([this, id, dataCopy = std::move(dataCopy)]() {
            auto result = write(dataCopy);

            std::lock_guard<std::mutex> asyncLock(asyncMutex);
            auto it = pendingOperations.find(id);
            if (it != pendingOperations.end() && !it->second->canceled) {
                if (result) {
                    it->second->callback(true, {}, *result);
                } else {
                    it->second->callback(false, result.error().error(), 0);
                }
                pendingOperations.erase(it);
            }
        }).detach();

        return id;
    }

    std::future<type::expected<std::size_t, std::error_code>>
    writeAsyncWithFuture(
        std::string_view data,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        auto promise = std::make_shared<
            std::promise<type::expected<std::size_t, std::error_code>>>();
        auto future = promise->get_future();

        writeAsync(
            data,
            [promise](bool success, std::error_code ec, size_t bytes) {
                if (success) {
                    promise->set_value(bytes);
                } else {
                    promise->set_value(type::unexpected(ec));
                }
            },
            timeout);

        return future;
    }

    type::expected<std::string, std::error_code> read(
        std::size_t maxSize,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        std::lock_guard<std::mutex> lock(operationMutex);
        auto startTime = std::chrono::steady_clock::now();

        if (!isOpen()) {
            auto reconnectResult = attemptReconnect(timeout);
            if (!reconnectResult) {
                return type::unexpected(reconnectResult.error().error());
            }
        }

        size_t bufferSize = maxSize > 0 ? maxSize : config.read_buffer_size;
        std::vector<char> buffer(bufferSize);

        auto effectiveTimeout = timeout.value_or(
            config.default_timeout.value_or(std::chrono::milliseconds(5000)));

        size_t bytesRead = 0;

#ifdef _WIN32
        DWORD read;
        BOOL result =
            ReadFile(fifoHandle, buffer.data(), bufferSize, &read, nullptr);

        if (!result) {
            DWORD error = GetLastError();
            spdlog::error("Read failed: error {}", error);
            return type::unexpected(make_error_code(FifoError::ReadFailed));
        }
        bytesRead = read;
#else
        ssize_t result = ::read(fifoFd, buffer.data(), bufferSize);

        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                pollfd pfd{fifoFd, POLLIN, 0};
                int pollResult = poll(&pfd, 1, effectiveTimeout.count());

                if (pollResult == 0) {
                    spdlog::warn("Read operation timed out");
                    return type::unexpected(
                        make_error_code(FifoError::Timeout));
                } else if (pollResult == -1) {
                    spdlog::error("Poll failed: {}", strerror(errno));
                    return type::unexpected(
                        make_error_code(FifoError::ReadFailed));
                }

                result = ::read(fifoFd, buffer.data(), bufferSize);
            }

            if (result == -1) {
                spdlog::error("Read failed: {}", strerror(errno));
                return type::unexpected(make_error_code(FifoError::ReadFailed));
            }
        }
        bytesRead = static_cast<size_t>(result);
#endif

        if (bytesRead == 0) {
            spdlog::debug("No data available to read");
            return std::string{};
        }

        std::string data(buffer.data(), bytesRead);
        data = processReceivedData(std::move(data));

        updateReadStats(bytesRead, startTime);

        spdlog::debug("Successfully read {} bytes from FIFO", bytesRead);
        return data;
    }

    std::string processReceivedData(std::string data) {
        if (config.enable_encryption) {
            try {
                data = decryptData(data);
                spdlog::debug("Decrypted data: {} bytes", data.size());
            } catch (const std::exception& e) {
                spdlog::error("Decryption failed: {}", e.what());
                throw std::system_error(
                    make_error_code(FifoError::DecryptionFailed));
            }
        }

        if (config.enable_compression) {
            try {
                data = decompressData(data);
                spdlog::debug("Decompressed data: {} bytes", data.size());
            } catch (const std::exception& e) {
                spdlog::warn("Data may not be compressed, using as-is: {}",
                             e.what());
            }
        }

        return data;
    }

    int readAsync(
        OperationCallback callback, std::size_t maxSize = 0,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        std::lock_guard<std::mutex> lock(asyncMutex);

        int id = nextOperationId++;
        auto operation = std::make_unique<AsyncOperation>(
            AsyncOperation::Type::Read, id, std::move(callback), timeout);

        pendingOperations[id] = std::move(operation);

        std::thread([this, id, maxSize]() {
            auto result = read(maxSize);

            std::lock_guard<std::mutex> asyncLock(asyncMutex);
            auto it = pendingOperations.find(id);
            if (it != pendingOperations.end() && !it->second->canceled) {
                if (result) {
                    it->second->callback(true, {}, result->size());
                } else {
                    it->second->callback(false, result.error().error(), 0);
                }
                pendingOperations.erase(it);
            }
        }).detach();

        return id;
    }

    std::future<type::expected<std::string, std::error_code>>
    readAsyncWithFuture(
        std::size_t maxSize = 0,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        auto promise = std::make_shared<
            std::promise<type::expected<std::string, std::error_code>>>();
        auto future = promise->get_future();

        readAsync(
            [promise](bool success, std::error_code ec, size_t) {
                if (success) {
                    promise->set_value(
                        std::string{});  // Would need to store actual data
                } else {
                    promise->set_value(type::unexpected(ec));
                }
            },
            maxSize, timeout);

        return future;
    }

    bool cancelOperation(int id) {
        std::lock_guard<std::mutex> lock(asyncMutex);
        auto it = pendingOperations.find(id);
        if (it != pendingOperations.end()) {
            it->second->canceled = true;
            pendingOperations.erase(it);
            spdlog::info("Cancelled operation {}", id);
            return true;
        }
        return false;
    }

    int registerConnectionCallback(ConnectionCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        int id = nextCallbackId++;
        connectionCallbacks[id] = std::move(callback);
        spdlog::debug("Registered connection callback {}", id);
        return id;
    }

    bool unregisterConnectionCallback(int id) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        auto it = connectionCallbacks.find(id);
        if (it != connectionCallbacks.end()) {
            connectionCallbacks.erase(it);
            spdlog::debug("Unregistered connection callback {}", id);
            return true;
        }
        return false;
    }

    void notifyConnectionChange(bool connected, std::error_code ec) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        for (const auto& [id, callback] : connectionCallbacks) {
            try {
                callback(connected, ec);
            } catch (const std::exception& e) {
                spdlog::error("Exception in connection callback {}: {}", id,
                              e.what());
            }
        }
    }

    void updateWriteStats(
        size_t dataSize, size_t bytesWritten,
        const std::chrono::steady_clock::time_point& start_time) {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto latencyMs =
            std::chrono::duration<double, std::milli>(duration).count();

        stats.bytes_sent += bytesWritten;
        stats.avg_write_latency_ms =
            (stats.avg_write_latency_ms * stats.messages_sent + latencyMs) /
            (stats.messages_sent + 1);

        if (config.enable_compression && dataSize > bytesWritten) {
            stats.avg_compression_ratio = (stats.avg_compression_ratio +
                                           (dataSize * 100 / bytesWritten)) /
                                          2;
        }
    }

    void updateReadStats(
        size_t bytesRead,
        const std::chrono::steady_clock::time_point& start_time) {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto latencyMs =
            std::chrono::duration<double, std::milli>(duration).count();

        stats.bytes_received += bytesRead;

        size_t totalReads = stats.bytes_received / config.read_buffer_size + 1;
        stats.avg_read_latency_ms =
            (stats.avg_read_latency_ms * (totalReads - 1) + latencyMs) /
            totalReads;
    }

    std::string compressData(const std::string& data) {
#ifdef ENABLE_COMPRESSION
        std::string compressed;
        compressed.resize(compressBound(data.size()));

        uLongf compressedSize = compressed.size();
        int result = compress(
            reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
            reinterpret_cast<const Bytef*>(data.data()), data.size());

        if (result != Z_OK) {
            throw std::runtime_error("Compression failed");
        }

        compressed.resize(compressedSize);
        return compressed;
#else
        return data;
#endif
    }

    std::string decompressData(const std::string& data) {
#ifdef ENABLE_COMPRESSION
        std::string decompressed;
        decompressed.resize(data.size() * 4);  // Initial guess

        uLongf decompressedSize = decompressed.size();
        int result = uncompress(
            reinterpret_cast<Bytef*>(decompressed.data()), &decompressedSize,
            reinterpret_cast<const Bytef*>(data.data()), data.size());

        if (result != Z_OK) {
            throw std::runtime_error("Decompression failed");
        }

        decompressed.resize(decompressedSize);
        return decompressed;
#else
        return data;
#endif
    }

    std::string encryptData(const std::string& data) {
#ifdef ENABLE_ENCRYPTION
        // Simplified encryption example - in practice, use proper key
        // management
        return data;  // Placeholder implementation
#else
        return data;
#endif
    }

    std::string decryptData(const std::string& data) {
#ifdef ENABLE_ENCRYPTION
        // Simplified decryption example - in practice, use proper key
        // management
        return data;  // Placeholder implementation
#else
        return data;
#endif
    }

    void startAsyncThread() {
        asyncThread = std::jthread([this](std::stop_token stoken) {
            while (!stoken.stop_requested() && !stopAsyncThread) {
                std::unique_lock<std::mutex> lock(asyncMutex);
                asyncCondition.wait_for(lock, std::chrono::milliseconds(100));

                auto now = std::chrono::steady_clock::now();
                std::vector<int> timedOutOps;

                for (const auto& [id, op] : pendingOperations) {
                    if (op->timeout && now - op->start_time > *op->timeout) {
                        timedOutOps.push_back(id);
                    }
                }

                for (int id : timedOutOps) {
                    auto it = pendingOperations.find(id);
                    if (it != pendingOperations.end()) {
                        it->second->callback(
                            false, make_error_code(FifoError::Timeout), 0);
                        pendingOperations.erase(it);
                    }
                }
            }
        });
    }

    ClientConfig getConfig() const {
        std::lock_guard<std::mutex> lock(operationMutex);
        return config;
    }

    bool updateConfig(const ClientConfig& newConfig) {
        std::lock_guard<std::mutex> lock(operationMutex);
        config = newConfig;
        spdlog::info("Updated FIFO client configuration");
        return true;
    }

    ClientStats getStatistics() const {
        std::lock_guard<std::mutex> lock(operationMutex);
        return stats;
    }

    void resetStatistics() {
        std::lock_guard<std::mutex> lock(operationMutex);
        stats = {};
        spdlog::info("Reset FIFO client statistics");
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
        return -1;
    }
    return m_impl->writeAsync(data, std::move(callback), timeout);
}

std::future<type::expected<std::size_t, std::error_code>>
FifoClient::writeAsyncWithFuture(
    std::string_view data, std::optional<std::chrono::milliseconds> timeout) {
    if (!m_impl) {
        auto promise =
            std::promise<type::expected<std::size_t, std::error_code>>();
        promise.set_value(
            type::unexpected(make_error_code(FifoError::NotOpen)));
        return promise.get_future();
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
        return -1;
    }
    return m_impl->readAsync(std::move(callback), maxSize, timeout);
}

std::future<type::expected<std::string, std::error_code>>
FifoClient::readAsyncWithFuture(
    std::size_t maxSize, std::optional<std::chrono::milliseconds> timeout) {
    if (!m_impl) {
        auto promise =
            std::promise<type::expected<std::string, std::error_code>>();
        promise.set_value(
            type::unexpected(make_error_code(FifoError::NotOpen)));
        return promise.get_future();
    }
    return m_impl->readAsyncWithFuture(maxSize, timeout);
}

auto FifoClient::open(std::optional<std::chrono::milliseconds> timeout)
    -> type::expected<void, std::error_code> {
    if (!m_impl) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    try {
        m_impl->openFifo();
        return {};
    } catch (const std::system_error& e) {
        return type::unexpected(e.code());
    }
}

bool FifoClient::isOpen() const noexcept { return m_impl && m_impl->isOpen(); }

std::string_view FifoClient::getPath() const noexcept {
    if (!m_impl) {
        return "";
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

bool FifoClient::cancelOperation(int id) {
    if (!m_impl) {
        return false;
    }
    return m_impl->cancelOperation(id);
}

}  // namespace atom::connection
