/*
 * mmap_logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-6

Description: High-Performance Memory-mapped File Logger Implementation for Atom
with C++20/23 Features:
- Lock-free operations where possible
- High-performance containers
- Optimized synchronization primitives
- Cross-platform system logging support
- Enhanced error handling with std::expected
- Modern pattern matching with C++23 features
- Highly optimized string handling
- Category-based message filtering
- Statistics and metrics collection
- Automatic compression for rotated logs

**************************************************/

#include "mmap_logger.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <span>
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

// Optional compression support using zlib if available
#if __has_include(<zlib.h>)
#include <zlib.h>
#define HAS_COMPRESSION_SUPPORT 1
#else
#define HAS_COMPRESSION_SUPPORT 0
#endif

namespace atom::log {

// Statistics tracking structure
struct LogStats {
    std::atomic<uint64_t> log_count{0};
    std::atomic<uint64_t> bytes_written{0};
    std::atomic<uint64_t> flush_count{0};
    std::atomic<uint64_t> rotation_count{0};
    std::atomic<uint64_t> error_count{0};
    std::atomic<uint64_t> filtered_out_count{0};
    std::atomic<uint64_t> message_sizes[6]{0};  // One per log level

    // RAII-style timing class for performance metrics
    class ScopedTimer {
    public:
        explicit ScopedTimer(std::chrono::nanoseconds& result)
            : start_(std::chrono::high_resolution_clock::now()),
              result_(result) {}
        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            result_ = end - start_;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> start_;
        std::chrono::nanoseconds& result_;
    };

    // Performance metrics
    std::atomic<std::chrono::nanoseconds> format_time{
        std::chrono::nanoseconds(0)};
    std::atomic<std::chrono::nanoseconds> write_time{
        std::chrono::nanoseconds(0)};
    std::atomic<std::chrono::nanoseconds> flush_time{
        std::chrono::nanoseconds(0)};
    std::atomic<std::chrono::nanoseconds> system_log_time{
        std::chrono::nanoseconds(0)};

    // For tracking peak operations
    std::atomic<uint32_t> peak_logs_per_second{0};
    std::atomic<uint32_t> current_logs_this_second{0};
    std::chrono::time_point<std::chrono::system_clock> last_reset_time{
        std::chrono::system_clock::now()};

    // Update logs per second counter
    void updateLogsPerSecond() {
        auto now = std::chrono::system_clock::now();
        auto current_count = ++current_logs_this_second;

        // Check if we've passed a second boundary
        if (now - last_reset_time >= std::chrono::seconds(1)) {
            // Atomic update of peak if current is higher
            uint32_t expected = peak_logs_per_second.load();
            while (current_count > expected &&
                   !peak_logs_per_second.compare_exchange_weak(expected,
                                                               current_count)) {
                // Keep trying if another thread updated the value
            }

            // Reset counter and timestamp atomically
            current_logs_this_second.store(0);
            last_reset_time = now;
        }
    }
};

class MmapLogger::MmapLoggerImpl {
public:
    MmapLoggerImpl(const Config& config)
        : file_name_(config.file_name),
          min_level_(config.min_level),
          buffer_size_(config.buffer_size),
          max_files_(config.max_files),
          current_pos_(0),
          map_ptr_(nullptr),
          system_logging_enabled_(config.use_system_logging),
          auto_flush_(config.auto_flush),
          thread_name_prefix_(config.thread_name_prefix),
          compression_enabled_(false),
          auto_flush_interval_(0),
          stats_(std::make_unique<LogStats>()) {
        // Initialize memory-mapped file with optimized page alignment
        mapFile();

        // Start auto-flush thread if enabled
        if (auto_flush_) {
            startAutoFlushThread();
        }
    }

    MmapLoggerImpl(fs::path file_name, LogLevel min_level, size_t buffer_size,
                   int max_files)
        : file_name_(std::move(file_name)),
          min_level_(min_level),
          buffer_size_(buffer_size),
          max_files_(max_files),
          current_pos_(0),
          map_ptr_(nullptr),
          system_logging_enabled_(false),
          auto_flush_(false),
          thread_name_prefix_("Thread-"),
          compression_enabled_(false),
          auto_flush_interval_(0),
          stats_(std::make_unique<LogStats>()) {
        // Initialize memory-mapped file with optimized page alignment
        mapFile();
    }

    ~MmapLoggerImpl() {
        // Stop auto-flush thread if running
        stopAutoFlushThread();
        // Unmap and close file
        unmap();
    }

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
          system_logging_enabled_(
              other.system_logging_enabled_.load(std::memory_order_relaxed)),
          auto_flush_(other.auto_flush_.load(std::memory_order_relaxed)),
          thread_name_prefix_(std::move(other.thread_name_prefix_)),
          compression_enabled_(
              other.compression_enabled_.load(std::memory_order_relaxed)),
          auto_flush_interval_(
              other.auto_flush_interval_.load(std::memory_order_relaxed)),
          thread_names_(std::move(other.thread_names_)),
          category_filters_(std::move(other.category_filters_)),
          pattern_filters_(std::move(other.pattern_filters_)),
          stats_(std::move(other.stats_)) {
        // Acquire and release mapping resources
        std::lock_guard<std::mutex> lock(other.file_mutex_);
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

        // Handle auto-flush thread
        if (auto_flush_) {
            startAutoFlushThread();
        }
    }

    void setLevel(LogLevel level) {
        // Use writer lock for changing level
        std::unique_lock<std::shared_mutex> lock(level_mutex_);
        min_level_ = level;
    }

    void setThreadName(const std::string& name) {
        // Use mutex for thread name updates
        std::lock_guard<std::mutex> lock(thread_mutex_);
        thread_names_[std::this_thread::get_id()] = name;
    }

    void enableSystemLogging(bool enable) {
        // Use atomic for lock-free access where possible
        system_logging_enabled_.store(enable, std::memory_order_relaxed);
    }

    void setAutoFlushInterval(uint32_t milliseconds) {
        auto_flush_interval_.store(milliseconds, std::memory_order_relaxed);

        if (milliseconds > 0 && !auto_flush_.load(std::memory_order_relaxed)) {
            // Enable auto-flush with the new interval
            auto_flush_.store(true, std::memory_order_relaxed);
            startAutoFlushThread();
        } else if (milliseconds == 0 &&
                   auto_flush_.load(std::memory_order_relaxed)) {
            // Disable auto-flush
            auto_flush_.store(false, std::memory_order_relaxed);
            stopAutoFlushThread();
        }
    }

    void enableCompression(bool enable) {
#if HAS_COMPRESSION_SUPPORT
        compression_enabled_.store(enable, std::memory_order_relaxed);
#else
        // Silently ignore if no compression support
        (void)enable;
#endif
    }

    void setCategoryFilter(std::span<const MmapLogger::Category> categories) {
        std::lock_guard<std::mutex> lock(filter_mutex_);

        // Clear existing filters and add new ones
        category_filters_.clear();
        for (auto& category : categories) {
            category_filters_.insert(category);
        }
    }

    void addFilterPattern(std::string_view pattern) {
        std::lock_guard<std::mutex> lock(filter_mutex_);
        try {
            pattern_filters_.push_back(
                std::regex(pattern.data(), std::regex::optimize));
        } catch (const std::regex_error& e) {
            // Handle regex compilation error
            stats_->error_count++;
            throw ConfigException(
                std::format("Invalid regex pattern: {}", e.what()));
        }
    }

    [[nodiscard]] std::string getStatistics() const {
        // Return JSON-formatted statistics
        return std::format(
            R"({{"log_count":{}, "bytes_written":{}, "flush_count":{}, "rotation_count":{}, "error_count":{}, "filtered_out_count":{}, "peak_logs_per_second":{}, "avg_format_time_ns":{}, "avg_write_time_ns":{}, "avg_flush_time_ns":{}, "avg_system_log_time_ns":{}}})",
            stats_->log_count.load(), stats_->bytes_written.load(),
            stats_->flush_count.load(), stats_->rotation_count.load(),
            stats_->error_count.load(), stats_->filtered_out_count.load(),
            stats_->peak_logs_per_second.load(),
            stats_->log_count.load() > 0
                ? stats_->format_time.load().count() / stats_->log_count.load()
                : 0,
            stats_->log_count.load() > 0
                ? stats_->write_time.load().count() / stats_->log_count.load()
                : 0,
            stats_->flush_count.load() > 0
                ? stats_->flush_time.load().count() / stats_->flush_count.load()
                : 0,
            stats_->log_count.load() > 0
                ? stats_->system_log_time.load().count() /
                      stats_->log_count.load()
                : 0);
    }

    std::expected<void, LoggerErrorCode> flush() noexcept {
        std::lock_guard<std::mutex> lock(file_mutex_);
        LogStats::ScopedTimer timer(flush_time_);

        if (map_ptr_ == nullptr) {
            stats_->error_count++;
            return std::unexpected(LoggerErrorCode::MappingError);
        }

        try {
#ifdef _WIN32
            if (!FlushViewOfFile(
                    map_ptr_, current_pos_.load(std::memory_order_acquire))) {
                stats_->error_count++;
                return std::unexpected(LoggerErrorCode::UnmapError);
            }
#else
            if (msync(map_ptr_, current_pos_.load(std::memory_order_acquire),
                      MS_SYNC) != 0) {
                stats_->error_count++;
                return std::unexpected(LoggerErrorCode::UnmapError);
            }
#endif
            stats_->flush_count++;
            return {};
        } catch (...) {
            stats_->error_count++;
            return std::unexpected(LoggerErrorCode::UnmapError);
        }
    }

    void log(LogLevel level, MmapLogger::Category category,
             std::string_view msg, const std::source_location& location) {
        // Check log level - use relaxed memory ordering for better performance
        {
            // Use reader lock for checking level (multiple readers can check
            // level simultaneously)
            std::shared_lock<std::shared_mutex> levelLock(level_mutex_);
            if (static_cast<int>(level) < static_cast<int>(min_level_)) {
                return;
            }
        }

        // Check category filters
        {
            std::shared_lock<std::mutex> lock(filter_mutex_);
            if (!category_filters_.empty() &&
                !category_filters_.contains(category)) {
                stats_->filtered_out_count++;
                return;
            }
        }

        // Update logs-per-second counter
        stats_->updateLogsPerSecond();

        // Format log message (including source location info)
        std::chrono::nanoseconds format_time;
        std::string formattedMsg;
        {
            LogStats::ScopedTimer timer(format_time);
            formattedMsg = formatMessage(level, category, msg, location);
        }
        stats_->format_time.store(stats_->format_time.load() + format_time,
                                  std::memory_order_relaxed);

        // Check message against pattern filters
        {
            std::shared_lock<std::mutex> lock(filter_mutex_);
            for (const auto& pattern : pattern_filters_) {
                if (std::regex_search(formattedMsg, pattern)) {
                    stats_->filtered_out_count++;
                    return;
                }
            }
        }

        // Write to memory-mapped buffer
        std::chrono::nanoseconds write_time;
        {
            LogStats::ScopedTimer timer(write_time);
            writeToBuffer(formattedMsg);
        }
        stats_->write_time.store(stats_->write_time.load() + write_time,
                                 std::memory_order_relaxed);

        // Track stats for message size by level
        stats_
            ->message_sizes[static_cast<int>(level) < 6
                                ? static_cast<int>(level)
                                : 5]
            .fetch_add(formattedMsg.size());
        stats_->log_count++;
        stats_->bytes_written.fetch_add(formattedMsg.size());

        // Check system logging - use atomic for lock-free access
        if (system_logging_enabled_.load(std::memory_order_relaxed)) {
            std::chrono::nanoseconds system_log_time;
            {
                LogStats::ScopedTimer timer(system_log_time);
                logToSystem(level, formattedMsg, location);
            }
            stats_->system_log_time.store(
                stats_->system_log_time.load() + system_log_time,
                std::memory_order_relaxed);
        }
    }

private:
    fs::path file_name_;
    LogLevel min_level_;
    size_t buffer_size_;
    int max_files_;
    std::atomic<size_t> current_pos_;
    char* map_ptr_;
    std::atomic<bool>
        system_logging_enabled_;      // Use atomic for lock-free access
    std::atomic<bool> auto_flush_;    // Use atomic for auto-flush control
    std::string thread_name_prefix_;  // Prefix for auto-generated thread names
    std::atomic<bool> compression_enabled_;  // For compressed log rotation
    std::atomic<uint32_t>
        auto_flush_interval_;  // Milliseconds between auto-flush

    // Storage for timing metrics
    std::chrono::nanoseconds flush_time_;

    // Platform-specific file handles
#ifdef _WIN32
    HANDLE file_handle_ = nullptr;
    HANDLE map_handle_ = nullptr;
#else
    int file_descriptor_ = -1;
#endif

    // Optimized synchronization primitives
    std::mutex file_mutex_;          // For file operations
    std::shared_mutex level_mutex_;  // For log level (read-heavy)
    std::mutex thread_mutex_;        // For thread name updates
    std::mutex filter_mutex_;        // For filter modifications

    // Auto-flush thread control
    std::thread auto_flush_thread_;
    std::atomic<bool> stop_auto_flush_{false};

    // High-performance thread name mapping using emhash
    emhash8::HashMap<std::thread::id, std::string> thread_names_;

    // Filters for log categories and message patterns
    std::unordered_set<MmapLogger::Category> category_filters_;
    std::vector<std::regex> pattern_filters_;

    // Statistics tracking
    std::unique_ptr<LogStats> stats_;

    // Initialize memory-mapped file with optimized page size
    void mapFile() {
        std::lock_guard<std::mutex> lock(file_mutex_);

#ifdef _WIN32
        // Windows implementation with optimized page size and mapping flags
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        DWORD pageSize = sysInfo.dwPageSize;

        // Adjust buffer size to be page-aligned for optimal performance
        buffer_size_ = ((buffer_size_ + pageSize - 1) / pageSize) * pageSize;

        // Use modern C++17 filesystem error handling
        std::error_code ec;
        if (!fs::exists(file_name_.parent_path(), ec)) {
            fs::create_directories(file_name_.parent_path(), ec);
            if (ec) {
                stats_->error_count++;
                throw FileException(std::format(
                    "Failed to create log directory '{}'. Error code: {}",
                    file_name_.parent_path().string(), ec.value()));
            }
        }

        file_handle_ = CreateFileW(
            file_name_.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL |
                FILE_FLAG_WRITE_THROUGH,  // Optimize for write performance
            nullptr);

        if (file_handle_ == INVALID_HANDLE_VALUE) {
            stats_->error_count++;
            throw FileException(
                std::format("Failed to open log file '{}' for memory mapping. "
                            "Error code: {}",
                            file_name_.string(), GetLastError()));
        }

        // Set file size
        LARGE_INTEGER file_size;
        file_size.QuadPart = static_cast<LONGLONG>(buffer_size_);
        if (!SetFilePointerEx(file_handle_, file_size, nullptr, FILE_BEGIN) ||
            !SetEndOfFile(file_handle_)) {
            CloseHandle(file_handle_);
            stats_->error_count++;
            throw FileException(
                std::format("Failed to set log file size to {}. Error code: {}",
                            buffer_size_, GetLastError()));
        }

        // Create file mapping with optimized caching flags
        map_handle_ = CreateFileMappingW(
            file_handle_, nullptr,
            PAGE_READWRITE | SEC_COMMIT,  // Commit pages immediately
            0, 0,                         // Use the entire file
            nullptr);                     // Unnamed mapping

        if (map_handle_ == nullptr) {
            CloseHandle(file_handle_);
            stats_->error_count++;
            throw MappingException(std::format(
                "Failed to create file mapping for log file. Error code: {}",
                GetLastError()));
        }

        // Map view of file with optimized flags
        map_ptr_ = static_cast<char*>(MapViewOfFile(
            map_handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0));  // Map the entire file

        if (map_ptr_ == nullptr) {
            CloseHandle(map_handle_);
            CloseHandle(file_handle_);
            stats_->error_count++;
            throw MappingException(
                std::format("Failed to map view of log file. Error code: {}",
                            GetLastError()));
        }
#else
        // POSIX implementation with optimized page size
        // Get system page size
        long pageSize = sysconf(_SC_PAGESIZE);

        // Adjust buffer size to be page-aligned
        buffer_size_ = ((buffer_size_ + pageSize - 1) / pageSize) * pageSize;

        // Create directory if it doesn't exist
        std::error_code ec;
        if (!fs::exists(file_name_.parent_path(), ec)) {
            fs::create_directories(file_name_.parent_path(), ec);
            if (ec) {
                stats_->error_count++;
                throw FileException(std::format(
                    "Failed to create log directory '{}'. Error code: {}",
                    file_name_.parent_path().string(), ec.value()));
            }
        }

        file_descriptor_ =
            open(file_name_.c_str(),
                 O_RDWR | O_CREAT,  // Avoid O_DIRECT as it may not be portable
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (file_descriptor_ == -1) {
            stats_->error_count++;
            throw FileException(std::format(
                "Failed to open log file '{}' for memory mapping. Error: {}",
                file_name_.string(), strerror(errno)));
        }

        // Set file size
        if (ftruncate(file_descriptor_, static_cast<off_t>(buffer_size_)) ==
            -1) {
            close(file_descriptor_);
            stats_->error_count++;
            throw FileException(
                std::format("Failed to set log file size to {}. Error: {}",
                            buffer_size_, strerror(errno)));
        }

        // Memory map with optimized flags
        map_ptr_ = static_cast<char*>(
            mmap(nullptr, buffer_size_, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_POPULATE,  // MAP_POPULATE to prefault pages
                 file_descriptor_, 0));

        if (map_ptr_ == MAP_FAILED) {
            close(file_descriptor_);
            stats_->error_count++;
            throw MappingException(std::format(
                "Failed to memory map log file. Error: {}", strerror(errno)));
        }
#endif
    }

    // Unmap the file
    void unmap() {
        std::lock_guard<std::mutex> lock(file_mutex_);

        if (map_ptr_ != nullptr) {
            // Track flush errors in statistics but continue with cleanup
            if (auto result = flush(); !result) {
                stats_->error_count++;
            }

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

    // Auto-flush thread management
    void startAutoFlushThread() {
        stop_auto_flush_.store(false, std::memory_order_relaxed);
        auto_flush_thread_ = std::thread([this]() {
            while (!stop_auto_flush_.load(std::memory_order_relaxed)) {
                // Default to 1000ms if not set
                uint32_t interval =
                    auto_flush_interval_.load(std::memory_order_relaxed);
                interval = (interval > 0) ? interval : 1000;

                // Sleep for the interval
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(interval));

                // Flush the log
                if (!stop_auto_flush_.load(std::memory_order_relaxed)) {
                    if (auto result = flush(); !result) {
                        // Log internal flush error without recursion
                        log(LogLevel::ERROR, Category::General,
                            "Auto-flush failed with error code: " +
                                std::to_string(
                                    static_cast<int>(result.error())),
                            std::source_location::current());
                    }
                }
            }
        });
    }

    void stopAutoFlushThread() {
        if (auto_flush_thread_.joinable()) {
            stop_auto_flush_.store(true, std::memory_order_relaxed);
            auto_flush_thread_.join();
        }
    }

    // Get thread name with optimized lookup
    [[nodiscard]] auto getThreadName() -> std::string {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        auto thread_id = std::this_thread::get_id();
        auto it = thread_names_.find(thread_id);
        if (it != thread_names_.end()) {
            return it->second;
        }

        // Generate auto-name with prefix
        std::ostringstream oss;
        oss << thread_name_prefix_ << thread_id;
        auto name = oss.str();

        // Cache the generated name
        thread_names_[thread_id] = name;
        return name;
    }

    // Convert log level to string - constexpr for compile-time evaluation
    [[nodiscard]] static constexpr auto logLevelToString(LogLevel level)
        -> std::string_view {
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

    // Convert category to string - constexpr for compile-time evaluation
    [[nodiscard]] static constexpr auto categoryToString(
        MmapLogger::Category category) -> std::string_view {
        using enum MmapLogger::Category;
        switch (category) {
            case General:
                return "General";
            case Network:
                return "Network";
            case Database:
                return "Database";
            case Security:
                return "Security";
            case Performance:
                return "Performance";
            case UI:
                return "UI";
            case API:
                return "API";
            case Custom:
                return "Custom";
            default:
                return "Unknown";
        }
    }

    // Format log message with optimized string handling
    [[nodiscard]] auto formatMessage(LogLevel level,
                                     MmapLogger::Category category,
                                     std::string_view msg,
                                     const std::source_location& location)
        -> std::string {
        auto timestamp = utils::getChinaTimestampString();  // Get timestamp
        auto threadName = getThreadName();
        auto levelStr = logLevelToString(level);
        auto categoryStr = categoryToString(category);

        // Format source location information - preallocate for performance
        std::string result;
        result.reserve(timestamp.size() + threadName.size() +
                       categoryStr.size() + msg.size() + 120);

        // Use std::format for modern formatting with additional category
        // information
        return std::format("[{}][{}][{}][{}] {} {}:{}:{}\n", timestamp,
                           levelStr, categoryStr, threadName, msg,
                           location.file_name(), location.line(),
                           location.function_name());
    }

    // Write to buffer with optimized memory ordering
    void writeToBuffer(std::string_view formatted_msg) {
        std::lock_guard<std::mutex> lock(file_mutex_);

        size_t msg_len = formatted_msg.length();
        size_t position = current_pos_.load(std::memory_order_relaxed);

        // Check if rotation is needed (buffer full)
        if (position + msg_len >= buffer_size_) {
            rotateLogFile();
            position = 0;
        }

        // Write message - direct memcpy for performance
        std::memcpy(map_ptr_ + position, formatted_msg.data(), msg_len);

        // Update position with release memory ordering to ensure visibility to
        // other threads
        current_pos_.store(position + msg_len, std::memory_order_release);
    }

    // Rotate log file with optional compression
    void rotateLogFile() {
        // Note: This function is called under the protection of file_mutex_

        // Unmap the current file
        if (map_ptr_ != nullptr) {
#ifdef _WIN32
            UnmapViewOfFile(map_ptr_);
            if (map_handle_ != nullptr) {
                CloseHandle(map_handle_);
                map_handle_ = nullptr;
            }
#else
            munmap(map_ptr_, buffer_size_);
#endif
            map_ptr_ = nullptr;
        }

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

            // Rotate other log files - process in reverse order to avoid
            // overwriting
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
            if (fs::exists(file_name_, ec)) {
                fs::rename(file_name_, backup, ec);

                // Compress the rotated file if enabled
                if (compression_enabled_.load(std::memory_order_relaxed)) {
                    compressFile(backup);
                }
            }

        } catch (const std::exception& e) {
            // Rotation failed, log to stderr (cannot log to the file)
            stats_->error_count++;
            std::cerr << std::format(
                "Failed to rotate memory-mapped log file: {}\n", e.what());
        }

        // Close file handle on POSIX systems
#ifndef _WIN32
        if (file_descriptor_ != -1) {
            close(file_descriptor_);
            file_descriptor_ = -1;
        }
#else
        if (file_handle_ != nullptr) {
            CloseHandle(file_handle_);
            file_handle_ = nullptr;
        }
#endif

        // Recreate and map the new file
        mapFile();

        // Reset write position
        current_pos_.store(0, std::memory_order_relaxed);

        // Update rotation count
        stats_->rotation_count++;
    }

    // Compress a file using zlib
    void compressFile(const fs::path& file_path) {
#if HAS_COMPRESSION_SUPPORT
        try {
            // Create gzip file path
            auto gz_path = file_path.string() + ".gz";

            // Open input file
            std::ifstream input_file(file_path, std::ios::binary);
            if (!input_file) {
                throw FileException(
                    std::format("Cannot open file for compression: {}",
                                file_path.string()));
            }

            // Open output gzip file
            gzFile out_file = gzopen(gz_path.c_str(), "wb");
            if (out_file == nullptr) {
                throw FileException(
                    std::format("Cannot create compressed file: {}", gz_path));
            }

            // Compress
            char buffer[4096];
            std::streamsize bytes_read;
            while (input_file.read(buffer, sizeof(buffer)),
                   (bytes_read = input_file.gcount()) > 0) {
                if (gzwrite(out_file, buffer,
                            static_cast<unsigned>(bytes_read)) == 0) {
                    gzclose(out_file);
                    throw FileException("Failed to write compressed data");
                }
            }

            // Close files
            gzclose(out_file);
            input_file.close();

            // Remove original file after successful compression
            std::error_code ec;
            fs::remove(file_path, ec);

        } catch (...) {
            stats_->error_count++;
            std::cerr << std::format("Failed to compress log file: {}\n",
                                     file_path.string());
        }
#else
        // Ignore if no compression support
        (void)file_path;
#endif
    }

    // System logging with optimized platform detection
    void logToSystem(LogLevel level, std::string_view msg,
                     const std::source_location& location) {
        // Prepare source location information with preallocation
        std::string source_info;
        source_info.reserve(100);
        source_info = std::format("{}:{}:{}", location.file_name(),
                                  location.line(), location.function_name());

#ifdef _WIN32
        // Windows Event Log implementation
        static HANDLE event_source = nullptr;
        static std::once_flag init_flag;

        // Lazy initialize event source handle with thread safety
        std::call_once(init_flag, []() {
            event_source = RegisterEventSourceW(nullptr, L"AtomLogger");
        });

        // Skip if event source couldn't be initialized
        if (event_source == nullptr) {
            stats_->error_count++;
            return;
        }

        // Convert message to wide characters with preallocation
        try {
            // Add log level and location info to the message
            std::string full_msg = std::format(
                "[{}] {} - {}", logLevelToString(level), msg, source_info);

            // Use C++20 text transcoding if available
            std::wstring wide_msg;

            // Determine buffer size
            int size_needed = MultiByteToWideChar(
                CP_UTF8, 0, full_msg.data(),
                static_cast<int>(full_msg.length()), nullptr, 0);
            if (size_needed <= 0) {
                stats_->error_count++;
                return;
            }

            wide_msg.resize(size_needed);

            // Convert to wide characters
            if (MultiByteToWideChar(CP_UTF8, 0, full_msg.data(),
                                    static_cast<int>(full_msg.length()),
                                    wide_msg.data(), size_needed) <= 0) {
                stats_->error_count++;
                return;
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
            if (!ReportEventW(event_source, event_type, 0, 0, nullptr, 1, 0,
                              &string_ptr, nullptr)) {
                stats_->error_count++;
            }

        } catch (...) {
            stats_->error_count++;
        }

#elif defined(__linux__)
        // Linux syslog implementation with thread safety
        static bool syslog_opened = false;
        static std::once_flag syslog_init_flag;

        // Use call_once for thread-safe initialization
        std::call_once(syslog_init_flag, []() {
            openlog("AtomLogger", LOG_PID | LOG_NDELAY, LOG_USER);
            syslog_opened = true;
        });

        // Map log level to syslog priority with constexpr if for performance
        int syslog_priority;
        if constexpr (std::is_enum_v<LogLevel>) {
            if (level == LogLevel::CRITICAL) {
                syslog_priority = LOG_CRIT;
            } else if (level == LogLevel::ERROR) {
                syslog_priority = LOG_ERR;
            } else if (level == LogLevel::WARN) {
                syslog_priority = LOG_WARNING;
            } else if (level == LogLevel::INFO) {
                syslog_priority = LOG_INFO;
            } else {
                syslog_priority = LOG_DEBUG;
            }
        } else {
            // Fallback if LogLevel is not an enum
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
        }

        // Send to syslog - use string_view for better performance
        try {
            std::string full_msg = std::format("{} - {}", msg, source_info);
            syslog(syslog_priority, "%.*s", static_cast<int>(full_msg.length()),
                   full_msg.data());
        } catch (...) {
            stats_->error_count++;
        }

#elif defined(__APPLE__)
        // macOS os_log implementation with thread safety
        static os_log_t log_handle = nullptr;
        static std::once_flag os_log_init_flag;

        // Use call_once for thread-safe initialization
        std::call_once(os_log_init_flag, []() {
            log_handle = os_log_create("com.lightapt.atom", "logger");
        });

        if (log_handle == nullptr) {
            stats_->error_count++;
            return;
        }

        // Prepare full message
        try {
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
            os_log_with_type(log_handle, log_type, "%{public}s",
                             full_msg.c_str());
        } catch (...) {
            stats_->error_count++;
        }
#endif
    }
};

// MmapLogger class method implementations

MmapLogger::MmapLogger(const Config& config)
    : impl_(std::make_unique<MmapLoggerImpl>(config)) {}

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

std::expected<void, LoggerErrorCode> MmapLogger::flush() noexcept {
    return impl_->flush();
}

void MmapLogger::setCategoryFilter(std::span<const Category> categories) {
    impl_->setCategoryFilter(categories);
}

void MmapLogger::addFilterPattern(std::string_view pattern) {
    impl_->addFilterPattern(pattern);
}

void MmapLogger::setAutoFlushInterval(uint32_t milliseconds) {
    impl_->setAutoFlushInterval(milliseconds);
}

std::string MmapLogger::getStatistics() const { return impl_->getStatistics(); }

void MmapLogger::enableCompression(bool enable) {
    impl_->enableCompression(enable);
}

void MmapLogger::log(LogLevel level, Category category, std::string_view msg,
                     const std::source_location& location) {
    impl_->log(level, category, msg, location);
}

}  // namespace atom::log