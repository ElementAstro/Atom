#ifndef ATOM_CONNECTION_SHARED_MEMORY_HPP
#define ATOM_CONNECTION_SHARED_MEMORY_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <unordered_map>

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"
#include "atom/meta/concept.hpp"
#include "atom/type/noncopyable.hpp"

#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#else
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

namespace atom::connection {

/**
 * @brief Exception class for shared memory errors.
 */
class SharedMemoryException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;

    /**
     * @brief Specific error codes for shared memory operations.
     */
    enum class ErrorCode {
        CREATION_FAILED,  ///< Failed to create shared memory.
        MAPPING_FAILED,   ///< Failed to map shared memory.
        ACCESS_DENIED,    ///< Access to shared memory denied.
        TIMEOUT,          ///< Operation timed out.
        SIZE_ERROR,       ///< Size error in shared memory operation.
        ALREADY_EXISTS,   ///< Shared memory already exists.
        NOT_FOUND,        ///< Shared memory not found.
        UNKNOWN           ///< Unknown error.
    };

    /**
     * @brief Constructs a SharedMemoryException with detailed information.
     *
     * @param file Source file where the exception occurred.
     * @param line Line number where the exception occurred.
     * @param func Function name where the exception occurred.
     * @param message Error message.
     * @param code Specific error code.
     */
    SharedMemoryException(const char* file, int line, const char* func,
                          const std::string& message, ErrorCode code)
        : atom::error::Exception(file, line, func, message), code_(code) {}

    /**
     * @brief Gets the specific error code.
     *
     * @return ErrorCode The specific error code.
     */
    ATOM_NODISCARD auto getErrorCode() const ATOM_NOEXCEPT -> ErrorCode {
        return code_;
    }

    /**
     * @brief Gets the string representation of the error code.
     *
     * @return std::string The string representation of the error code.
     */
    ATOM_NODISCARD auto getErrorCodeString() const -> std::string {
        switch (code_) {
            case ErrorCode::CREATION_FAILED:
                return "CREATION_FAILED";
            case ErrorCode::MAPPING_FAILED:
                return "MAPPING_FAILED";
            case ErrorCode::ACCESS_DENIED:
                return "ACCESS_DENIED";
            case ErrorCode::TIMEOUT:
                return "TIMEOUT";
            case ErrorCode::SIZE_ERROR:
                return "SIZE_ERROR";
            case ErrorCode::ALREADY_EXISTS:
                return "ALREADY_EXISTS";
            case ErrorCode::NOT_FOUND:
                return "NOT_FOUND";
            default:
                return "UNKNOWN";
        }
    }

private:
    ErrorCode code_{ErrorCode::UNKNOWN};
};

#define THROW_SHARED_MEMORY_ERROR_WITH_CODE(message, code) \
    throw atom::connection::SharedMemoryException(         \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, message, code)

#define THROW_SHARED_MEMORY_ERROR(...)             \
    throw atom::connection::SharedMemoryException( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_SHARED_MEMORY_ERROR(...)               \
    atom::connection::SharedMemoryException::rethrowNested( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Performance statistics for SharedMemory operations
 */
struct alignas(CACHE_LINE_SIZE) SharedMemoryStats {
    std::atomic<size_t> read_operations{0};      ///< Total read operations
    std::atomic<size_t> write_operations{0};     ///< Total write operations
    std::atomic<size_t> lock_acquisitions{0};    ///< Lock acquisition attempts
    std::atomic<size_t> lock_timeouts{0};        ///< Lock timeout events
    std::atomic<size_t> version_conflicts{0};    ///< Version conflict events
    std::atomic<size_t> resize_operations{0};    ///< Resize operations
    std::atomic<size_t> callback_invocations{0}; ///< Change callback invocations
    std::atomic<uint64_t> total_read_time{0};    ///< Total read time (ns)
    std::atomic<uint64_t> total_write_time{0};   ///< Total write time (ns)
    std::atomic<uint64_t> total_lock_time{0};    ///< Total lock wait time (ns)
    std::atomic<uint64_t> max_read_time{0};      ///< Maximum read time (ns)
    std::atomic<uint64_t> max_write_time{0};     ///< Maximum write time (ns)
    std::atomic<uint64_t> max_lock_time{0};      ///< Maximum lock wait time (ns)
    std::atomic<size_t> memory_usage{0};         ///< Current memory usage
    std::atomic<size_t> peak_memory_usage{0};    ///< Peak memory usage

    void reset() noexcept {
        read_operations = 0; write_operations = 0; lock_acquisitions = 0;
        lock_timeouts = 0; version_conflicts = 0; resize_operations = 0;
        callback_invocations = 0; total_read_time = 0; total_write_time = 0;
        total_lock_time = 0; max_read_time = 0; max_write_time = 0;
        max_lock_time = 0; memory_usage = 0; peak_memory_usage = 0;
    }

    double getAverageReadTime() const noexcept {
        size_t count = read_operations.load();
        return count > 0 ? static_cast<double>(total_read_time.load()) / count : 0.0;
    }

    double getAverageWriteTime() const noexcept {
        size_t count = write_operations.load();
        return count > 0 ? static_cast<double>(total_write_time.load()) / count : 0.0;
    }

    double getAverageLockTime() const noexcept {
        size_t count = lock_acquisitions.load();
        return count > 0 ? static_cast<double>(total_lock_time.load()) / count : 0.0;
    }

    double getLockTimeoutRatio() const noexcept {
        size_t total = lock_acquisitions.load();
        return total > 0 ? static_cast<double>(lock_timeouts.load()) / total : 0.0;
    }

    // Create a copyable snapshot of the statistics
    void snapshot(SharedMemoryStats& copy) const noexcept {
        copy.read_operations.store(read_operations.load());
        copy.write_operations.store(write_operations.load());
        copy.lock_acquisitions.store(lock_acquisitions.load());
        copy.lock_timeouts.store(lock_timeouts.load());
        copy.version_conflicts.store(version_conflicts.load());
        copy.resize_operations.store(resize_operations.load());
        copy.callback_invocations.store(callback_invocations.load());
        copy.total_read_time.store(total_read_time.load());
        copy.total_write_time.store(total_write_time.load());
        copy.total_lock_time.store(total_lock_time.load());
        copy.max_read_time.store(max_read_time.load());
        copy.max_write_time.store(max_write_time.load());
        copy.max_lock_time.store(max_lock_time.load());
        copy.memory_usage.store(memory_usage.load());
        copy.peak_memory_usage.store(peak_memory_usage.load());
    }
};

/**
 * @brief Configuration for SharedMemory optimizations
 */
struct SharedMemoryConfig {
    bool enable_stats{true};              ///< Enable performance statistics
    bool enable_version_checking{true};   ///< Enable version conflict detection
    bool enable_memory_prefetching{true}; ///< Enable memory prefetching
    bool enable_auto_recovery{true};      ///< Enable automatic error recovery
    std::chrono::milliseconds default_timeout{1000}; ///< Default operation timeout
    std::chrono::milliseconds lock_retry_interval{1}; ///< Lock retry interval
    size_t max_retry_attempts{100};       ///< Maximum retry attempts for operations
    size_t memory_alignment{CACHE_LINE_SIZE}; ///< Memory alignment for performance
};

/**
 * @brief Enhanced header structure stored at the beginning of shared memory
 */
struct alignas(CACHE_LINE_SIZE) SharedMemoryHeader {
    std::atomic_flag accessLock;
    std::atomic<std::size_t> size;
    std::atomic<uint64_t> version;
    std::atomic<bool> initialized;
    std::atomic<uint64_t> creation_time;  ///< Creation timestamp
    std::atomic<uint64_t> last_access_time; ///< Last access timestamp
    std::atomic<size_t> access_count;     ///< Total access count
    std::atomic<uint32_t> checksum;       ///< Data integrity checksum
    char creator_info[64];                ///< Creator process information
    char reserved[64];                    ///< Reserved for future use
};

/**
 * @brief Enhanced cross-platform shared memory implementation with advanced features.
 *
 * Features:
 * - Comprehensive performance monitoring and statistics
 * - Enhanced error handling and automatic recovery
 * - Cross-platform compatibility optimizations
 * - Memory integrity checking with checksums
 * - Configurable timeouts and retry mechanisms
 * - Cache-aligned data structures for better performance
 *
 * @tparam T The type of data stored in shared memory, must be trivially copyable.
 */
template <TriviallyCopyable T>
class SharedMemory : public NonCopyable {
public:
    using ChangeCallback = std::function<void(const T&)>;

    /**
     * @brief Constructs a new SharedMemory object with enhanced configuration.
     *
     * @param name The name of the shared memory.
     * @param create Whether to create new shared memory.
     * @param initialData Optional initial data to write to shared memory.
     * @param config Configuration options for performance and behavior.
     */
    explicit SharedMemory(std::string_view name, bool create = true,
                          const std::optional<T>& initialData = std::nullopt,
                          const SharedMemoryConfig& config = SharedMemoryConfig{});

    /**
     * @brief Destructor for SharedMemory.
     */
    ~SharedMemory() override;

    /**
     * @brief Writes data to shared memory.
     *
     * @param data The data to write.
     * @param timeout The operation timeout.
     * @param notifyListeners Whether to notify listeners.
     */
    void write(const T& data,
               std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
               bool notifyListeners = true);

    /**
     * @brief Reads data from shared memory.
     *
     * @param timeout The operation timeout.
     * @return The data read from shared memory.
     */
    ATOM_NODISCARD auto read(std::chrono::milliseconds timeout =
                                 std::chrono::milliseconds(0)) const -> T;

    /**
     * @brief Clears the data in shared memory.
     */
    void clear();

    /**
     * @brief Checks if the shared memory is occupied by another process.
     *
     * @return True if occupied, false otherwise.
     */
    ATOM_NODISCARD auto isOccupied() const -> bool;

    /**
     * @brief Gets the name of the shared memory.
     *
     * @return The name of the shared memory.
     */
    ATOM_NODISCARD auto getName() const ATOM_NOEXCEPT -> std::string_view;

    /**
     * @brief Gets the size of the shared memory.
     *
     * @return The size of the shared memory.
     */
    ATOM_NODISCARD auto getSize() const ATOM_NOEXCEPT -> std::size_t;

    /**
     * @brief Gets the version number of the shared memory data.
     *
     * @return The version number of the shared memory data.
     */
    ATOM_NODISCARD auto getVersion() const ATOM_NOEXCEPT -> uint64_t;

    /**
     * @brief Checks if the current process is the creator of the shared memory.
     *
     * @return True if the current process is the creator, false otherwise.
     */
    ATOM_NODISCARD auto isCreator() const ATOM_NOEXCEPT -> bool;

    /**
     * @brief Checks if shared memory with the specified name exists.
     *
     * @param name The name of the shared memory.
     * @return True if the shared memory exists, false otherwise.
     */
    ATOM_NODISCARD static auto exists(std::string_view name) -> bool;

    /**
     * @brief Writes partial data to shared memory.
     *
     * @tparam U The type of the partial data.
     * @param data The partial data to write.
     * @param offset The offset at which to write the data.
     * @param timeout The operation timeout.
     */
    template <typename U>
    void writePartial(
        const U& data, std::size_t offset,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Reads partial data from shared memory.
     *
     * @tparam U The type of the partial data.
     * @param offset The offset from which to read the data.
     * @param timeout The operation timeout.
     * @return The partial data read from shared memory.
     */
    template <typename U>
    ATOM_NODISCARD auto readPartial(
        std::size_t offset,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) const
        -> U;

    /**
     * @brief Tries to read data from shared memory without throwing exceptions.
     *
     * @param timeout The operation timeout.
     * @return The data read from shared memory or std::nullopt if an error
     * occurs.
     */
    ATOM_NODISCARD auto tryRead(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) const
        -> std::optional<T>;

    /**
     * @brief Writes binary data to shared memory.
     *
     * @param data The binary data to write.
     * @param timeout The operation timeout.
     */
    void writeSpan(
        std::span<const std::byte> data,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Reads binary data from shared memory.
     *
     * @param data The buffer to receive the data.
     * @param timeout The operation timeout.
     * @return The number of bytes actually read.
     */
    ATOM_NODISCARD auto readSpan(
        std::span<std::byte> data,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) const
        -> std::size_t;

    /**
     * @brief Resizes the shared memory.
     *
     * @param newSize The new size of the shared memory.
     */
    void resize(std::size_t newSize);

    /**
     * @brief Executes a function safely under lock.
     *
     * @tparam Func The type of the function.
     * @param func The function to execute.
     * @param timeout The operation timeout.
     * @return The return value of the function.
     */
    template <typename Func>
    auto withLock(Func&& func, std::chrono::milliseconds timeout) const
        -> decltype(std::forward<Func>(func)());

    /**
     * @brief Asynchronously reads data from shared memory.
     *
     * @param timeout The operation timeout.
     * @return A future containing the read result.
     */
    auto readAsync(std::chrono::milliseconds timeout =
                       std::chrono::milliseconds(0)) -> std::future<T>;

    /**
     * @brief Asynchronously writes data to shared memory.
     *
     * @param data The data to write.
     * @param timeout The operation timeout.
     * @return A future indicating the completion of the operation.
     */
    auto writeAsync(const T& data, std::chrono::milliseconds timeout =
                                       std::chrono::milliseconds(0))
        -> std::future<void>;

    /**
     * @brief Registers a change callback.
     *
     * @param callback The callback function to call when data changes.
     * @return The callback ID for later unregistration.
     */
    auto registerChangeCallback(ChangeCallback callback) -> std::size_t;

    /**
     * @brief Unregisters a change callback.
     *
     * @param callbackId The callback ID to unregister.
     * @return True if the callback was successfully unregistered, false
     * otherwise.
     */
    auto unregisterChangeCallback(std::size_t callbackId) -> bool;

    /**
     * @brief Waits for data to change.
     *
     * @param timeout The wait timeout.
     * @return True if data changed, false if the timeout expired.
     */
    auto waitForChange(std::chrono::milliseconds timeout =
                           std::chrono::milliseconds(0)) -> bool;

    /**
     * @brief Gets the platform-specific handle for the shared memory.
     *
     * @return The platform-specific handle.
     */
    auto getNativeHandle() const -> void*;

    /**
     * @brief Checks if the shared memory is initialized.
     *
     * @return True if initialized, false otherwise.
     */
    ATOM_NODISCARD auto isInitialized() const -> bool;

    /**
     * @brief Gets the pointer to the actual data in shared memory.
     *
     * @return Pointer to the data.
     */
    void* getDataPtr() const {
        return static_cast<char*>(buffer_) + sizeof(SharedMemoryHeader);
    }

    /**
     * @brief Notifies all registered listeners about data changes
     * @param data The new data to notify about
     */
    void notifyListeners(const T& data) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        for (const auto& [id, callback] : changeCallbacks_) {
            try {
                callback(data);
            } catch (const std::exception& e) {
                spdlog::error(
                    "Exception in change callback for shared memory {}: {}",
                    name_, e.what());
            }
        }
    }

private:
    std::string name_;
    std::size_t totalSize_;

#ifdef _WIN32
    HANDLE handle_{nullptr};
    HANDLE changeEvent_{nullptr};
#else
    int fd_{-1};
    sem_t* semId_{SEM_FAILED};
#endif

    void* buffer_{nullptr};
    SharedMemoryHeader* header_{nullptr};
    mutable std::mutex mutex_;
    mutable std::condition_variable changeCondition_;
    bool isCreator_{false};
    mutable uint64_t lastKnownVersion_{0};

    std::vector<std::pair<std::size_t, ChangeCallback>> changeCallbacks_;
    std::size_t nextCallbackId_{1};
    mutable std::mutex callbackMutex_;

    std::jthread watchThread_;
    std::atomic<bool> stopWatching_{false};

    // Enhanced features
    SharedMemoryConfig config_;
    mutable SharedMemoryStats stats_;
    std::unordered_map<std::string, std::string> metadata_;
    mutable std::atomic<uint64_t> last_operation_time_{0};

    void unmap() noexcept;
    void mapMemory(bool create, std::size_t size);
    void startWatchThread();
    void watchForChanges();
    void platformSpecificInit();
    void platformSpecificCleanup() noexcept;
    static std::string getLastErrorMessage();

    // Enhanced helper methods
    void updateTimingStats(uint64_t duration, bool is_read) const noexcept;
    uint32_t calculateChecksum(const void* data, size_t size) const noexcept;
    void validateDataIntegrity() const;
    void initializeCreatorInfo();
    void handleRecoveryOperation();

public:
    /**
     * @brief Get performance statistics
     *
     * @param stats Reference to statistics structure to fill
     */
    void getStats(SharedMemoryStats& stats) const {
        std::lock_guard lock(mutex_);
        stats_.snapshot(stats);
    }

    /**
     * @brief Reset performance statistics
     */
    void resetStats() {
        std::lock_guard lock(mutex_);
        stats_.reset();
    }

    /**
     * @brief Get performance metrics
     *
     * @return Tuple of (avg_read_time, avg_write_time, avg_lock_time, lock_timeout_ratio)
     */
    [[nodiscard]] auto getPerformanceMetrics() const -> std::tuple<double, double, double, double> {
        std::lock_guard lock(mutex_);
        return std::make_tuple(
            stats_.getAverageReadTime(),
            stats_.getAverageWriteTime(),
            stats_.getAverageLockTime(),
            stats_.getLockTimeoutRatio()
        );
    }

    /**
     * @brief Get memory usage information
     *
     * @return Tuple of (current_usage, peak_usage, total_size)
     */
    [[nodiscard]] auto getMemoryUsage() const -> std::tuple<size_t, size_t, size_t> {
        std::lock_guard lock(mutex_);
        return std::make_tuple(
            stats_.memory_usage.load(),
            stats_.peak_memory_usage.load(),
            totalSize_
        );
    }

    /**
     * @brief Get current configuration
     *
     * @return Current configuration settings
     */
    [[nodiscard]] const SharedMemoryConfig& getConfig() const noexcept {
        return config_;
    }

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration to apply
     */
    void updateConfig(const SharedMemoryConfig& new_config) {
        std::lock_guard lock(mutex_);
        config_ = new_config;
    }

    /**
     * @brief Validate data integrity using checksum
     *
     * @return True if data integrity is valid
     */
    [[nodiscard]] bool validateIntegrity() const {
        if (!config_.enable_version_checking) {
            return true;  // Validation disabled
        }

        try {
            validateDataIntegrity();
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Get metadata about the shared memory
     *
     * @return Map of metadata key-value pairs
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> getMetadata() const {
        std::lock_guard lock(mutex_);
        auto result = metadata_;

        // Add runtime metadata
        result["creation_time"] = std::to_string(header_->creation_time.load());
        result["last_access_time"] = std::to_string(header_->last_access_time.load());
        result["access_count"] = std::to_string(header_->access_count.load());
        result["version"] = std::to_string(header_->version.load());
        result["size"] = std::to_string(totalSize_);
        result["is_creator"] = isCreator_ ? "true" : "false";

        return result;
    }

    /**
     * @brief Set metadata for the shared memory
     *
     * @param key Metadata key
     * @param value Metadata value
     */
    void setMetadata(const std::string& key, const std::string& value) {
        std::lock_guard lock(mutex_);
        metadata_[key] = value;
    }
};

template <TriviallyCopyable T>
SharedMemory<T>::SharedMemory(std::string_view name, bool create,
                              const std::optional<T>& initialData,
                              const SharedMemoryConfig& config)
    : name_(name), isCreator_(create), config_(config) {
    totalSize_ = sizeof(SharedMemoryHeader) + sizeof(T);

    try {
        mapMemory(create, totalSize_);
        platformSpecificInit();

        // Initialize enhanced header fields if creating
        if (create) {
            initializeCreatorInfo();
            header_->creation_time.store(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count(),
                std::memory_order_release);
        }

        if (create && initialData) {
            withLock(
                [&]() {
                    std::memcpy(getDataPtr(), &(*initialData), sizeof(T));
                    header_->initialized.store(true, std::memory_order_release);
                    header_->version.fetch_add(1, std::memory_order_release);

                    // Calculate and store checksum for data integrity
                    if (config_.enable_version_checking) {
                        uint32_t checksum = calculateChecksum(getDataPtr(), sizeof(T));
                        header_->checksum.store(checksum, std::memory_order_release);
                    }

                    header_->last_access_time.store(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count(),
                        std::memory_order_release);

                    spdlog::info(
                        "Initialized shared memory '{}' with initial data",
                        name_);
                },
                config_.default_timeout);
        }

        // Update memory usage statistics
        if (config_.enable_stats) {
            stats_.memory_usage.store(totalSize_, std::memory_order_relaxed);
            size_t current_peak = stats_.peak_memory_usage.load(std::memory_order_relaxed);
            while (totalSize_ > current_peak &&
                   !stats_.peak_memory_usage.compare_exchange_weak(current_peak, totalSize_,
                                                                  std::memory_order_relaxed)) {
                // Keep trying until we successfully update or find a larger value
            }
        }

        startWatchThread();
    } catch (...) {
        unmap();
        platformSpecificCleanup();
        throw;
    }
}

template <TriviallyCopyable T>
SharedMemory<T>::~SharedMemory() {
    stopWatching_ = true;
    if (watchThread_.joinable()) {
        watchThread_.join();
    }

    unmap();
    platformSpecificCleanup();
}

template <TriviallyCopyable T>
void SharedMemory<T>::platformSpecificInit() {
#ifdef _WIN32
    std::string eventName = name_ + "_event";
    changeEvent_ = CreateEventA(nullptr, TRUE, FALSE, eventName.c_str());
    if (!changeEvent_) {
        spdlog::warn("Failed to create change event for shared memory: {}",
                     getLastErrorMessage());
    }
#else
    std::string semName = "/" + name_ + "_sem";
    semId_ = sem_open(semName.c_str(), O_CREAT, 0666, 0);
    if (semId_ == SEM_FAILED) {
        spdlog::warn("Failed to create semaphore for shared memory: {}",
                     strerror(errno));
    }
#endif
}

template <TriviallyCopyable T>
void SharedMemory<T>::platformSpecificCleanup() noexcept {
#ifdef _WIN32
    if (changeEvent_) {
        CloseHandle(changeEvent_);
        changeEvent_ = nullptr;
    }
#else
    if (semId_ != SEM_FAILED) {
        std::string semName = "/" + name_ + "_sem";
        sem_close(semId_);
        if (isCreator_) {
            sem_unlink(semName.c_str());
        }
        semId_ = SEM_FAILED;
    }
#endif
}

template <TriviallyCopyable T>
std::string SharedMemory<T>::getLastErrorMessage() {
#ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0)
        return "No error";

    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

    std::string message(buffer, size);
    LocalFree(buffer);
    return message;
#else
    return strerror(errno);
#endif
}

template <TriviallyCopyable T>
void SharedMemory<T>::unmap() noexcept {
#ifdef _WIN32
    if (buffer_) {
        UnmapViewOfFile(buffer_);
        buffer_ = nullptr;
    }
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#else
    if (buffer_ != nullptr) {
        munmap(buffer_, totalSize_);
        buffer_ = nullptr;
    }
    if (fd_ != -1) {
        close(fd_);
        if (isCreator_) {
            shm_unlink(name_.c_str());
        }
        fd_ = -1;
    }
#endif
    header_ = nullptr;
}

template <TriviallyCopyable T>
void SharedMemory<T>::mapMemory(bool create, std::size_t size) {
#ifdef _WIN32
    handle_ =
        create
            ? CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                 0, static_cast<DWORD>(size), name_.c_str())
            : OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());

    if (handle_ == nullptr) {
        auto error = GetLastError();
        if (create && error == ERROR_ALREADY_EXISTS) {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Shared memory already exists: " + name_,
                SharedMemoryException::ErrorCode::ALREADY_EXISTS);
        } else if (!create && error == ERROR_FILE_NOT_FOUND) {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Shared memory not found: " + name_,
                SharedMemoryException::ErrorCode::NOT_FOUND);
        } else {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Failed to create/open file mapping: " + name_ + " - " +
                    getLastErrorMessage(),
                SharedMemoryException::ErrorCode::CREATION_FAILED);
        }
    }

    buffer_ = MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (buffer_ == nullptr) {
        CloseHandle(handle_);
        handle_ = nullptr;
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to map view of file: " + name_ + " - " +
                getLastErrorMessage(),
            SharedMemoryException::ErrorCode::MAPPING_FAILED);
    }
#else
    fd_ = shm_open(name_.c_str(), create ? (O_CREAT | O_RDWR) : O_RDWR,
                   S_IRUSR | S_IWUSR);

    if (fd_ == -1) {
        if (create && errno == EEXIST) {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Shared memory already exists: " + std::string(name_),
                SharedMemoryException::ErrorCode::ALREADY_EXISTS);
        } else if (!create && errno == ENOENT) {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Shared memory not found: " + std::string(name_),
                SharedMemoryException::ErrorCode::NOT_FOUND);
        } else {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Failed to create/open shared memory: " + std::string(name_) +
                    " - " + strerror(errno),
                SharedMemoryException::ErrorCode::CREATION_FAILED);
        }
    }

    if (create && ftruncate(fd_, size) == -1) {
        close(fd_);
        if (create) {
            shm_unlink(name_.c_str());
        }
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to resize shared memory: " + std::string(name_) + " - " +
                strerror(errno),
            SharedMemoryException::ErrorCode::SIZE_ERROR);
    }

    buffer_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (buffer_ == MAP_FAILED) {
        close(fd_);
        if (create) {
            shm_unlink(name_.c_str());
        }
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to map shared memory: " + std::string(name_) + " - " +
                strerror(errno),
            SharedMemoryException::ErrorCode::MAPPING_FAILED);
    }
#endif

    header_ = static_cast<SharedMemoryHeader*>(buffer_);

    if (create) {
        new (header_) SharedMemoryHeader();
        header_->size.store(sizeof(T), std::memory_order_release);
        header_->version.store(0, std::memory_order_release);
        header_->initialized.store(false, std::memory_order_release);
    }

    totalSize_ = size;
}

template <TriviallyCopyable T>
void SharedMemory<T>::resize(std::size_t newSize) {
    std::size_t totalNewSize = sizeof(SharedMemoryHeader) + newSize;

    if (!isCreator_) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Only the creator can resize shared memory",
            SharedMemoryException::ErrorCode::ACCESS_DENIED);
    }

    T currentData;
    bool wasInitialized = false;

    if (isInitialized()) {
        currentData = read(std::chrono::milliseconds(100));
        wasInitialized = true;
    }

    unmap();

#ifdef _WIN32
    handle_ =
        CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           static_cast<DWORD>(totalNewSize), name_.c_str());
    if (handle_ == nullptr) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to resize file mapping: " + name_ + " - " +
                getLastErrorMessage(),
            SharedMemoryException::ErrorCode::CREATION_FAILED);
    }

    buffer_ = MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, totalNewSize);
    if (buffer_ == nullptr) {
        CloseHandle(handle_);
        handle_ = nullptr;
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to remap view of file: " + name_ + " - " +
                getLastErrorMessage(),
            SharedMemoryException::ErrorCode::MAPPING_FAILED);
    }
#else
    fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd_ == -1) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to reopen shared memory: " + std::string(name_) + " - " +
                strerror(errno),
            SharedMemoryException::ErrorCode::CREATION_FAILED);
    }

    if (ftruncate(fd_, totalNewSize) == -1) {
        close(fd_);
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to resize shared memory: " + std::string(name_) + " - " +
                strerror(errno),
            SharedMemoryException::ErrorCode::SIZE_ERROR);
    }

    buffer_ =
        mmap(nullptr, totalNewSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (buffer_ == MAP_FAILED) {
        close(fd_);
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Failed to remap shared memory: " + std::string(name_) + " - " +
                strerror(errno),
            SharedMemoryException::ErrorCode::MAPPING_FAILED);
    }
#endif

    header_ = static_cast<SharedMemoryHeader*>(buffer_);
    new (header_) SharedMemoryHeader();
    header_->size.store(newSize, std::memory_order_release);
    header_->version.store(0, std::memory_order_release);

    if (wasInitialized) {
        std::size_t copySize = std::min(newSize, sizeof(T));
        std::memcpy(getDataPtr(), &currentData, copySize);
        header_->initialized.store(true, std::memory_order_release);
        header_->version.fetch_add(1, std::memory_order_release);
    } else {
        header_->initialized.store(false, std::memory_order_release);
    }

    totalSize_ = totalNewSize;
    spdlog::info("Shared memory '{}' resized to {} bytes", name_, newSize);
}

template <TriviallyCopyable T>
ATOM_NODISCARD bool SharedMemory<T>::exists(std::string_view name) {
#ifdef _WIN32
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.data());
    if (h) {
        CloseHandle(h);
        return true;
    }
    return false;
#else
    int fd = shm_open(name.data(), O_RDONLY, 0);
    if (fd != -1) {
        close(fd);
        return true;
    }
    return false;
#endif
}

template <TriviallyCopyable T>
template <typename Func>
auto SharedMemory<T>::withLock(Func&& func,
                               std::chrono::milliseconds timeout) const
    -> decltype(std::forward<Func>(func)()) {
    auto lock_start_time = std::chrono::high_resolution_clock::now();

    if (config_.enable_stats) {
        stats_.lock_acquisitions.fetch_add(1, std::memory_order_relaxed);
    }

    std::unique_lock lock(mutex_);
    auto startTime = std::chrono::steady_clock::now();
    size_t retry_count = 0;

    while (header_->accessLock.test_and_set(std::memory_order_acquire)) {
        if (timeout != std::chrono::milliseconds(0) &&
            std::chrono::steady_clock::now() - startTime >= timeout) {
            if (config_.enable_stats) {
                stats_.lock_timeouts.fetch_add(1, std::memory_order_relaxed);
            }

            // Attempt auto-recovery if enabled
            if (config_.enable_auto_recovery && retry_count < config_.max_retry_attempts) {
                handleRecoveryOperation();
                ++retry_count;
                startTime = std::chrono::steady_clock::now();  // Reset timeout
                continue;
            }

            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Failed to acquire mutex within timeout for shared memory: " +
                    name_ + " (retries: " + std::to_string(retry_count) + ")",
                SharedMemoryException::ErrorCode::TIMEOUT);
        }
        std::this_thread::sleep_for(config_.lock_retry_interval);
    }

    // Update lock timing statistics
    if (config_.enable_stats) {
        auto lock_end_time = std::chrono::high_resolution_clock::now();
        auto lock_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            lock_end_time - lock_start_time).count();
        stats_.total_lock_time.fetch_add(lock_duration, std::memory_order_relaxed);

        uint64_t current_max = stats_.max_lock_time.load(std::memory_order_relaxed);
        while (lock_duration > current_max &&
               !stats_.max_lock_time.compare_exchange_weak(current_max, lock_duration,
                                                          std::memory_order_relaxed)) {
            // Keep trying until we successfully update or find a larger value
        }
    }

    try {
        // Update last access time
        header_->last_access_time.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_relaxed);
        header_->access_count.fetch_add(1, std::memory_order_relaxed);

        if constexpr (std::is_void_v<decltype(std::forward<Func>(func)())>) {
            std::forward<Func>(func)();
            header_->accessLock.clear(std::memory_order_release);
        } else {
            auto result = std::forward<Func>(func)();
            header_->accessLock.clear(std::memory_order_release);
            return result;
        }
    } catch (...) {
        header_->accessLock.clear(std::memory_order_release);
        throw;
    }
}

template <TriviallyCopyable T>
void SharedMemory<T>::write(const T& data, std::chrono::milliseconds timeout,
                            bool notifyListeners) {
    withLock(
        [&]() {
            std::memcpy(getDataPtr(), &data, sizeof(T));
            header_->initialized.store(true, std::memory_order_release);
            header_->version.fetch_add(1, std::memory_order_release);
            spdlog::debug("Data written to shared memory: {} (version {})",
                          name_, header_->version.load());

#ifdef _WIN32
            if (changeEvent_) {
                SetEvent(changeEvent_);
                ResetEvent(changeEvent_);
            }
#else
            if (semId_ != SEM_FAILED) {
                sem_post(semId_);
            }
#endif
        },
        timeout);

    if (notifyListeners) {
        notifyListeners(data);
        changeCondition_.notify_all();
    }
}

template <TriviallyCopyable T>
auto SharedMemory<T>::read(std::chrono::milliseconds timeout) const -> T {
    return withLock(
        [&]() -> T {
            if (!header_->initialized.load(std::memory_order_acquire)) {
                THROW_SHARED_MEMORY_ERROR(
                    "Shared memory not initialized yet: " + name_);
            }

            T data;
            std::memcpy(&data, getDataPtr(), sizeof(T));
            lastKnownVersion_ =
                header_->version.load(std::memory_order_acquire);
            spdlog::debug("Data read from shared memory: {} (version {})",
                          name_, lastKnownVersion_);
            return data;
        },
        timeout);
}

template <TriviallyCopyable T>
void SharedMemory<T>::clear() {
    withLock(
        [&]() {
            std::memset(getDataPtr(), 0, sizeof(T));
            header_->version.fetch_add(1, std::memory_order_release);
            header_->initialized.store(false, std::memory_order_release);
            spdlog::info("Shared memory cleared: {}", name_);

#ifdef _WIN32
            if (changeEvent_) {
                SetEvent(changeEvent_);
                ResetEvent(changeEvent_);
            }
#else
            if (semId_ != SEM_FAILED) {
                sem_post(semId_);
            }
#endif
        },
        std::chrono::milliseconds(0));

    changeCondition_.notify_all();
}

template <TriviallyCopyable T>
auto SharedMemory<T>::isOccupied() const -> bool {
    return header_->accessLock.test(std::memory_order_acquire);
}

template <TriviallyCopyable T>
auto SharedMemory<T>::getName() const ATOM_NOEXCEPT -> std::string_view {
    return name_;
}

template <TriviallyCopyable T>
auto SharedMemory<T>::getSize() const ATOM_NOEXCEPT -> std::size_t {
    return header_->size.load(std::memory_order_acquire);
}

template <TriviallyCopyable T>
auto SharedMemory<T>::getVersion() const ATOM_NOEXCEPT -> uint64_t {
    return header_->version.load(std::memory_order_acquire);
}

template <TriviallyCopyable T>
auto SharedMemory<T>::isCreator() const ATOM_NOEXCEPT -> bool {
    return isCreator_;
}

template <TriviallyCopyable T>
auto SharedMemory<T>::isInitialized() const -> bool {
    return header_->initialized.load(std::memory_order_acquire);
}

template <TriviallyCopyable T>
template <typename U>
void SharedMemory<T>::writePartial(const U& data, std::size_t offset,
                                   std::chrono::milliseconds timeout) {
    static_assert(std::is_trivially_copyable_v<U>,
                  "U must be trivially copyable");

    if (offset + sizeof(U) > sizeof(T)) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Partial write out of bounds: offset " + std::to_string(offset) +
                " + size " + std::to_string(sizeof(U)) + " exceeds " +
                std::to_string(sizeof(T)),
            SharedMemoryException::ErrorCode::SIZE_ERROR);
    }

    withLock(
        [&]() {
            std::memcpy(static_cast<char*>(getDataPtr()) + offset, &data,
                        sizeof(U));
            header_->initialized.store(true, std::memory_order_release);
            header_->version.fetch_add(1, std::memory_order_release);
            spdlog::debug(
                "Partial data written to shared memory: {} (offset: {}, size: "
                "{})",
                name_, offset, sizeof(U));

#ifdef _WIN32
            if (changeEvent_) {
                SetEvent(changeEvent_);
                ResetEvent(changeEvent_);
            }
#else
            if (semId_ != SEM_FAILED) {
                sem_post(semId_);
            }
#endif
        },
        timeout);

    changeCondition_.notify_all();
}

template <TriviallyCopyable T>
template <typename U>
auto SharedMemory<T>::readPartial(std::size_t offset,
                                  std::chrono::milliseconds timeout) const
    -> U {
    static_assert(std::is_trivially_copyable_v<U>,
                  "U must be trivially copyable");

    if (offset + sizeof(U) > sizeof(T)) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Partial read out of bounds: offset " + std::to_string(offset) +
                " + size " + std::to_string(sizeof(U)) + " exceeds " +
                std::to_string(sizeof(T)),
            SharedMemoryException::ErrorCode::SIZE_ERROR);
    }

    return withLock(
        [&]() -> U {
            if (!header_->initialized.load(std::memory_order_acquire)) {
                THROW_SHARED_MEMORY_ERROR(
                    "Shared memory not initialized yet: " + name_);
            }

            U data;
            std::memcpy(&data, static_cast<const char*>(getDataPtr()) + offset,
                        sizeof(U));
            spdlog::debug(
                "Partial data read from shared memory: {} (offset: {}, size: "
                "{})",
                name_, offset, sizeof(U));
            return data;
        },
        timeout);
}

template <TriviallyCopyable T>
auto SharedMemory<T>::tryRead(std::chrono::milliseconds timeout) const
    -> std::optional<T> {
    try {
        return read(timeout);
    } catch (const SharedMemoryException& e) {
        spdlog::warn("Try read failed: {} ({})", e.what(),
                     e.getErrorCodeString());
        return std::nullopt;
    }
}

template <TriviallyCopyable T>
void SharedMemory<T>::writeSpan(std::span<const std::byte> data,
                                std::chrono::milliseconds timeout) {
    if (data.size_bytes() > sizeof(T)) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Span write out of bounds: size " +
                std::to_string(data.size_bytes()) + " exceeds " +
                std::to_string(sizeof(T)),
            SharedMemoryException::ErrorCode::SIZE_ERROR);
    }

    withLock(
        [&]() {
            std::memcpy(getDataPtr(), data.data(), data.size_bytes());
            header_->initialized.store(true, std::memory_order_release);
            header_->version.fetch_add(1, std::memory_order_release);
            spdlog::debug("Span data written to shared memory: {} (size: {})",
                          name_, data.size_bytes());

#ifdef _WIN32
            if (changeEvent_) {
                SetEvent(changeEvent_);
                ResetEvent(changeEvent_);
            }
#else
            if (semId_ != SEM_FAILED) {
                sem_post(semId_);
            }
#endif
        },
        timeout);

    changeCondition_.notify_all();
}

template <TriviallyCopyable T>
auto SharedMemory<T>::readSpan(std::span<std::byte> data,
                               std::chrono::milliseconds timeout) const
    -> std::size_t {
    return withLock(
        [&]() -> std::size_t {
            if (!header_->initialized.load(std::memory_order_acquire)) {
                THROW_SHARED_MEMORY_ERROR(
                    "Shared memory not initialized yet: " + name_);
            }

            std::size_t bytesToRead = std::min(data.size_bytes(), sizeof(T));
            std::memcpy(data.data(), getDataPtr(), bytesToRead);
            spdlog::debug("Span data read from shared memory: {} (size: {})",
                          name_, bytesToRead);
            return bytesToRead;
        },
        timeout);
}

template <TriviallyCopyable T>
auto SharedMemory<T>::readAsync(std::chrono::milliseconds timeout)
    -> std::future<T> {
    return std::async(std::launch::async,
                      [this, timeout]() -> T { return this->read(timeout); });
}

template <TriviallyCopyable T>
auto SharedMemory<T>::writeAsync(const T& data,
                                 std::chrono::milliseconds timeout)
    -> std::future<void> {
    return std::async(std::launch::async,
                      [this, data, timeout]() { this->write(data, timeout); });
}

template <TriviallyCopyable T>
auto SharedMemory<T>::registerChangeCallback(ChangeCallback callback)
    -> std::size_t {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    std::size_t id = nextCallbackId_++;
    changeCallbacks_.emplace_back(id, std::move(callback));
    return id;
}

template <TriviallyCopyable T>
auto SharedMemory<T>::unregisterChangeCallback(std::size_t callbackId) -> bool {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    auto it = std::find_if(
        changeCallbacks_.begin(), changeCallbacks_.end(),
        [callbackId](const auto& pair) { return pair.first == callbackId; });
    if (it != changeCallbacks_.end()) {
        changeCallbacks_.erase(it);
        return true;
    }
    return false;
}

template <TriviallyCopyable T>
void SharedMemory<T>::watchForChanges() {
    while (!stopWatching_) {
#ifdef _WIN32
        if (changeEvent_) {
            if (WaitForSingleObject(changeEvent_, 100) == WAIT_OBJECT_0) {
                uint64_t currentVersion =
                    header_->version.load(std::memory_order_acquire);
                if (currentVersion != lastKnownVersion_) {
                    try {
                        T data = read(std::chrono::milliseconds(50));
                        notifyListeners(data);
                        changeCondition_.notify_all();
                    } catch (const std::exception& e) {
                        spdlog::error(
                            "Exception while reading changed data: {}",
                            e.what());
                    }
                }
                ResetEvent(changeEvent_);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            uint64_t currentVersion =
                header_->version.load(std::memory_order_acquire);
            if (currentVersion != lastKnownVersion_) {
                try {
                    T data = read(std::chrono::milliseconds(50));
                    notifyListeners(data);
                    changeCondition_.notify_all();
                    lastKnownVersion_ = currentVersion;
                } catch (const std::exception& e) {
                    spdlog::error("Exception while reading changed data: {}",
                                  e.what());
                }
            }
        }
#else
        if (semId_ != SEM_FAILED) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100 * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            if (sem_timedwait(semId_, &ts) == 0) {
                uint64_t currentVersion =
                    header_->version.load(std::memory_order_acquire);
                if (currentVersion != lastKnownVersion_) {
                    try {
                        T data = read(std::chrono::milliseconds(50));
                        notifyListeners(data);
                        changeCondition_.notify_all();
                        lastKnownVersion_ = currentVersion;
                    } catch (const std::exception& e) {
                        spdlog::error(
                            "Exception while reading changed data: {}",
                            e.what());
                    }
                }
            } else if (errno != ETIMEDOUT) {
                spdlog::warn("sem_timedwait failed: {}", strerror(errno));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            uint64_t currentVersion =
                header_->version.load(std::memory_order_acquire);
            if (currentVersion != lastKnownVersion_) {
                try {
                    T data = read(std::chrono::milliseconds(50));
                    notifyListeners(data);
                    changeCondition_.notify_all();
                    lastKnownVersion_ = currentVersion;
                } catch (const std::exception& e) {
                    spdlog::error("Exception while reading changed data: {}",
                                  e.what());
                }
            }
        }
#endif
    }
}

template <TriviallyCopyable T>
auto SharedMemory<T>::getNativeHandle() const -> void* {
#ifdef _WIN32
    return handle_;
#else
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd_));
#endif
}

// Implementation of enhanced helper methods

template <TriviallyCopyable T>
void SharedMemory<T>::updateTimingStats(uint64_t duration, bool is_read) const noexcept {
    if (!config_.enable_stats) return;

    if (is_read) {
        stats_.total_read_time.fetch_add(duration, std::memory_order_relaxed);
        uint64_t current_max = stats_.max_read_time.load(std::memory_order_relaxed);
        while (duration > current_max &&
               !stats_.max_read_time.compare_exchange_weak(current_max, duration,
                                                          std::memory_order_relaxed)) {
            // Keep trying until we successfully update or find a larger value
        }
    } else {
        stats_.total_write_time.fetch_add(duration, std::memory_order_relaxed);
        uint64_t current_max = stats_.max_write_time.load(std::memory_order_relaxed);
        while (duration > current_max &&
               !stats_.max_write_time.compare_exchange_weak(current_max, duration,
                                                           std::memory_order_relaxed)) {
            // Keep trying until we successfully update or find a larger value
        }
    }
}

template <TriviallyCopyable T>
uint32_t SharedMemory<T>::calculateChecksum(const void* data, size_t size) const noexcept {
    // Simple CRC32-like checksum implementation
    uint32_t checksum = 0xFFFFFFFF;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    for (size_t i = 0; i < size; ++i) {
        checksum ^= bytes[i];
        for (int j = 0; j < 8; ++j) {
            if (checksum & 1) {
                checksum = (checksum >> 1) ^ 0xEDB88320;
            } else {
                checksum >>= 1;
            }
        }
    }

    return ~checksum;
}

template <TriviallyCopyable T>
void SharedMemory<T>::validateDataIntegrity() const {
    if (!config_.enable_version_checking || !header_->initialized.load()) {
        return;
    }

    uint32_t stored_checksum = header_->checksum.load(std::memory_order_acquire);
    uint32_t calculated_checksum = calculateChecksum(getDataPtr(), sizeof(T));

    if (stored_checksum != calculated_checksum) {
        if (config_.enable_stats) {
            stats_.version_conflicts.fetch_add(1, std::memory_order_relaxed);
        }

        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Data integrity validation failed for shared memory: " + name_ +
            " (stored: " + std::to_string(stored_checksum) +
            ", calculated: " + std::to_string(calculated_checksum) + ")",
            SharedMemoryException::ErrorCode::UNKNOWN);
    }
}

template <TriviallyCopyable T>
void SharedMemory<T>::initializeCreatorInfo() {
    if (!isCreator_) return;

    // Get process information
    std::string process_info = "pid:" + std::to_string(getpid());

#ifdef _WIN32
    process_info += ",tid:" + std::to_string(GetCurrentThreadId());
#else
    process_info += ",tid:" + std::to_string(pthread_self());
#endif

    // Copy to header (ensure null termination)
    size_t copy_size = std::min(process_info.size(), sizeof(header_->creator_info) - 1);
    std::memcpy(header_->creator_info, process_info.c_str(), copy_size);
    header_->creator_info[copy_size] = '\0';
}

template <TriviallyCopyable T>
void SharedMemory<T>::handleRecoveryOperation() {
    if (!config_.enable_auto_recovery) return;

    try {
        // Clear the access lock if it's stuck
        header_->accessLock.clear(std::memory_order_release);

        // Log recovery attempt
        spdlog::warn("Attempting auto-recovery for shared memory: {}", name_);

        // Brief delay to allow other processes to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    } catch (...) {
        // Recovery failed, but don't throw - let the original operation handle the timeout
        spdlog::error("Auto-recovery failed for shared memory: {}", name_);
    }
}

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SHARED_MEMORY_HPP
