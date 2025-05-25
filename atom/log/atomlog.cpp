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
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <sstream>
#include <stop_token>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#undef ERROR
#elif defined(__linux__)
#include <syslog.h>
#include <systemd/sd-journal.h>
#elif defined(__APPLE__)
#include <os/log.h>
#include <syslog.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#endif

#include "atom/utils/time.hpp"

namespace atom::log {

constexpr std::string_view logLevelToString(LogLevel level) noexcept {
    constexpr std::array<std::string_view, 8> level_strings = {
        "TRACE", "DEBUG",    "INFO", "WARN",
        "ERROR", "CRITICAL", "OFF",  "UNKNOWN"};

    const auto index = static_cast<size_t>(level);
    return index < 7 ? level_strings[index] : level_strings[7];
}

LogLevel stringToLogLevel(std::string_view str) {
    static const std::unordered_map<std::string_view, LogLevel> level_map = {
        {"TRACE", LogLevel::TRACE}, {"DEBUG", LogLevel::DEBUG},
        {"INFO", LogLevel::INFO},   {"WARN", LogLevel::WARN},
        {"ERROR", LogLevel::ERROR}, {"CRITICAL", LogLevel::CRITICAL},
        {"OFF", LogLevel::OFF}};

    auto it = level_map.find(str);
    return it != level_map.end() ? it->second : LogLevel::INFO;
}

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string timestamp;
    std::string thread_name;
    std::source_location location;

    LogEntry(LogLevel lvl, std::string msg, std::string ts, std::string tn,
             const std::source_location& loc)
        : level(lvl),
          message(std::move(msg)),
          timestamp(std::move(ts)),
          thread_name(std::move(tn)),
          location(loc) {}
};

class Logger::LoggerImpl {
public:
    LoggerImpl(fs::path file_name, LogLevel min_level, size_t max_file_size,
               int max_files)
        : file_name_(std::move(file_name)),
          max_file_size_(max_file_size),
          max_files_(max_files),
          min_level_(min_level),
          current_format_(LogFormat::SIMPLE),
          batch_size_(64),
          flush_interval_(std::chrono::milliseconds(1000)),
          system_logging_enabled_(false),
          async_logging_enabled_(true),
          color_output_enabled_(false),
          memory_logging_enabled_(false),
          max_memory_entries_(1000),
          compression_enabled_(false),
          is_enabled_(true) {
        pattern_ = "[{}][{}][{}] {} - {}:{}";
        openLogFile();

        if (async_logging_enabled_) {
            startWorkerThread();
        }

#ifdef __APPLE__
        os_log_handle_ = os_log_create("com.lightapt.atomlogger", "main");
#endif
    }

    ~LoggerImpl() {
        stopWorkerThread();
        closeLogFile();

#if defined(__linux__)
        if (system_logging_enabled_) {
            closelog();
        }
#elif defined(__APPLE__)
        if (system_logging_enabled_) {
            closelog();
        }
#elif defined(_WIN32)
        if (h_event_log_) {
            DeregisterEventSource(h_event_log_);
        }
#endif
    }

    void setLevel(LogLevel level) {
        std::lock_guard lock(config_mutex_);
        min_level_ = level;
    }

    void setPattern(std::string_view pattern) {
        std::lock_guard lock(config_mutex_);
        pattern_ = pattern;
    }

    void setThreadName(std::string_view name) {
        std::lock_guard lock(thread_mutex_);
        thread_names_[std::this_thread::get_id()] = std::string(name);
    }

    void setFormat(LogFormat format) {
        std::lock_guard lock(config_mutex_);
        current_format_ = format;
    }

    void setCustomFormatter(LogFormatter formatter) {
        std::lock_guard lock(config_mutex_);
        custom_formatter_ = std::move(formatter);
    }

    void addFilter(LogFilter filter) {
        std::lock_guard lock(config_mutex_);
        filters_.push_back(std::move(filter));
    }

    void clearFilters() {
        std::lock_guard lock(config_mutex_);
        filters_.clear();
    }

    void setBatchSize(size_t size) {
        std::lock_guard lock(config_mutex_);
        batch_size_ = std::max(size_t(1), size);
    }

    void setFlushInterval(std::chrono::milliseconds interval) {
        std::lock_guard lock(config_mutex_);
        flush_interval_ = interval;
    }

    void setCompressionEnabled(bool enabled) {
        std::lock_guard lock(config_mutex_);
        compression_enabled_ = enabled;
    }

    void setEncryptionKey(std::string_view key) {
        std::lock_guard lock(config_mutex_);
        encryption_key_ = key;
    }

    void enableAsyncLogging(bool enable) {
        std::lock_guard lock(config_mutex_);
        if (async_logging_enabled_ != enable) {
            async_logging_enabled_ = enable;
            if (enable && !worker_.joinable()) {
                startWorkerThread();
            } else if (!enable && worker_.joinable()) {
                stopWorkerThread();
            }
        }
    }

    void enableColorOutput(bool enable) {
        std::lock_guard lock(config_mutex_);
        color_output_enabled_ = enable;
    }

    void enableMemoryLogging(bool enable, size_t max_entries) {
        std::lock_guard lock(config_mutex_);
        memory_logging_enabled_ = enable;
        max_memory_entries_ = max_entries;
        if (!enable) {
            memory_logs_.clear();
        }
    }

    bool shouldLog(LogLevel level) const noexcept {
        return is_enabled_.load(std::memory_order_relaxed) &&
               static_cast<int>(level) >=
                   static_cast<int>(min_level_.load(std::memory_order_relaxed));
    }

    LogLevel getLevel() const noexcept {
        return min_level_.load(std::memory_order_relaxed);
    }

    size_t getQueueSize() const noexcept {
        std::shared_lock lock(queue_mutex_);
        return log_queue_.size();
    }

    bool isEnabled() const noexcept {
        return is_enabled_.load(std::memory_order_relaxed);
    }

    std::vector<std::string> getMemoryLogs() const {
        std::shared_lock lock(memory_mutex_);
        return memory_logs_;
    }

    std::string getStats() const {
        std::ostringstream oss;
        oss << "Logger Stats:\n"
            << "  Level: " << logLevelToString(getLevel()) << "\n"
            << "  Queue Size: " << getQueueSize() << "\n"
            << "  Memory Logs: " << getMemoryLogs().size() << "\n"
            << "  Batch Size: " << batch_size_ << "\n"
            << "  Async Enabled: " << async_logging_enabled_ << "\n";
        return oss.str();
    }

    void registerSink(std::shared_ptr<LoggerImpl> logger) {
        if (!logger || logger.get() == this)
            return;

        std::lock_guard lock(sinks_mutex_);
        auto it = std::find(sinks_.begin(), sinks_.end(), logger);
        if (it == sinks_.end()) {
            sinks_.push_back(std::move(logger));
        }
    }

    void removeSink(std::shared_ptr<LoggerImpl> logger) {
        if (!logger)
            return;

        std::lock_guard lock(sinks_mutex_);
        sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), logger),
                     sinks_.end());
    }

    void clearSinks() {
        std::lock_guard lock(sinks_mutex_);
        sinks_.clear();
    }

    void enableSystemLogging(bool enable) {
        std::lock_guard lock(config_mutex_);
        if (system_logging_enabled_ == enable)
            return;

        system_logging_enabled_ = enable;

#ifdef _WIN32
        if (enable) {
            if (!h_event_log_) {
                h_event_log_ = RegisterEventSourceW(nullptr, L"AtomLogger");
                if (!h_event_log_) {
                    system_logging_enabled_ = false;
                }
            }
        } else if (h_event_log_) {
            DeregisterEventSource(h_event_log_);
            h_event_log_ = nullptr;
        }
#elif defined(__linux__) || defined(__APPLE__)
        if (enable) {
            openlog("AtomLogger", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        }
#endif
    }

    void registerCustomLogLevel(std::string_view name, int severity) {
        std::lock_guard lock(config_mutex_);
        custom_levels_[std::string(name)] = severity;
    }

    void logCustomLevel(std::string_view level_name, std::string_view msg,
                        const std::source_location& location) {
        std::shared_lock lock(config_mutex_);
        auto it = custom_levels_.find(std::string(level_name));
        if (it != custom_levels_.end()) {
            lock.unlock();
            if (it->second >= static_cast<int>(min_level_.load())) {
                log(static_cast<LogLevel>(it->second), std::string(msg),
                    location);
            }
        }
    }

    void log(LogLevel level, std::string msg,
             const std::source_location& location) {
        if (!shouldLog(level))
            return;

        auto timestamp = utils::getChinaTimestampString();
        auto thread_name = getThreadName();

        {
            std::shared_lock lock(config_mutex_);
            for (const auto& filter : filters_) {
                if (!filter(level, msg)) {
                    return;
                }
            }
        }

        if (memory_logging_enabled_) {
            addToMemoryLog(
                formatMessage(level, msg, location, timestamp, thread_name));
        }

        if (async_logging_enabled_) {
            {
                std::lock_guard lock(queue_mutex_);
                if (!finished_) {
                    log_queue_.emplace(level, std::move(msg),
                                       std::move(timestamp),
                                       std::move(thread_name), location);
                    if (log_queue_.size() >= batch_size_) {
                        cv_.notify_one();
                    }
                }
            }
        } else {
            writeToFile(
                formatMessage(level, msg, location, timestamp, thread_name));
        }

        if (system_logging_enabled_) {
            logToSystem(level, msg, location);
        }

        dispatchToSinks(level, msg, location);
    }

    void flush() {
        if (async_logging_enabled_) {
            cv_.notify_all();
        }

        std::lock_guard lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_.flush();
        }
    }

    void forceFlush() {
        if (async_logging_enabled_) {
            std::unique_lock lock(queue_mutex_);
            cv_.wait(lock, [this] { return log_queue_.empty(); });
        }
        flush();
    }

    void rotate() {
        std::lock_guard lock(file_mutex_);
        rotateLogFile();
    }

    void close() {
        is_enabled_.store(false, std::memory_order_relaxed);
        stopWorkerThread();
        closeLogFile();
    }

    void reopen() {
        is_enabled_.store(true, std::memory_order_relaxed);
        openLogFile();
        if (async_logging_enabled_) {
            startWorkerThread();
        }
    }

private:
    fs::path file_name_;
    std::ofstream log_file_;
    std::queue<LogEntry> log_queue_;
    mutable std::shared_mutex queue_mutex_;
    std::condition_variable_any cv_;
    std::atomic<bool> finished_{false};
    std::jthread worker_;

    size_t max_file_size_;
    int max_files_;
    std::atomic<LogLevel> min_level_;
    std::unordered_map<std::thread::id, std::string> thread_names_;
    std::string pattern_;
    std::vector<std::shared_ptr<LoggerImpl>> sinks_;
    std::vector<LogFilter> filters_;
    LogFormatter custom_formatter_;
    std::unordered_map<std::string, int> custom_levels_;
    std::vector<std::string> memory_logs_;

    LogFormat current_format_;
    size_t batch_size_;
    std::chrono::milliseconds flush_interval_;
    std::atomic<bool> system_logging_enabled_;
    std::atomic<bool> async_logging_enabled_;
    std::atomic<bool> color_output_enabled_;
    std::atomic<bool> memory_logging_enabled_;
    size_t max_memory_entries_;
    std::atomic<bool> compression_enabled_;
    std::string encryption_key_;
    std::atomic<bool> is_enabled_;

#ifdef _WIN32
    HANDLE h_event_log_ = nullptr;
#elif defined(__APPLE__)
    os_log_t os_log_handle_ = nullptr;
#endif

    mutable std::shared_mutex thread_mutex_;
    mutable std::shared_mutex config_mutex_;
    mutable std::shared_mutex sinks_mutex_;
    mutable std::shared_mutex file_mutex_;
    mutable std::shared_mutex memory_mutex_;

    void startWorkerThread() {
        finished_.store(false);
        worker_ = std::jthread([this](std::stop_token st) { workerLoop(st); });
    }

    void stopWorkerThread() {
        if (worker_.joinable()) {
            finished_.store(true);
            cv_.notify_all();
            worker_.request_stop();
        }
    }

    void workerLoop(std::stop_token stop_token) {
        std::vector<LogEntry> batch;
        batch.reserve(batch_size_);

        auto last_flush = std::chrono::steady_clock::now();

        while (!stop_token.stop_requested()) {
            {
                std::unique_lock lock(queue_mutex_);

                if (!cv_.wait_for(lock, flush_interval_, [this] {
                        return !log_queue_.empty() || finished_.load();
                    })) {
                    if (std::chrono::steady_clock::now() - last_flush >=
                        flush_interval_) {
                        std::lock_guard file_lock(file_mutex_);
                        if (log_file_.is_open()) {
                            log_file_.flush();
                            last_flush = std::chrono::steady_clock::now();
                        }
                    }
                    continue;
                }

                if (finished_.load() && log_queue_.empty())
                    break;

                while (!log_queue_.empty() && batch.size() < batch_size_) {
                    batch.push_back(std::move(log_queue_.front()));
                    log_queue_.pop();
                }
            }

            if (!batch.empty()) {
                processBatch(batch);
                batch.clear();
                last_flush = std::chrono::steady_clock::now();
            }
        }
    }

    void processBatch(const std::vector<LogEntry>& batch) {
        std::lock_guard lock(file_mutex_);

        if (!log_file_.is_open())
            return;

        std::osyncstream synced_stream(log_file_);
        for (const auto& entry : batch) {
            auto formatted =
                formatMessage(entry.level, entry.message, entry.location,
                              entry.timestamp, entry.thread_name);
            synced_stream << formatted << '\n';
        }

        checkRotation();
    }

    void writeToFile(const std::string& message) {
        std::lock_guard lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_ << message << '\n';
            log_file_.flush();
            checkRotation();
        }
    }

    void checkRotation() {
        if (max_file_size_ > 0) {
            auto pos = log_file_.tellp();
            if (pos != std::streampos(-1) &&
                pos >= static_cast<std::streampos>(max_file_size_)) {
                rotateLogFile();
            }
        }
    }

    void rotateLogFile() {
        if (log_file_.is_open()) {
            log_file_.close();
        }

        try {
            std::error_code ec;
            if (fs::exists(file_name_, ec) && !ec) {
                auto extension = file_name_.extension().string();
                auto stem = file_name_.stem().string();
                auto parent_path = file_name_.parent_path();

                for (int i = max_files_ - 1; i > 0; --i) {
                    auto src = parent_path /
                               (stem + "." + std::to_string(i) + extension);
                    auto dst = parent_path /
                               (stem + "." + std::to_string(i + 1) + extension);

                    if (fs::exists(src, ec) && !ec) {
                        fs::rename(src, dst, ec);
                    }
                }

                auto dst = parent_path / (stem + ".1" + extension);
                fs::rename(file_name_, dst, ec);
            }
        } catch (const fs::filesystem_error&) {
            // Handle error silently or log to stderr
        }

        openLogFile();
    }

    void openLogFile() {
        log_file_.open(file_name_, std::ios::out | std::ios::app);
    }

    void closeLogFile() {
        std::lock_guard lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    std::string getThreadName() {
        std::shared_lock lock(thread_mutex_);
        auto thread_id = std::this_thread::get_id();
        auto it = thread_names_.find(thread_id);
        if (it != thread_names_.end()) {
            return it->second;
        }

        std::ostringstream oss;
        oss << thread_id;
        return oss.str();
    }

    std::string formatMessage(LogLevel level, std::string_view msg,
                              const std::source_location& location,
                              std::string_view timestamp,
                              std::string_view thread_name) {
        std::shared_lock lock(config_mutex_);

        if (custom_formatter_) {
            return custom_formatter_(level, msg, location, timestamp,
                                     thread_name);
        }

        switch (current_format_) {
            case LogFormat::JSON:
                return formatAsJson(level, msg, location, timestamp,
                                    thread_name);
            case LogFormat::XML:
                return formatAsXml(level, msg, location, timestamp,
                                   thread_name);
            default:
                std::string result;
                try {
                    /*
                    result = std::vformat(
                        pattern_,
                        std::make_format_args(
                            std::string(timestamp),
                            std::string(logLevelToString(level)),
                            std::string(thread_name),
                            std::string(msg),
                            std::string(location.file_name()),
                            static_cast<unsigned int>(location.line())));
                    */
                } catch (const std::format_error&) {
                    result = std::string(timestamp) + " [" +
                             std::string(logLevelToString(level)) + "] " +
                             std::string(msg) + " (" + location.file_name() +
                             ":" + std::to_string(location.line()) + ")";
                }
                return result;
        }
    }

    std::string formatAsJson(LogLevel level, std::string_view msg,
                             const std::source_location& location,
                             std::string_view timestamp,
                             std::string_view thread_name) {
        std::ostringstream oss;
        oss << "{"
            << "\"timestamp\":\"" << timestamp << "\","
            << "\"level\":\"" << logLevelToString(level) << "\","
            << "\"thread\":\"" << thread_name << "\","
            << "\"message\":\"" << msg << "\","
            << "\"file\":\"" << location.file_name() << "\","
            << "\"line\":" << location.line() << ","
            << "\"function\":\"" << location.function_name() << "\""
            << "}";
        return oss.str();
    }

    std::string formatAsXml(LogLevel level, std::string_view msg,
                            const std::source_location& location,
                            std::string_view timestamp,
                            std::string_view thread_name) {
        std::ostringstream oss;
        oss << "<log>"
            << "<timestamp>" << timestamp << "</timestamp>"
            << "<level>" << logLevelToString(level) << "</level>"
            << "<thread>" << thread_name << "</thread>"
            << "<message>" << msg << "</message>"
            << "<file>" << location.file_name() << "</file>"
            << "<line>" << location.line() << "</line>"
            << "<function>" << location.function_name() << "</function>"
            << "</log>";
        return oss.str();
    }

    void addToMemoryLog(const std::string& message) {
        if (!memory_logging_enabled_.load(std::memory_order_relaxed))
            return;

        std::lock_guard lock(memory_mutex_);
        memory_logs_.push_back(message);
        if (memory_logs_.size() > max_memory_entries_) {
            memory_logs_.erase(memory_logs_.begin());
        }
    }

    void logToSystem(LogLevel level, std::string_view msg,
                     const std::source_location& location) {
#ifdef _WIN32
        if (h_event_log_) {
            WORD eventType = EVENTLOG_INFORMATION_TYPE;
            switch (level) {
                case LogLevel::CRITICAL:
                case LogLevel::ERROR:
                    eventType = EVENTLOG_ERROR_TYPE;
                    break;
                case LogLevel::WARN:
                    eventType = EVENTLOG_WARNING_TYPE;
                    break;
                default:
                    break;
            }

            auto fullMsg = std::format("{}:{} - {}", location.file_name(),
                                       location.line(), msg);
            int wide_len = MultiByteToWideChar(
                CP_UTF8, 0, fullMsg.data(), static_cast<int>(fullMsg.length()),
                nullptr, 0);
            if (wide_len > 0) {
                std::wstring wideMsg(wide_len, 0);
                MultiByteToWideChar(CP_UTF8, 0, fullMsg.data(),
                                    static_cast<int>(fullMsg.length()),
                                    &wideMsg[0], wide_len);
                LPCWSTR messages[] = {wideMsg.c_str()};
                ReportEventW(h_event_log_, eventType, 0, 0, nullptr, 1, 0,
                             messages, nullptr);
            }
        }
#elif defined(__linux__)
        int priority = LOG_INFO;
        switch (level) {
            case LogLevel::CRITICAL:
                priority = LOG_CRIT;
                break;
            case LogLevel::ERROR:
                priority = LOG_ERR;
                break;
            case LogLevel::WARN:
                priority = LOG_WARNING;
                break;
            case LogLevel::DEBUG:
                priority = LOG_DEBUG;
                break;
            default:
                break;
        }

        auto fullMsg = std::format("{}:{} - {}", location.file_name(),
                                   location.line(), msg);
        sd_journal_send("MESSAGE=%s", fullMsg.c_str(), "PRIORITY=%i", priority,
                        "CODE_FILE=%s", location.file_name(), "CODE_LINE=%d",
                        location.line(), "CODE_FUNC=%s",
                        location.function_name(), nullptr);
#elif defined(__APPLE__)
        if (os_log_handle_) {
            os_log_type_t type = OS_LOG_TYPE_INFO;
            switch (level) {
                case LogLevel::CRITICAL:
                    type = OS_LOG_TYPE_FAULT;
                    break;
                case LogLevel::ERROR:
                    type = OS_LOG_TYPE_ERROR;
                    break;
                case LogLevel::WARN:
                    type = OS_LOG_TYPE_DEFAULT;
                    break;
                case LogLevel::DEBUG:
                    type = OS_LOG_TYPE_DEBUG;
                    break;
                default:
                    break;
            }

            auto fullMsg = std::format("{}:{} - {}", location.file_name(),
                                       location.line(), msg);
            os_log_with_type(os_log_handle_, type, "%{public}s",
                             fullMsg.c_str());
        }
#elif defined(__ANDROID__)
        android_LogPriority priority = ANDROID_LOG_INFO;
        switch (level) {
            case LogLevel::CRITICAL:
                priority = ANDROID_LOG_FATAL;
                break;
            case LogLevel::ERROR:
                priority = ANDROID_LOG_ERROR;
                break;
            case LogLevel::WARN:
                priority = ANDROID_LOG_WARN;
                break;
            case LogLevel::DEBUG:
                priority = ANDROID_LOG_DEBUG;
                break;
            default:
                break;
        }

        auto fullMsg = std::format("{}:{} - {}", location.file_name(),
                                   location.line(), msg);
        __android_log_print(priority, "AtomLogger", "%s", fullMsg.c_str());
#endif
    }

    void dispatchToSinks(LogLevel level, std::string_view msg,
                         const std::source_location& location) {
        std::shared_lock lock(sinks_mutex_);
        for (const auto& sink : sinks_) {
            if (sink && sink->shouldLog(level)) {
                sink->log(level, std::string(msg), location);
            }
        }
    }
};

// Logger implementation
Logger::Logger(const fs::path& file_name, LogLevel min_level,
               size_t max_file_size, int max_files)
    : impl_(std::make_shared<LoggerImpl>(file_name, min_level, max_file_size,
                                         max_files)) {}

Logger::~Logger() = default;

void Logger::setLevel(LogLevel level) { impl_->setLevel(level); }
void Logger::setPattern(std::string_view pattern) {
    impl_->setPattern(pattern);
}
void Logger::setThreadName(std::string_view name) {
    impl_->setThreadName(name);
}
void Logger::setFormat(LogFormat format) { impl_->setFormat(format); }
void Logger::setCustomFormatter(LogFormatter formatter) {
    impl_->setCustomFormatter(std::move(formatter));
}
void Logger::addFilter(LogFilter filter) {
    impl_->addFilter(std::move(filter));
}
void Logger::clearFilters() { impl_->clearFilters(); }
void Logger::setBatchSize(size_t size) { impl_->setBatchSize(size); }
void Logger::setFlushInterval(std::chrono::milliseconds interval) {
    impl_->setFlushInterval(interval);
}
void Logger::setCompressionEnabled(bool enabled) {
    impl_->setCompressionEnabled(enabled);
}
void Logger::setEncryptionKey(std::string_view key) {
    impl_->setEncryptionKey(key);
}

void Logger::registerSink(std::shared_ptr<Logger> logger) {
    if (logger && logger.get() != this) {
        impl_->registerSink(logger->impl_);
    }
}

void Logger::removeSink(std::shared_ptr<Logger> logger) {
    if (logger) {
        impl_->removeSink(logger->impl_);
    }
}

void Logger::clearSinks() { impl_->clearSinks(); }
void Logger::flush() { impl_->flush(); }
void Logger::forceFlush() { impl_->forceFlush(); }
void Logger::enableSystemLogging(bool enable) {
    impl_->enableSystemLogging(enable);
}
void Logger::enableAsyncLogging(bool enable) {
    impl_->enableAsyncLogging(enable);
}
void Logger::enableColorOutput(bool enable) {
    impl_->enableColorOutput(enable);
}
void Logger::enableMemoryLogging(bool enable, size_t max_entries) {
    impl_->enableMemoryLogging(enable, max_entries);
}

void Logger::registerCustomLogLevel(std::string_view name, int severity) {
    impl_->registerCustomLogLevel(name, severity);
}

void Logger::logCustomLevel(std::string_view level_name, std::string_view msg,
                            const std::source_location& location) {
    impl_->logCustomLevel(level_name, msg, location);
}

bool Logger::shouldLog(LogLevel level) const noexcept {
    return impl_->shouldLog(level);
}
LogLevel Logger::getLevel() const noexcept { return impl_->getLevel(); }
size_t Logger::getQueueSize() const noexcept { return impl_->getQueueSize(); }
bool Logger::isEnabled() const noexcept { return impl_->isEnabled(); }
std::vector<std::string> Logger::getMemoryLogs() const {
    return impl_->getMemoryLogs();
}
std::string Logger::getStats() const { return impl_->getStats(); }

void Logger::rotate() { impl_->rotate(); }
void Logger::close() { impl_->close(); }
void Logger::reopen() { impl_->reopen(); }

void Logger::log(LogLevel level, std::string msg,
                 const std::source_location& location) {
    impl_->log(level, std::move(msg), location);
}

// Static methods
static std::shared_ptr<Logger> default_logger_;
static std::mutex default_logger_mutex_;

std::shared_ptr<Logger> Logger::getDefault() {
    std::lock_guard lock(default_logger_mutex_);
    if (!default_logger_) {
        default_logger_ = std::make_shared<Logger>("atom.log");
    }
    return default_logger_;
}

void Logger::setDefault(std::shared_ptr<Logger> logger) {
    std::lock_guard lock(default_logger_mutex_);
    default_logger_ = std::move(logger);
}

std::shared_ptr<Logger> Logger::create(const fs::path& file_name) {
    return std::make_shared<Logger>(file_name);
}

}  // namespace atom::log