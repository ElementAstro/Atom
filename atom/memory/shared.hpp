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

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
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
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
    ErrorCode code_{ErrorCode::UNKNOWN};  ///< The specific error code.
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

// Header structure stored at the beginning of shared memory
struct SharedMemoryHeader {
    std::atomic_flag accessLock;    ///< Access lock
    std::atomic<std::size_t> size;  ///< Data size
    std::atomic<uint64_t> version;  ///< Data version number
    std::atomic<bool> initialized;  ///< Initialization flag
    // Additional metadata can be added here...
};

/**
 * @brief Enhanced cross-platform shared memory implementation.
 *
 * @tparam T The type of data stored in shared memory, must be trivially
 * copyable.
 */
template <TriviallyCopyable T>
class SharedMemory : public NonCopyable {
public:
    /// Type definition for change callback functions.
    using ChangeCallback = std::function<void(const T&)>;

    /**
     * @brief Constructs a new SharedMemory object.
     *
     * @param name The name of the shared memory.
     * @param create Whether to create new shared memory.
     * @param initialData Optional initial data to write to shared memory.
     */
    explicit SharedMemory(std::string_view name, bool create = true,
                          const std::optional<T>& initialData = std::nullopt);

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
        std::size_t offset, std::chrono::milliseconds timeout =
                                std::chrono::milliseconds(0)) const -> U;

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
    auto writeAsync(const T& data,
                    std::chrono::milliseconds timeout =
                        std::chrono::milliseconds(0)) -> std::future<void>;

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

private:
    std::string name_;       ///< The name of the shared memory.
    std::size_t totalSize_;  ///< The total size of the shared memory.

#ifdef _WIN32
    HANDLE handle_;       ///< Windows handle for the shared memory.
    HANDLE changeEvent_;  ///< Windows event object for change notifications.
#else
    int fd_{-1};  ///< File descriptor for the shared memory.
    sem_t* semId_{
        SEM_FAILED};  ///< POSIX semaphore for cross-process synchronization.
#endif

    void* buffer_;                ///< Pointer to the shared memory buffer.
    SharedMemoryHeader* header_;  ///< Pointer to the shared memory header.
    mutable std::mutex mutex_;    ///< Mutex for synchronizing access.
    mutable std::condition_variable
        changeCondition_;  ///< Condition variable for change notifications.
    bool is_creator_;      ///< Flag indicating if the current process is the
                           ///< creator.
    mutable uint64_t lastKnownVersion_{0};  ///< Last known version of the data.

    std::vector<std::pair<std::size_t, ChangeCallback>>
        changeCallbacks_;            ///< List of registered change callbacks.
    std::size_t nextCallbackId_{1};  ///< Next callback ID to assign.
    mutable std::mutex
        callbackMutex_;  ///< Mutex for synchronizing callback access.

    std::thread watchThread_;  ///< Thread for watching for changes.
    std::atomic<bool> stopWatching_{false};  ///< Flag to stop the watch thread.

    /**
     * @brief Unmaps the shared memory.
     */
    void unmap();

    /**
     * @brief Maps the shared memory.
     *
     * @param create Whether to create new shared memory.
     * @param size The size of the shared memory.
     */
    void mapMemory(bool create, std::size_t size);

    /**
     * @brief Notifies listeners of data changes.
     *
     * @param data The new data.
     */
    void notifyListeners(const T& data);

    /**
     * @brief Starts the watch thread.
     */
    void startWatchThread();

    /**
     * @brief Watches for changes in the shared memory.
     */
    void watchForChanges();

    /**
     * @brief Platform-specific initialization.
     */
    void platformSpecificInit();

    /**
     * @brief Platform-specific cleanup.
     */
    void platformSpecificCleanup();

    /**
     * @brief Gets the detailed message for the last error.
     *
     * @return The error message.
     */
    static std::string getLastErrorMessage();
};

template <TriviallyCopyable T>
SharedMemory<T>::SharedMemory(std::string_view name, bool create,
                              const std::optional<T>& initialData)
    : name_(name), buffer_(nullptr), header_(nullptr), is_creator_(create) {
    totalSize_ = sizeof(SharedMemoryHeader) + sizeof(T);

    try {
        mapMemory(create, totalSize_);
        platformSpecificInit();

        if (create && initialData) {
            withLock(
                [&]() {
                    std::memcpy(getDataPtr(), &(*initialData), sizeof(T));
                    header_->initialized.store(true, std::memory_order_release);
                    header_->version.fetch_add(1, std::memory_order_release);
                    DLOG_F(INFO,
                           "Initialized shared memory '{}' with initial data",
                           name_.c_str());
                },
                std::chrono::milliseconds(100));
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
    // 停止监视线程
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
    changeEvent_ = CreateEventA(NULL, TRUE, FALSE, eventName.c_str());
    if (!changeEvent_) {
        DLOG_F(WARNING, "Failed to create change event for shared memory: {}",
               getLastErrorMessage().c_str());
    }
#else
    std::string semName = "/" + name_ + "_sem";
    semId_ = sem_open(semName.c_str(), O_CREAT, 0666, 0);
    if (semId_ == SEM_FAILED) {
        DLOG_F(WARNING, "Failed to create semaphore for shared memory: {}",
               strerror(errno));
        // Keep semId_ as SEM_FAILED rather than assigning an integer to it
    }
#endif
}

template <TriviallyCopyable T>
void SharedMemory<T>::platformSpecificCleanup() {
#ifdef _WIN32
    if (changeEvent_) {
        CloseHandle(changeEvent_);
        changeEvent_ = NULL;
    }
#else
    if (semId_ != SEM_FAILED) {
        std::string semName = "/" + name_ + "_sem";
        sem_close(semId_);
        if (is_creator_) {
            sem_unlink(semName.c_str());
        }
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
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer,
        0, NULL);

    std::string message(buffer, size);
    LocalFree(buffer);
    return message;
#else
    return strerror(errno);
#endif
}

template <TriviallyCopyable T>
void SharedMemory<T>::unmap() {
#ifdef _WIN32
    if (buffer_) {
        UnmapViewOfFile(buffer_);
        buffer_ = nullptr;
    }
    if (handle_) {
        CloseHandle(handle_);
        handle_ = NULL;
    }
#else
    if (buffer_ != nullptr) {
        munmap(buffer_, totalSize_);
        buffer_ = nullptr;
    }
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
        if (is_creator_) {
            shm_unlink(name_.c_str());
        }
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
        handle_ = NULL;
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

    // 设置头部指针
    header_ = static_cast<SharedMemoryHeader*>(buffer_);

    // 如果是创建者，初始化头部
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

    if (!is_creator_) {
        THROW_SHARED_MEMORY_ERROR_WITH_CODE(
            "Only the creator can resize shared memory",
            SharedMemoryException::ErrorCode::ACCESS_DENIED);
    }

    // 获取当前数据的副本
    T currentData;
    bool wasInitialized = false;

    if (isInitialized()) {
        currentData = read(std::chrono::milliseconds(100));
        wasInitialized = true;
    }

    // 解除映射，重新创建
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
        handle_ = NULL;
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

    // 重新初始化头部
    header_ = static_cast<SharedMemoryHeader*>(buffer_);
    new (header_) SharedMemoryHeader();
    header_->size.store(newSize, std::memory_order_release);
    header_->version.store(0, std::memory_order_release);

    // 如果之前有数据，恢复数据
    if (wasInitialized) {
        std::size_t copySize = std::min(newSize, sizeof(T));
        std::memcpy(getDataPtr(), &currentData, copySize);
        header_->initialized.store(true, std::memory_order_release);
        header_->version.fetch_add(1, std::memory_order_release);
    } else {
        header_->initialized.store(false, std::memory_order_release);
    }

    totalSize_ = totalNewSize;
    DLOG_F(INFO, "Shared memory '{}' resized to %zu bytes", name_.c_str(),
           newSize);
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
auto SharedMemory<T>::withLock(Func&& func, std::chrono::milliseconds timeout)
    const -> decltype(std::forward<Func>(func)()) {
    std::unique_lock lock(mutex_);
    auto startTime = std::chrono::steady_clock::now();

    while (header_->accessLock.test_and_set(std::memory_order_acquire)) {
        if (timeout != std::chrono::milliseconds(0) &&
            std::chrono::steady_clock::now() - startTime >= timeout) {
            THROW_SHARED_MEMORY_ERROR_WITH_CODE(
                "Failed to acquire mutex within timeout for shared memory: " +
                    name_,
                SharedMemoryException::ErrorCode::TIMEOUT);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    try {
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
            // 复制数据到共享内存
            std::memcpy(getDataPtr(), &data, sizeof(T));

            // 更新元数据
            header_->initialized.store(true, std::memory_order_release);
            header_->version.fetch_add(1, std::memory_order_release);
            DLOG_F(INFO, "Data written to shared memory: {} (version %lu)",
                   name_.c_str(), header_->version.load());

        // 通知其他进程
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

    // 通知本地监听器
    if (notifyListeners) {
        notifyListeners(data);

        // 通知任何等待变更的线程
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

            DLOG_F(INFO, "Data read from shared memory: {} (version %lu)",
                   name_.c_str(), lastKnownVersion_);

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

            DLOG_F(INFO, "Shared memory cleared: {}", name_.c_str());

        // 通知其他进程
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

    // 通知本地监听器和等待线程
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
    return is_creator_;
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

            // 确保设置初始化标志并增加版本号
            header_->initialized.store(true, std::memory_order_release);
            header_->version.fetch_add(1, std::memory_order_release);

            DLOG_F(INFO,
                   "Partial data written to shared memory: {} (offset: %zu, "
                   "size: %zu)",
                   name_.c_str(), offset, sizeof(U));

        // 通知其他进程
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

    // 只在部分写入后通知，如果需要监听整个对象的变更
    changeCondition_.notify_all();
}

template <TriviallyCopyable T>
template <typename U>
auto SharedMemory<T>::readPartial(
    std::size_t offset, std::chrono::milliseconds timeout) const -> U {
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

            DLOG_F(INFO,
                   "Partial data read from shared memory: {} (offset: %zu, "
                   "size: %zu)",
                   name_.c_str(), offset, sizeof(U));

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
        LOG_F(WARNING, "Try read failed: {} ({})", e.what(),
              e.getErrorCodeString().c_str());
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

            // 确保设置初始化标志并增加版本号
            header_->initialized.store(true, std::memory_order_release);
            header_->version.fetch_add(1, std::memory_order_release);

            DLOG_F(INFO, "Span data written to shared memory: {} (size: %zu)",
                   name_.c_str(), data.size_bytes());

        // 通知其他进程
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

    // 通知本地监听器和等待线程
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

            DLOG_F(INFO, "Span data read from shared memory: {} (size: %zu)",
                   name_.c_str(), bytesToRead);

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
auto SharedMemory<T>::writeAsync(
    const T& data, std::chrono::milliseconds timeout) -> std::future<void> {
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
void SharedMemory<T>::notifyListeners(const T& data) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (const auto& [id, callback] : changeCallbacks_) {
        try {
            callback(data);
        } catch (const std::exception& e) {
            LOG_F(ERROR,
                  "Exception in change callback for shared memory {}: {}",
                  name_.c_str(), e.what());
        }
    }
}

template <TriviallyCopyable T>
auto SharedMemory<T>::waitForChange(std::chrono::milliseconds timeout) -> bool {
    std::unique_lock<std::mutex> lock(mutex_);
    uint64_t currentVersion = header_->version.load(std::memory_order_acquire);

    // 如果版本已经变化，立即返回
    if (currentVersion != lastKnownVersion_) {
        lastKnownVersion_ = currentVersion;
        return true;
    }

    if (timeout == std::chrono::milliseconds(0)) {
        // 永久等待
        changeCondition_.wait(lock, [this, currentVersion]() {
            return header_->version.load(std::memory_order_acquire) !=
                   currentVersion;
        });
        lastKnownVersion_ = header_->version.load(std::memory_order_acquire);
        return true;
    } else {
        // 限时等待
        bool changed =
            changeCondition_.wait_for(lock, timeout, [this, currentVersion]() {
                return header_->version.load(std::memory_order_acquire) !=
                       currentVersion;
            });

        if (changed) {
            lastKnownVersion_ =
                header_->version.load(std::memory_order_acquire);
        }
        return changed;
    }
}

template <TriviallyCopyable T>
void SharedMemory<T>::startWatchThread() {
    watchThread_ = std::thread([this]() { this->watchForChanges(); });
}

template <TriviallyCopyable T>
void SharedMemory<T>::watchForChanges() {
    while (!stopWatching_) {
#ifdef _WIN32
        if (changeEvent_) {
            // 等待事件，最多100毫秒
            if (WaitForSingleObject(changeEvent_, 100) == WAIT_OBJECT_0) {
                uint64_t currentVersion =
                    header_->version.load(std::memory_order_acquire);
                if (currentVersion != lastKnownVersion_) {
                    try {
                        T data = read(std::chrono::milliseconds(50));
                        notifyListeners(data);
                        changeCondition_.notify_all();
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception while reading changed data: {}",
                              e.what());
                    }
                }
                ResetEvent(changeEvent_);
            }
        } else {
            // 如果没有事件，每100ms轮询一次
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
                    LOG_F(ERROR, "Exception while reading changed data: {}",
                          e.what());
                }
            }
        }
#else
        if (semId_ != SEM_FAILED) {
            // 使用超时的sem_wait
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 0;
            ts.tv_nsec += 100 * 1000000;  // 100ms
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
                        LOG_F(ERROR, "Exception while reading changed data: {}",
                              e.what());
                    }
                }
            } else if (errno != ETIMEDOUT) {
                LOG_F(WARNING, "sem_timedwait failed: {}", strerror(errno));
            }
        } else {
            // 如果没有信号量，每100ms轮询一次
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
                    LOG_F(ERROR, "Exception while reading changed data: {}",
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

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SHARED_MEMORY_HPP