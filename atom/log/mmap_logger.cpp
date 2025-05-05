// mmap_logger.cpp
/*
 * mmap_logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-5

Description: High-Performance Memory-mapped File Logger Implementation for Atom with C++20
Features:
- Lock-free operations where possible
- High-performance containers
- Optimized synchronization primitives
- Cross-platform system logging support

**************************************************/

#include "mmap_logger.hpp"

#include <atomic>
#include <format>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <utility>

// Platform-specific memory mapping implementation
#ifdef _WIN32
#include <windows.h>
#undef ERROR  // Avoid conflict between Windows and log enum
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// Add header files related to system logging
#ifdef _WIN32
// Header files required for Windows Event Log are included in <windows.h>
#elif defined(__linux__)
#include <syslog.h>
#elif defined(__APPLE__)
#include <os/log.h>
#endif

#include "atom/utils/time.hpp"

// Include high-performance hash map implementation
#include "extra/emhash/hash_table8.hpp"

namespace atom::log {

class MmapLogger::MmapLoggerImpl {
public:
    MmapLoggerImpl(fs::path file_name, LogLevel min_level, size_t buffer_size,
                   int max_files)
        : file_name_(std::move(file_name)),
          min_level_(min_level),
          buffer_size_(buffer_size),
          max_files_(max_files),
          current_pos_(0),
          map_ptr_(nullptr),
          system_logging_enabled_(false) {
        // Initialize memory-mapped file with optimized page alignment
        mapFile();
    }

    ~MmapLoggerImpl() { unmap(); }

    // Disable copying
    MmapLoggerImpl(const MmapLoggerImpl&) = delete;
    auto operator=(const MmapLoggerImpl&) -> MmapLoggerImpl& = delete;

    // Allow moving
    MmapLoggerImpl(MmapLoggerImpl&& other) noexcept
        : file_name_(std::move(other.file_name_)),
          min_level_(other.min_level_),
          buffer_size_(other.buffer_size_),
          max_files_(other.max_files_),
          current_pos_(other.current_pos_.load(std::memory_order_relaxed)),
          system_logging_enabled_(other.system_logging_enabled_.load(std::memory_order_relaxed)),
          thread_names_(std::move(other.thread_names_)) {
        // Acquire and release mapping resources
        std::lock_guard<std::mutex> lock(other.file_spinlock_);
        map_ptr_ = other.map_ptr_;
        other.map_ptr_ = nullptr;

#ifdef _WIN32
        map_handle_ = other.map_handle_;
        file_handle_ = other.file_handle_;
        other.map_handle_ = nullptr;
        other.file_handle_ = nullptr;
#else
        file_descriptor_ = other.file_descriptor_;
        other.file_descriptor_ = -1;
#endif
    }

    void setLevel(LogLevel level) {
        // Use writer lock for changing level
        std::unique_lock<std::shared_mutex> lock(level_mutex_);
        min_level_ = level;
    }

    void setThreadName(const std::string& name) {
        // Use spinlock for short, less contentious operation
        std::lock_guard<std::mutex> lock(thread_spinlock_);
        thread_names_[std::this_thread::get_id()] = name;
    }

    void enableSystemLogging(bool enable) {
        // Use atomic for lock-free access where possible
        system_logging_enabled_.store(enable, std::memory_order_relaxed);
    }

    void flush() {
        std::lock_guard<std::mutex> lock(file_spinlock_);
#ifdef _WIN32
        FlushViewOfFile(map_ptr_, current_pos_.load(std::memory_order_acquire));
#else
        msync(map_ptr_, current_pos_.load(std::memory_order_acquire), MS_SYNC);
#endif
    }

    void log(LogLevel level, std::string_view msg,
             const std::source_location& location) {
        // Check log level - use relaxed memory ordering for better performance
        {
            // Use reader lock for checking level (multiple readers can check level simultaneously)
            std::shared_lock<std::shared_mutex> levelLock(level_mutex_);
            if (static_cast<int>(level) < static_cast<int>(min_level_)) {
                return;
            }
        }

        // Format log message (including source location info)
        auto formattedMsg = formatMessage(level, msg, location);

        // Write to memory-mapped buffer
        writeToBuffer(formattedMsg);

        // Check system logging - use atomic for lock-free access
        if (system_logging_enabled_.load(std::memory_order_relaxed)) {
            logToSystem(level, formattedMsg, location);
        }
    }

private:
    fs::path file_name_;
    LogLevel min_level_;
    size_t buffer_size_;
    int max_files_;
    std::atomic<size_t> current_pos_;
    char* map_ptr_;
    std::atomic<bool> system_logging_enabled_;  // Use atomic for lock-free access

    // Platform-specific file handles
#ifdef _WIN32
    HANDLE file_handle_ = nullptr;
    HANDLE map_handle_ = nullptr;
#else
    int file_descriptor_ = -1;
#endif

    // Optimized synchronization primitives
    std::mutex file_spinlock_;       // Use mutex for short critical sections
    std::shared_mutex level_mutex_;  // Use shared_mutex for read-heavy operations
    std::mutex thread_spinlock_;     // Use mutex for thread name updates

    // High-performance thread name mapping using emhash
    emhash8::HashMap<std::thread::id, std::string> thread_names_;

    // Initialize memory-mapped file with optimized page size
    void mapFile() {
        std::lock_guard lock(file_spinlock_);

#ifdef _WIN32
        // Windows implementation with optimized page size and mapping flags
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        DWORD pageSize = sysInfo.dwPageSize;
        
        // Adjust buffer size to be page-aligned for optimal performance
        buffer_size_ = ((buffer_size_ + pageSize - 1) / pageSize) * pageSize;

        file_handle_ = CreateFileW(file_name_.wstring().c_str(),
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_ALWAYS, 
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, // Optimize for write performance
                                  nullptr);

        if (file_handle_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error(
                std::format("Failed to open log file '{}' for memory mapping. "
                            "Error code: {}",
                            file_name_.string(), GetLastError()));
        }

        // Set file size
        LARGE_INTEGER file_size;
        file_size.QuadPart = buffer_size_;
        if (!SetFilePointerEx(file_handle_, file_size, nullptr, FILE_BEGIN) ||
            !SetEndOfFile(file_handle_)) {
            CloseHandle(file_handle_);
            throw std::runtime_error(
                std::format("Failed to set log file size to {}. Error code: {}",
                            buffer_size_, GetLastError()));
        }

        // Create file mapping with optimized caching flags
        map_handle_ = CreateFileMappingW(
            file_handle_, 
            nullptr, 
            PAGE_READWRITE | SEC_COMMIT, // Commit pages immediately
            0, 0,      // Use the entire file
            nullptr);  // Unnamed mapping

        if (map_handle_ == nullptr) {
            CloseHandle(file_handle_);
            throw std::runtime_error(std::format(
                "Failed to create file mapping for log file. Error code: {}",
                GetLastError()));
        }

        // Map view of file with optimized flags
        map_ptr_ = static_cast<char*>(MapViewOfFile(
            map_handle_, 
            FILE_MAP_ALL_ACCESS, 
            0, 0, 0));  // Map the entire file

        if (map_ptr_ == nullptr) {
            CloseHandle(map_handle_);
            CloseHandle(file_handle_);
            throw std::runtime_error(
                std::format("Failed to map view of log file. Error code: {}",
                            GetLastError()));
        }
#else
        // POSIX implementation with optimized page size
        // Get system page size
        long pageSize = sysconf(_SC_PAGESIZE);
        
        // Adjust buffer size to be page-aligned
        buffer_size_ = ((buffer_size_ + pageSize - 1) / pageSize) * pageSize;
        
        file_descriptor_ = open(file_name_.c_str(), 
                                O_RDWR | O_CREAT | O_DIRECT,  // O_DIRECT for bypass cache when appropriate
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (file_descriptor_ == -1) {
            throw std::runtime_error(std::format(
                "Failed to open log file '{}' for memory mapping. Error: {}",
                file_name_.string(), strerror(errno)));
        }

        // Set file size
        if (ftruncate(file_descriptor_, buffer_size_) == -1) {
            close(file_descriptor_);
            throw std::runtime_error(
                std::format("Failed to set log file size to {}. Error: {}",
                            buffer_size_, strerror(errno)));
        }

        // Memory map with optimized flags
        map_ptr_ = static_cast<char*>(mmap(
            nullptr, 
            buffer_size_,
            PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_POPULATE, // MAP_POPULATE to prefault pages
            file_descriptor_, 
            0));

        if (map_ptr_ == MAP_FAILED) {
            close(file_descriptor_);
            throw std::runtime_error(std::format(
                "Failed to memory map log file. Error: {}", strerror(errno)));
        }
#endif
    }

    // Unmap the file
    void unmap() {
        std::lock_guard lock(file_spinlock_);

        if (map_ptr_ != nullptr) {
            flush();  // Ensure data is written

#ifdef _WIN32
            UnmapViewOfFile(map_ptr_);
            if (map_handle_ != nullptr) {
                CloseHandle(map_handle_);
            }
            if (file_handle_ != nullptr) {
                CloseHandle(file_handle_);
            }
#else
            munmap(map_ptr_, buffer_size_);
            if (file_descriptor_ != -1) {
                close(file_descriptor_);
            }
#endif
            map_ptr_ = nullptr;
        }
    }

    // Get thread name with optimized lookup
    auto getThreadName() -> std::string {
        std::lock_guard<std::mutex> lock(thread_spinlock_);
        auto thread_id = std::this_thread::get_id();
        auto it = thread_names_.find(thread_id);
        if (it != thread_names_.end()) {
            return it->second;
        }

        // Format thread ID if name is not found
        std::ostringstream oss;
        oss << thread_id;
        return oss.str();
    }

    // Convert log level to string - static for better performance
    static auto logLevelToString(LogLevel level) -> std::string_view {
        using enum LogLevel;
        switch (level) {
            case TRACE:
                return "TRACE";
            case DEBUG:
                return "DEBUG";
            case INFO:
                return "INFO";
            case WARN:
                return "WARN";
            case ERROR:
                return "ERROR";
            case CRITICAL:
                return "CRITICAL";
            case OFF:
                return "OFF";
            default:
                return "UNKNOWN";
        }
    }

    // Format log message with optimized string handling
    auto formatMessage(LogLevel level, std::string_view msg,
                       const std::source_location& location) -> std::string {
        auto timestamp = utils::getChinaTimestampString();  // Get timestamp
        auto threadName = getThreadName();
        auto levelStr = logLevelToString(level);

        // Format source location information - preallocate for performance
        std::string result;
        result.reserve(timestamp.size() + threadName.size() + msg.size() + 100);
        
        // Use std::format for modern formatting, efficient for small strings
        return std::format("[{}][{}][{}] {} {}:{}:{}\n", 
                          timestamp, levelStr, threadName, msg,
                          location.file_name(), location.line(), 
                          location.function_name());
    }

    // Write to buffer with optimized memory ordering
    void writeToBuffer(std::string_view formatted_msg) {
        std::lock_guard lock(file_spinlock_);

        size_t msg_len = formatted_msg.length();
        size_t position = current_pos_.load(std::memory_order_relaxed);

        // Check if rotation is needed (buffer full)
        if (position + msg_len >= buffer_size_) {
            rotateLogFile();
            position = 0;
        }

        // Write message - direct memcpy for performance
        memcpy(map_ptr_ + position, formatted_msg.data(), msg_len);

        // Update position with release memory ordering to ensure visibility to other threads
        current_pos_.store(position + msg_len, std::memory_order_release);
    }

    // Rotate log file
    void rotateLogFile() {
        // Note: This function is called under the protection of file_spinlock_
        // lock

        // Unmap the current file
        unmap();

        // Rename the current file as a backup
        try {
            std::error_code ec;

            // Similar logic to Logger, move old files first
            auto extension = file_name_.extension();
            auto stem = file_name_.stem().string();
            auto parent_path = file_name_.parent_path();

            // Remove the oldest log file
            auto oldest_log =
                parent_path /
                (stem + "." + std::to_string(max_files_) + extension.string());
            if (fs::exists(oldest_log, ec)) {
                fs::remove(oldest_log, ec);
            }

            // Rotate other log files - process in reverse order to avoid overwriting
            for (int i = max_files_ - 1; i > 0; --i) {
                auto src = parent_path / (stem + "." + std::to_string(i) +
                                          extension.string());
                auto dst = parent_path / (stem + "." + std::to_string(i + 1) +
                                          extension.string());

                if (fs::exists(src, ec)) {
                    fs::rename(src, dst, ec);
                }
            }

            // Rename the current log file
            auto backup = parent_path / (stem + ".1" + extension.string());
            fs::rename(file_name_, backup, ec);

        } catch (const std::exception& e) {
            // Rotation failed, log to stderr (cannot log to the file)
            fprintf(stderr, "Failed to rotate memory-mapped log file: %s\n",
                    e.what());
        }

        // Recreate and map the new file
        mapFile();

        // Reset write position
        current_pos_.store(0, std::memory_order_relaxed);
    }

    // System logging with optimized platform detection
    void logToSystem(LogLevel level, std::string_view msg,
                     const std::source_location& location) {
        // Prepare source location information with preallocation
        std::string source_info;
        source_info.reserve(100);
        source_info = std::format("{}:{}:{}", location.file_name(), location.line(),
                                 location.function_name());

#ifdef _WIN32
        // Windows Event Log implementation
        static HANDLE event_source = nullptr;

        // Lazy initialize event source handle
        if (event_source == nullptr) {
            event_source = RegisterEventSourceW(nullptr, L"AtomLogger");
            if (event_source == nullptr) {
                return;  // Cannot initialize event log
            }
        }

        // Convert message to wide characters with preallocation
        std::wstring wide_msg;
        try {
            // Add log level and location info to the message
            std::string full_msg = std::format(
                "[{}] {} - {}", logLevelToString(level), msg, source_info);

            // Determine buffer size
            int size_needed = MultiByteToWideChar(
                CP_UTF8, 0, full_msg.data(),
                static_cast<int>(full_msg.length()), nullptr, 0);
            wide_msg.resize(size_needed);

            // Convert to wide characters
            MultiByteToWideChar(CP_UTF8, 0, full_msg.data(),
                                static_cast<int>(full_msg.length()),
                                wide_msg.data(), size_needed);
        } catch (...) {
            return;  // Conversion failed, skip system logging
        }

        // Determine event type based on log level
        WORD event_type;
        switch (level) {
            case LogLevel::CRITICAL:
            case LogLevel::ERROR:
                event_type = EVENTLOG_ERROR_TYPE;
                break;
            case LogLevel::WARN:
                event_type = EVENTLOG_WARNING_TYPE;
                break;
            case LogLevel::INFO:
            case LogLevel::DEBUG:
            case LogLevel::TRACE:
            default:
                event_type = EVENTLOG_INFORMATION_TYPE;
                break;
        }

        // Send to Windows Event Log
        const WCHAR* string_ptr = wide_msg.c_str();
        ReportEventW(event_source, event_type, 0, 0, nullptr, 1, 0, &string_ptr,
                     nullptr);

#elif defined(__linux__)
        // Linux syslog implementation with thread safety
        static bool syslog_opened = false;
        static std::once_flag syslog_init_flag;

        // Use call_once for thread-safe initialization
        std::call_once(syslog_init_flag, []() {
            openlog("AtomLogger", LOG_PID | LOG_NDELAY, LOG_USER);
            syslog_opened = true;
        });

        // Map log level to syslog priority
        int syslog_priority;
        switch (level) {
            case LogLevel::CRITICAL:
                syslog_priority = LOG_CRIT;
                break;
            case LogLevel::ERROR:
                syslog_priority = LOG_ERR;
                break;
            case LogLevel::WARN:
                syslog_priority = LOG_WARNING;
                break;
            case LogLevel::INFO:
                syslog_priority = LOG_INFO;
                break;
            case LogLevel::DEBUG:
            case LogLevel::TRACE:
            default:
                syslog_priority = LOG_DEBUG;
                break;
        }

        // Send to syslog - use string_view for better performance
        std::string full_msg = std::format("{} - {}", msg, source_info);
        syslog(syslog_priority, "%.*s", static_cast<int>(full_msg.length()), full_msg.data());

#elif defined(__APPLE__)
        // macOS os_log implementation with thread safety
        static os_log_t log_handle = nullptr;
        static std::once_flag os_log_init_flag;

        // Use call_once for thread-safe initialization
        std::call_once(os_log_init_flag, []() {
            log_handle = os_log_create("com.lightapt.atom", "logger");
        });

        // Prepare full message
        std::string full_msg = std::format("{} - {}", msg, source_info);

        // Map log level to os_log type
        os_log_type_t log_type;
        switch (level) {
            case LogLevel::CRITICAL:
                log_type = OS_LOG_TYPE_FAULT;
                break;
            case LogLevel::ERROR:
                log_type = OS_LOG_TYPE_ERROR;
                break;
            case LogLevel::WARN:
                log_type = OS_LOG_TYPE_DEFAULT;
                break;
            case LogLevel::INFO:
                log_type = OS_LOG_TYPE_INFO;
                break;
            case LogLevel::DEBUG:
            case LogLevel::TRACE:
            default:
                log_type = OS_LOG_TYPE_DEBUG;
                break;
        }

        // Send to os_log
        os_log_with_type(log_handle, log_type, "%{public}s", full_msg.c_str());
#endif
    }
};

// MmapLogger class method implementations

MmapLogger::MmapLogger(const fs::path& file_name, LogLevel min_level,
                       size_t buffer_size, int max_files)
    : impl_(std::make_unique<MmapLoggerImpl>(file_name, min_level, buffer_size,
                                             max_files)) {}

MmapLogger::~MmapLogger() = default;

// Move constructor and move assignment
MmapLogger::MmapLogger(MmapLogger&&) noexcept = default;
auto MmapLogger::operator=(MmapLogger&&) noexcept -> MmapLogger& = default;

void MmapLogger::setLevel(LogLevel level) { impl_->setLevel(level); }

void MmapLogger::setThreadName(const String& name) {
    impl_->setThreadName(name);
}

void MmapLogger::enableSystemLogging(bool enable) {
    impl_->enableSystemLogging(enable);
}

void MmapLogger::flush() { impl_->flush(); }

void MmapLogger::log(LogLevel level, std::string_view msg,
                     const std::source_location& location) {
    impl_->log(level, msg, location);
}

}  // namespace atom::log