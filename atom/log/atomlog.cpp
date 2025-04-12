// atomlog.cpp
/*
 * atomlog.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Enhanced Logger Implementation for Atom with C++20 Features

**************************************************/

#include "atomlog.hpp"

#include <algorithm>
// #include <chrono> // Removed unused include
#include <condition_variable>
#include <format>  // Keep for std::format and std::vformat
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
// #include <sstream> // Removed unused include
#include <stop_token>  // Include stop_token header
#include <thread>
#include <unordered_map>
#include <utility>  // For std::move

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#elif defined(__linux__) || defined(__APPLE__)
#include <syslog.h>
#include <sstream>  // Needed for getThreadName fallback
#elif defined(__ANDROID__)
#include <android/log.h>
#include <sstream>  // Needed for getThreadName fallback
#else
#include <sstream>  // Needed for getThreadName fallback
#endif

#include "atom/utils/time.hpp"

namespace atom::log {

class Logger::LoggerImpl : public std::enable_shared_from_this<LoggerImpl> {
public:
    LoggerImpl(fs::path file_name_, LogLevel min_level, size_t max_file_size,
               int max_files)
        : file_name_(std::move(file_name_)),
          max_file_size_(max_file_size),
          max_files_(max_files),
          min_level_(min_level),
          system_logging_enabled_(false) {
        rotateLogFile();
        worker_ = std::jthread([this](std::stop_token st) { this->run(st); });
    }

    ~LoggerImpl() {
        // Request stop and wait for worker thread to finish processing queue
        worker_.request_stop();
        {
            std::lock_guard lock(queue_mutex_);
            finished_ = true;  // Signal that no more items will be added
        }
        cv_.notify_one();  // Wake up worker if waiting
        // jthread destructor automatically joins

        if (log_file_.is_open()) {
            log_file_.close();
        }

#if defined(__linux__) || defined(__APPLE__)
        if (system_logging_enabled_) {
            closelog();
        }
#elif defined(_WIN32)  // Ensure h_event_log_ is handled on Windows
        if (h_event_log_) {
            DeregisterEventSource(h_event_log_);
            h_event_log_ = nullptr;
        }
#endif
    }

    // Prevent copying
    LoggerImpl(const LoggerImpl&) = delete;
    LoggerImpl& operator=(const LoggerImpl&) = delete;

    // Manually defined move constructor
    LoggerImpl(LoggerImpl&& other) noexcept
        : file_name_(std::move(other.file_name_)),
          log_file_(std::move(other.log_file_)),  // ofstream is movable
          // queue_mutex_ and cv_ are default constructed
          finished_(
              other.finished_),  // Copy finished state under lock? Needs care.
          worker_(std::move(other.worker_)),
          max_file_size_(other.max_file_size_),
          max_files_(other.max_files_),
          min_level_(other.min_level_),
          thread_names_(std::move(other.thread_names_)),
          pattern_(std::move(other.pattern_)),
          sinks_(std::move(other.sinks_)),
          system_logging_enabled_(other.system_logging_enabled_),
#ifdef _WIN32
          h_event_log_(other.h_event_log_),
#endif
          custom_levels_(std::move(other.custom_levels_)) {
        // Lock the source's mutex to safely move the queue contents
        // This is complex because the source worker might still be running.
        // A cleaner approach might be to disallow moving active loggers
        // or ensure the source worker is stopped before moving.
        // For simplicity here, we assume move happens when source is inactive
        // or about to be destroyed.
        std::lock_guard lock(other.queue_mutex_);
        log_queue_ = std::move(other.log_queue_);
        other.finished_ = true;  // Mark source as finished
#ifdef _WIN32
        other.h_event_log_ = nullptr;  // Prevent double DeregisterEventSource
#endif
        // Note: Moving an active logger with a running jthread is tricky.
        // The moved-from object's jthread destructor will still run.
        // Consider stopping the worker in the source before move if needed.
    }

    // Manually defined move assignment operator
    LoggerImpl& operator=(LoggerImpl&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        // Stop the current worker thread before assigning
        // This prevents issues with the jthread destructor
        if (worker_.joinable()) {
            worker_.request_stop();
            {
                std::lock_guard lock(queue_mutex_);
                finished_ = true;  // Signal current worker
            }
            cv_.notify_one();
            // worker_.join(); // jthread destructor handles join
        }

        // Lock both mutexes to prevent deadlock (using std::scoped_lock)
        std::scoped_lock lock(queue_mutex_, other.queue_mutex_);

        file_name_ = std::move(other.file_name_);
        log_file_ = std::move(other.log_file_);
        log_queue_ = std::move(other.log_queue_);
        // cv_ doesn't need moving
        finished_ = other.finished_;
        worker_ = std::move(other.worker_);  // Move the jthread handle
        max_file_size_ = other.max_file_size_;
        max_files_ = other.max_files_;
        min_level_ = other.min_level_;
        thread_names_ = std::move(other.thread_names_);
        pattern_ = std::move(other.pattern_);
        sinks_ = std::move(other.sinks_);
        system_logging_enabled_ = other.system_logging_enabled_;
#ifdef _WIN32
        h_event_log_ = other.h_event_log_;
        other.h_event_log_ = nullptr;  // Prevent double DeregisterEventSource
#endif
        custom_levels_ = std::move(other.custom_levels_);

        other.finished_ = true;  // Mark source as finished

        return *this;
    }

    void setThreadName(const std::string& name) {
        std::lock_guard lock(thread_mutex_);
        thread_names_[std::this_thread::get_id()] = name;
    }

    void setLevel(LogLevel level) {
        std::lock_guard lock(level_mutex_);
        min_level_ = level;
    }

    void setPattern(const std::string& pattern) {
        std::lock_guard lock(pattern_mutex_);
        pattern_ = pattern;
    }

    void registerSink(const std::shared_ptr<LoggerImpl>& logger) {
        if (!logger || logger.get() == this) {  // Add null check
            // Prevent registering null or self
            return;
        }
        std::lock_guard lock(sinks_mutex_);
        // Avoid duplicates
        if (std::find(sinks_.begin(), sinks_.end(), logger) == sinks_.end()) {
            sinks_.emplace_back(logger);
        }
    }

    void removeSink(const std::shared_ptr<LoggerImpl>& logger) {
        if (!logger)
            return;  // Add null check
        std::lock_guard lock(sinks_mutex_);
        sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), logger),
                     sinks_.end());
    }

    void clearSinks() {
        std::lock_guard lock(sinks_mutex_);
        sinks_.clear();
    }

    void enableSystemLogging(bool enable) {
        std::lock_guard lock(system_log_mutex_);
        if (system_logging_enabled_ == enable)
            return;  // Avoid redundant calls

        system_logging_enabled_ = enable;

#ifdef _WIN32
        if (system_logging_enabled_) {
            if (!h_event_log_) {  // Check if already registered
                h_event_log_ = RegisterEventSourceW(nullptr, L"AtomLogger");
                if (!h_event_log_) {
                    // Log error or throw? For now, just disable system logging
                    // again.
                    system_logging_enabled_ = false;
                    // Consider logging this failure to the file logger itself
                    log(LogLevel::ERROR,
                        "Failed to register Windows Event Source.");
                }
            }
        } else if (h_event_log_) {
            DeregisterEventSource(h_event_log_);
            h_event_log_ = nullptr;
        }
#elif defined(__linux__) || defined(__APPLE__)
        if (system_logging_enabled_) {
            // openlog can be called multiple times, subsequent calls change
            // identity
            openlog("AtomLogger", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        } else {
            // closelog should only be called when completely done, typically in
            // destructor If disabling temporarily, maybe just skip syslog
            // calls? For simplicity, we keep closelog in destructor only.
        }
#endif
    }

    void registerCustomLogLevel(const std::string& name, int severity) {
        std::lock_guard lock(custom_level_mutex_);
        custom_levels_[name] = severity;
    }

    // Changed msg parameter to std::string_view for efficiency
    void log(LogLevel level, std::string_view msg) {
        // Check level early
        // Use relaxed atomic load if min_level_ becomes atomic later
        std::shared_lock levelLock(level_mutex_);
        if (static_cast<int>(level) < static_cast<int>(min_level_)) {
            levelLock.unlock();  // Unlock early if not logging
            return;
        }
        levelLock.unlock();  // Unlock after check

        // Format message outside the queue lock
        auto formattedMsg = formatMessage(level, msg);

        {
            std::lock_guard lock(queue_mutex_);
            if (finished_)
                return;  // Don't enqueue if logger is shutting down
            // Consider checking queue size if limiting is needed
            log_queue_.push(std::move(formattedMsg));  // Move formatted message
        }
        cv_.notify_one();

        // Check system logging enablement outside queue lock
        std::shared_lock sysLogLock(system_log_mutex_);
        bool sysLogEnabled = system_logging_enabled_;
        sysLogLock.unlock();

        if (sysLogEnabled) {
            // Pass the already formatted message if system log doesn't need
            // reformatting Assuming logToSystem wants the fully formatted
            // message
            logToSystem(level, formattedMsg);  // Pass formatted message
        }

        // Dispatch to sinks outside queue lock
        dispatchToSinks(level, msg);  // Pass original message
    }

private:
    fs::path file_name_;
    std::ofstream log_file_;
    std::queue<std::string> log_queue_;
    std::mutex queue_mutex_;  // Protects log_queue_ and finished_ flag
    std::condition_variable_any
        cv_;  // Use condition_variable_any for stop_token support
    bool finished_ = false;  // Protected by queue_mutex_
    std::jthread worker_;
    size_t max_file_size_;
    int max_files_;
    LogLevel min_level_;  // Protected by level_mutex_
    std::unordered_map<std::thread::id, std::string>
        thread_names_;                         // Protected by thread_mutex_
    std::string pattern_ = "[{}][{}][{}] {}";  // Protected by pattern_mutex_
    std::vector<std::shared_ptr<LoggerImpl>>
        sinks_;                            // Protected by sinks_mutex_
    bool system_logging_enabled_ = false;  // Protected by system_log_mutex_

#ifdef _WIN32
    HANDLE h_event_log_ = nullptr;  // Protected indirectly by system_log_mutex_
                                    // during enable/disable
#endif

    // Mutexes for thread-safe access to members
    mutable std::shared_mutex thread_mutex_;
    mutable std::shared_mutex pattern_mutex_;
    mutable std::shared_mutex sinks_mutex_;
    mutable std::shared_mutex system_log_mutex_;
    mutable std::shared_mutex level_mutex_;
    mutable std::shared_mutex custom_level_mutex_;
    std::unordered_map<std::string, int>
        custom_levels_;  // Protected by custom_level_mutex_

    void rotateLogFile() {
        // This function modifies log_file_ and interacts with the filesystem.
        // It's called from the constructor and the worker thread.
        // The constructor call is safe.
        // The worker thread call needs protection if other threads might access
        // log_file_ or filesystem state related to log files concurrently.
        // Currently, only the worker thread calls this after construction.
        // Let's assume queue_mutex_ is sufficient if file operations are quick
        // and primarily done by the worker. If rotation becomes complex or
        // involves external interaction, a dedicated file mutex might be
        // needed.

        // std::lock_guard lock(queue_mutex_); // Assuming this is sufficient
        // for now If rotation is slow, consider a separate mutex to avoid
        // blocking logging queue.

        if (log_file_.is_open()) {
            log_file_.close();
        }

        try {
            std::error_code size_ec;
            uintmax_t current_size = fs::file_size(file_name_, size_ec);
            if (!size_ec && max_files_ > 0 &&
                current_size > 0) {  // Check if file exists and is not empty
                auto extension =
                    file_name_.extension().string();     // Use string()
                auto stem = file_name_.stem().string();  // Use string()
                auto parent_path = file_name_.parent_path();

                // Construct filenames using fs::path concatenation
                auto make_path = [&](int index) {
                    return parent_path /
                           (stem + "." + std::to_string(index) + extension);
                };

                // Shift existing rotated files
                std::error_code ec;  // Reuse error code
                fs::path dst_last = make_path(max_files_);
                if (fs::exists(dst_last, ec)) {  // Remove the oldest file first
                    fs::remove(dst_last, ec);
                    if (ec) {
                        fprintf(
                            stderr, "Log rotation: Failed to remove %s: %s\n",
                            dst_last.string().c_str(), ec.message().c_str());
                        ec.clear();
                    }
                } else if (ec) {
                    fprintf(stderr,
                            "Log rotation: Failed check exists %s: %s\n",
                            dst_last.string().c_str(), ec.message().c_str());
                    ec.clear();
                }

                for (int i = max_files_ - 1; i > 0; --i) {
                    fs::path src = make_path(i);
                    fs::path dst = make_path(i + 1);

                    if (fs::exists(src, ec) && !ec) {
                        fs::rename(src, dst, ec);
                        if (ec) {
                            fprintf(
                                stderr,
                                "Log rotation: Failed to rename %s to %s: %s\n",
                                src.string().c_str(), dst.string().c_str(),
                                ec.message().c_str());
                            ec.clear();
                        }
                    } else if (ec) {
                        fprintf(stderr,
                                "Log rotation: Failed check exists %s: %s\n",
                                src.string().c_str(), ec.message().c_str());
                        ec.clear();
                    }
                }

                // Rename current log file to .1
                fs::path dst1 = make_path(1);
                fs::rename(file_name_, dst1, ec);
                if (ec) {
                    fprintf(stderr,
                            "Log rotation: Failed to rename %s to %s: %s\n",
                            file_name_.string().c_str(), dst1.string().c_str(),
                            ec.message().c_str());
                    ec.clear();
                }

            } else if (size_ec &&
                       size_ec != std::errc::no_such_file_or_directory) {
                fprintf(stderr,
                        "Log rotation: Failed to get file size for %s: %s\n",
                        file_name_.string().c_str(), size_ec.message().c_str());
            }
        } catch (const fs::filesystem_error& e) {
            // Log rotation error to stderr or a fallback mechanism?
            // Cannot use the logger itself easily here.
            fprintf(stderr, "Log rotation failed: %s\n", e.what());
        }

        // Open the new log file
        log_file_.open(file_name_, std::ios::out | std::ios::app);
        if (!log_file_.is_open()) {
            // Critical failure, throw exception or log to stderr
            fprintf(stderr, "CRITICAL: Failed to open log file: %s\n",
                    file_name_.string().c_str());
            // THROW_FAIL_TO_OPEN_FILE("Failed to open log file: " +
            //                         file_name_.string());
        }
    }

    auto getThreadName() -> std::string {
        // Use shared lock for reading thread_names_
        std::shared_lock lock(thread_mutex_);
        auto thread_id = std::this_thread::get_id();
        auto it = thread_names_.find(thread_id);
        if (it != thread_names_.end()) {
            return it->second;
        }
        lock.unlock();  // Unlock before potential formatting/hashing

        // Format thread ID if name not found
        // Use std::format for consistency if available and suitable
        // std::hash might not be stable across runs, consider alternative ID
        // representation if needed return std::format("{}",
        // std::hash<std::thread::id>{}(thread_id)); Using ostringstream as a
        // portable way to format thread::id
        std::ostringstream oss;
        oss << thread_id;
        return oss.str();
    }

    static auto logLevelToString(LogLevel level)
        -> std::string_view {  // Return string_view
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
            default:
                return "UNKNOWN";
        }
    }

    // Changed msg parameter to std::string_view
    auto formatMessage(LogLevel level, std::string_view msg) -> std::string {
        auto currentTime =
            utils::getChinaTimestampString();  // Assuming this returns
                                               // std::string
        auto threadName = getThreadName();  // Assuming this returns std::string
        auto levelStr = logLevelToString(level);  // Returns std::string_view

        // Use shared lock for reading pattern_
        std::shared_lock patternLock(pattern_mutex_);
        // Ensure pattern_ is treated as a runtime format string if needed
        // return std::format(pattern_, currentTime, levelStr, threadName, msg);
        // // If pattern_ is simple

        // Use vformat for runtime format string
        // Need to ensure arguments are correctly passed to make_format_args
        // Arguments must match the placeholders in pattern_
        // Assuming pattern_ is like "[{}] [{}] [{}] {}"
        // Ensure all arguments are convertible for std::make_format_args
        // std::string_view is convertible.
        return std::vformat(
            pattern_,
            std::make_format_args(currentTime, levelStr, threadName, msg));
    }

    void run(std::stop_token stop_token) {
        while (true) {
            std::string msg;
            bool should_rotate = false;
            {
                std::unique_lock lock(queue_mutex_);
                // Wait using condition_variable_any with stop_token
                if (!cv_.wait(lock, stop_token, [this] {
                        return !log_queue_.empty() || finished_;
                    })) {
                    // Wait was cancelled by stop_token
                    // Process remaining messages if any, then exit
                    if (log_queue_.empty())
                        break;
                }

                // Check finished condition (might be set after stop_token)
                if (finished_ && log_queue_.empty()) {
                    break;  // Exit if finished and queue is empty
                }

                // If we woke up and queue is not empty
                if (!log_queue_.empty()) {
                    msg = std::move(log_queue_.front());  // Move message
                    log_queue_.pop();
                } else {
                    // Spurious wake or finished_ is true but queue became empty
                    // between predicate check and now. Loop again.
                    continue;
                }

            }  // Mutex unlocked here

            // Perform file I/O outside the lock
            if (log_file_.is_open()) {
                log_file_ << msg << std::endl;
                // Flush frequently for visibility, maybe less often for
                // performance
                log_file_.flush();

                // Check file size for rotation
                // Use try-catch for tellp in case of stream errors
                try {
                    // Check file size only after writing
                    if (max_file_size_ > 0) {
                        // Get current position which is approx file size after
                        // write+flush
                        std::streampos current_pos = log_file_.tellp();
                        if (current_pos != static_cast<std::streampos>(-1) &&
                            current_pos >=
                                static_cast<std::streampos>(max_file_size_)) {
                            should_rotate = true;
                        }
                    }
                } catch (const std::ios_base::failure& e) {
                    // Handle stream error, maybe log to stderr
                    fprintf(stderr, "Error checking log file size: %s\n",
                            e.what());
                }
            } else {
                // Handle case where log file is not open (e.g., initial open
                // failed)
                fprintf(stderr, "Log file is not open. Message lost: %s\n",
                        msg.c_str());
            }

            if (should_rotate) {
                // Rotation must be done carefully.
                // No lock needed here if rotateLogFile handles its own safety
                // or if only this thread modifies the file stream state.
                rotateLogFile();
            }

            // Check stop token again after processing message, before
            // potentially waiting again
            if (stop_token.stop_requested() && log_queue_.empty()) {
                // Check queue one last time under lock before exiting loop
                std::lock_guard lock(queue_mutex_);
                if (log_queue_.empty())
                    break;
            }
        }
    }

    // Changed msg parameter to std::string_view
    void logToSystem(LogLevel level, std::string_view msg) const {
        // No need for system_log_mutex_ here if system_logging_enabled_ check
        // happens before calling
#ifdef _WIN32
        if (h_event_log_) {
            using enum LogLevel;
            WORD eventType;
            switch (level) {
                case CRITICAL:
                    eventType = EVENTLOG_ERROR_TYPE;
                    break;  // Treat critical as error
                case ERROR:
                    eventType = EVENTLOG_ERROR_TYPE;
                    break;
                case WARN:
                    eventType = EVENTLOG_WARNING_TYPE;
                    break;
                case INFO:
                    eventType = EVENTLOG_INFORMATION_TYPE;
                    break;
                case DEBUG:
                    eventType = EVENTLOG_INFORMATION_TYPE;
                    break;  // Treat debug as info
                case TRACE:
                    eventType = EVENTLOG_INFORMATION_TYPE;
                    break;  // Treat trace as info
                default:
                    eventType = EVENTLOG_INFORMATION_TYPE;
                    break;
            }

            // Convert UTF-8 string_view to wide string for Windows API
            int wide_len =
                MultiByteToWideChar(CP_UTF8, 0, msg.data(),
                                    static_cast<int>(msg.length()), nullptr, 0);
            if (wide_len > 0) {
                std::wstring wideMsg(wide_len, 0);
                MultiByteToWideChar(CP_UTF8, 0, msg.data(),
                                    static_cast<int>(msg.length()), &wideMsg[0],
                                    wide_len);
                LPCWSTR messages[] = {wideMsg.c_str()};
                ReportEventW(h_event_log_, eventType, 0, 0, nullptr, 1, 0,
                             messages, nullptr);
            } else {
                // Log conversion error?
                fprintf(stderr,
                        "Failed to convert log message to wide string for "
                        "Windows Event Log.\n");
            }
        }
#elif defined(__linux__) || defined(__APPLE__)
        // system_logging_enabled_ check should happen before calling this
        // function if (system_logging_enabled_) { // Redundant check?
        using enum LogLevel;
        int priority;
        switch (level) {
            case CRITICAL:
                priority = LOG_CRIT;
                break;
            case ERROR:
                priority = LOG_ERR;
                break;
            case WARN:
                priority = LOG_WARNING;
                break;
            case INFO:
                priority = LOG_INFO;
                break;
            case DEBUG:
                priority = LOG_DEBUG;
                break;
            case TRACE:
                priority = LOG_DEBUG;
                break;  // Map TRACE to DEBUG for syslog
            default:
                priority = LOG_NOTICE;
                break;  // Default mapping
        }
        // syslog expects a null-terminated C string. Create one temporarily if
        // msg is not null-terminated. Since msg is now string_view, we need to
        // copy it.
        std::string msg_str(msg);
        syslog(priority, "%s", msg_str.c_str());
        // }
#elif defined(__ANDROID__)
        // if (system_logging_enabled_) { // Redundant check?
        using enum LogLevel;
        android_LogPriority priority;
        switch (level) {
            case CRITICAL:
                priority = ANDROID_LOG_FATAL;
                break;
            case ERROR:
                priority = ANDROID_LOG_ERROR;
                break;
            case WARN:
                priority = ANDROID_LOG_WARN;
                break;
            case INFO:
                priority = ANDROID_LOG_INFO;
                break;
            case DEBUG:
                priority = ANDROID_LOG_DEBUG;
                break;
            case TRACE:
                priority = ANDROID_LOG_VERBOSE;
                break;  // Map TRACE to VERBOSE
            default:
                priority = ANDROID_LOG_DEFAULT;
                break;
        }
        // __android_log_print also expects a null-terminated C string.
        std::string msg_str(msg);
        __android_log_print(priority, "AtomLogger", "%s", msg_str.c_str());
        // }
#else
        // Suppress unused variable warning if no system logging is implemented
        (void)level;
        (void)msg;
#endif
    }

    // Changed msg parameter to std::string_view
    void dispatchToSinks(LogLevel level, std::string_view msg) {
        // Use shared lock for reading sinks_
        std::shared_lock lock(sinks_mutex_);
        if (sinks_.empty()) {
            return;  // Avoid iterating if no sinks
        }
        // Create a copy of the sink pointers to avoid holding the lock
        // while calling potentially long-running sink->log methods.
        auto sinks_copy = sinks_;
        lock.unlock();

        // Convert string_view to std::string as sink->log expects std::string
        // Do this only once before the loop
        std::string msg_str(msg);

        for (const auto& sink : sinks_copy) {
            if (sink) {  // Check if sink pointer is valid
                // Pass the original message string
                sink->log(level, msg_str);  // Pass std::string copy
            }
        }
    }

    // This function seems unused based on the provided code.
    // If used, ensure thread safety with custom_level_mutex_.
    /*
    auto getCustomLogLevel(const std::string& name) -> LogLevel {
        std::shared_lock lock(custom_level_mutex_);
        auto it = custom_levels_.find(name);
        if (it != custom_levels_.end()) {
            return static_cast<LogLevel>(it->second);
        }
        return LogLevel::INFO; // Default or throw?
    }
    */
};

// `Logger` 类的方法实现

Logger::Logger(const fs::path& file_name, LogLevel min_level,
               size_t max_file_size, int max_files)
    : impl_(std::make_shared<LoggerImpl>(file_name, min_level, max_file_size,
                                         max_files)) {}

// Define destructor body (even if empty) because LoggerImpl is forward-declared
Logger::~Logger() = default;

// Use String consistently as declared in the header
void Logger::setThreadName(const String& name) {
    // Pass String directly, LoggerImpl expects std::string
    impl_->setThreadName(name);
}

void Logger::setLevel(LogLevel level) { impl_->setLevel(level); }

// Use String consistently
void Logger::setPattern(const String& pattern) {
    // Pass String directly, LoggerImpl expects std::string
    impl_->setPattern(pattern);
}

void Logger::registerSink(const std::shared_ptr<Logger>& logger) {
    if (logger && logger.get() != this) {  // Prevent self-sink
        impl_->registerSink(logger->impl_);
    }
}

void Logger::removeSink(const std::shared_ptr<Logger>& logger) {
    if (logger) {
        impl_->removeSink(logger->impl_);
    }
}

void Logger::clearSinks() { impl_->clearSinks(); }

void Logger::enableSystemLogging(bool enable) {
    impl_->enableSystemLogging(enable);
}

// Use String consistently
void Logger::registerCustomLogLevel(const String& name, int severity) {
    // Pass String directly, LoggerImpl expects std::string
    impl_->registerCustomLogLevel(name, severity);
}

// Keep std::string here as std::format returns std::string
void Logger::log(LogLevel level, const std::string& msg) {
    // Pass as string_view for efficiency
    impl_->log(level, std::string_view(msg));
}

}  // namespace atom::log