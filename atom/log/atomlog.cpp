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
#include <condition_variable>
#include <format>  // 用于std::format和std::vformat
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <stop_token>  // 用于std::stop_token
#include <syncstream>  // 添加std::osyncstream支持
#include <thread>
#include <unordered_map>
#include <utility>  // 用于std::move

// 平台特定头文件
#ifdef _WIN32
#include <windows.h>
#undef ERROR
#elif defined(__linux__)
#include <syslog.h>
#include <systemd/sd-journal.h>  // 添加systemd journal支持
#include <sstream>               // getThreadName回退需要
#elif defined(__APPLE__)
#include <os/log.h>  // 添加macOS os_log支持
#include <syslog.h>
#include <sstream>  // getThreadName回退需要
#elif defined(__ANDROID__)
#include <android/log.h>
#include <sstream>  // getThreadName回退需要
#else
#include <sstream>  // getThreadName回退需要
#endif

#include "atom/utils/time.hpp"

namespace atom::log {

// 定义一个帮助函数，用于从std::source_location创建位置字符串
[[nodiscard]] constexpr std::string formatSourceLocation(
    const std::source_location& location) {
    return std::format("{}:{}:{}", location.file_name(), location.line(),
                       location.function_name());
}

class Logger::LoggerImpl : public std::enable_shared_from_this<LoggerImpl> {
public:
    LoggerImpl(fs::path file_name_, LogLevel min_level, size_t max_file_size,
               int max_files)
        : file_name_(std::move(file_name_)),
          max_file_size_(max_file_size),
          max_files_(max_files),
          min_level_(min_level),
          system_logging_enabled_(false),
          batch_size_(64) {  // 添加批处理大小参数
        rotateLogFile();
        // 使用std::jthread和C++20 stop_token
        worker_ = std::jthread([this](std::stop_token st) { this->run(st); });
#ifdef __APPLE__
        // 初始化macOS os_log
        os_log_handle_ = os_log_create("com.lightapt.atomlogger", "main");
#endif
    }

    ~LoggerImpl() {
        // 请求停止并等待worker线程完成队列处理
        worker_.request_stop();
        {
            std::lock_guard lock(queue_mutex_);
            finished_ = true;  // 标记不再添加项目
        }
        cv_.notify_one();  // 如果正在等待则唤醒worker

        // jthread析构函数自动join

        if (log_file_.is_open()) {
            log_file_.close();
        }

#if defined(__linux__)
        if (system_logging_enabled_) {
            closelog();
        }
#elif defined(__APPLE__)
        if (system_logging_enabled_) {
            closelog();
            // os_log不需要显式关闭
        }
#elif defined(_WIN32)  // 确保Windows上处理h_event_log_
        if (h_event_log_) {
            DeregisterEventSource(h_event_log_);
            h_event_log_ = nullptr;
        }
#endif
    }

    // 防止复制
    LoggerImpl(const LoggerImpl&) = delete;
    LoggerImpl& operator=(const LoggerImpl&) = delete;

    // 手动定义移动构造函数
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
          batch_size_(other.batch_size_),
#elif defined(__APPLE__)
          os_log_handle_(other.os_log_handle_),
#endif
          h_event_log_(other.h_event_log_),
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

    // 手动定义移动赋值运算符
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
#elif defined(__APPLE__)
        os_log_handle_ = other.os_log_handle_;
#endif
        custom_levels_ = std::move(other.custom_levels_);
        batch_size_ = other.batch_size_;

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

    // 设置批处理大小
    void setBatchSize(size_t size) {
        std::lock_guard lock(queue_mutex_);
        batch_size_ = size;
    }

    void registerSink(const std::shared_ptr<LoggerImpl>& logger) {
        if (!logger || logger.get() == this) {  // 添加空检查
            // 防止注册空指针或自身
            return;
        }
        std::lock_guard lock(sinks_mutex_);
        // 避免重复
        if (std::find(sinks_.begin(), sinks_.end(), logger) == sinks_.end()) {
            sinks_.emplace_back(logger);
        }
    }

    void removeSink(const std::shared_ptr<LoggerImpl>& logger) {
        if (!logger)
            return;  // 添加空检查
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
            return;  // 避免重复调用

        system_logging_enabled_ = enable;

#ifdef _WIN32
        if (system_logging_enabled_) {
            if (!h_event_log_) {  // 检查是否已注册
                h_event_log_ = RegisterEventSourceW(nullptr, L"AtomLogger");
                if (!h_event_log_) {
                    // 日志错误或抛出？现在只是再次禁用系统日志记录。
                    system_logging_enabled_ = false;
                    // 考虑将此失败记录到文件日志记录器本身
                    log(LogLevel::ERROR,
                        "Failed to register Windows Event Source.");
                }
            }
        } else if (h_event_log_) {
            DeregisterEventSource(h_event_log_);
            h_event_log_ = nullptr;
        }
#elif defined(__linux__)
        if (system_logging_enabled_) {
            // openlog可以多次调用，后续调用会更改标识
            openlog("AtomLogger", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        }
#elif defined(__APPLE__)
        if (system_logging_enabled_) {
            // macOS既使用传统syslog也使用新的os_log
            openlog("AtomLogger", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
            // os_log_handle_已在构造函数中初始化
        }
#endif
    }

    void registerCustomLogLevel(const std::string& name, int severity) {
        std::lock_guard lock(custom_level_mutex_);
        custom_levels_[name] = severity;
    }

    // 使用std::string_view提高效率
    void log(LogLevel level, std::string_view msg,
             const std::source_location& location =
                 std::source_location::current()) {
        // 早期检查日志级别
        std::shared_lock levelLock(level_mutex_);
        if (static_cast<int>(level) < static_cast<int>(min_level_)) {
            levelLock.unlock();  // 如果不记录则提前解锁
            return;
        }
        levelLock.unlock();  // 检查后解锁

        // 在队列锁外格式化消息
        auto formattedMsg = formatMessage(level, msg, location);

        {
            std::lock_guard lock(queue_mutex_);
            if (finished_)
                return;  // 如果记录器正在关闭，则不入队

            log_queue_.push(std::move(formattedMsg));  // 移动格式化消息

            // 如果队列大小超过批处理阈值，唤醒worker线程进行处理
            if (log_queue_.size() >= batch_size_) {
                cv_.notify_one();
            }
        }

        // 非批处理大小触发时也要通知，确保低延迟日志
        if (batch_size_ > 1) {
            cv_.notify_one();
        }

        // 在队列锁外检查系统日志启用
        std::shared_lock sysLogLock(system_log_mutex_);
        bool sysLogEnabled = system_logging_enabled_;
        sysLogLock.unlock();

        if (sysLogEnabled) {
            logToSystem(level, formattedMsg, location);  // 传递格式化消息
        }

        // 在队列锁外调度到接收器
        dispatchToSinks(level, msg, location);  // 传递原始消息
    }

    void flush() {
        std::lock_guard lock(queue_mutex_);
        if (log_file_.is_open()) {
            log_file_.flush();
        }
    }

private:
    fs::path file_name_;
    std::ofstream log_file_;
    std::queue<std::string> log_queue_;
    std::mutex queue_mutex_;  // 保护log_queue_和finished_标志
    std::condition_variable_any
        cv_;                 // 使用condition_variable_any支持stop_token
    bool finished_ = false;  // 受queue_mutex_保护
    std::jthread worker_;
    size_t max_file_size_;
    int max_files_;
    LogLevel min_level_;  // 受level_mutex_保护
    std::unordered_map<std::thread::id, std::string>
        thread_names_;  // 受thread_mutex_保护
    std::string pattern_ =
        "[{}][{}][{}] {} {}";  // 修改默认模式包含位置信息，受pattern_mutex_保护
    std::vector<std::shared_ptr<LoggerImpl>> sinks_;  // 受sinks_mutex_保护
    bool system_logging_enabled_ = false;             // 受system_log_mutex_保护
    size_t batch_size_;  // 批处理大小，受queue_mutex_保护

#ifdef _WIN32
    HANDLE h_event_log_ =
        nullptr;  // 在启用/禁用期间由system_log_mutex_间接保护
#elif defined(__APPLE__)
    os_log_t os_log_handle_ = nullptr;  // macOS os_log句柄
#endif

    // 互斥锁，用于线程安全访问成员
    mutable std::shared_mutex thread_mutex_;
    mutable std::shared_mutex pattern_mutex_;
    mutable std::shared_mutex sinks_mutex_;
    mutable std::shared_mutex system_log_mutex_;
    mutable std::shared_mutex level_mutex_;
    mutable std::shared_mutex custom_level_mutex_;
    std::unordered_map<std::string, int>
        custom_levels_;  // 受custom_level_mutex_保护

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
        // 使用共享锁读取thread_names_
        std::shared_lock lock(thread_mutex_);
        auto thread_id = std::this_thread::get_id();
        auto it = thread_names_.find(thread_id);
        if (it != thread_names_.end()) {
            return it->second;
        }
        lock.unlock();  // 格式化/散列前解锁

        // 如果未找到名称，则格式化线程ID
        std::ostringstream oss;
        oss << thread_id;
        return oss.str();
    }

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

    // 添加source_location支持
    auto formatMessage(LogLevel level, std::string_view msg,
                       const std::source_location& location) -> std::string {
        auto currentTime =
            utils::getChinaTimestampString();     // 假设这返回std::string
        auto threadName = getThreadName();        // 假设这返回std::string
        auto levelStr = logLevelToString(level);  // 返回std::string_view

        // 格式化源位置信息
        std::string locationInfo =
            std::format("{}:{}:{}", location.file_name(), location.line(),
                        location.function_name());

        // 使用共享锁读取pattern_
        std::shared_lock patternLock(pattern_mutex_);

        // 使用string_view和format_to减少分配和复制
        std::string result;
        // 预分配足够的空间，避免重新分配
        result.reserve(currentTime.size() + levelStr.size() +
                       threadName.size() + msg.size() + locationInfo.size() +
                       20);

        // 使用vformat将参数传递给运行时format字符串
        return std::vformat(
            pattern_, std::make_format_args(currentTime, levelStr, threadName,
                                            msg, locationInfo));
    }

    void run(std::stop_token stop_token) {
        std::vector<std::string> batch;  // 用于批量处理的消息缓冲区
        batch.reserve(batch_size_);      // 预分配空间

        while (true) {
            {
                std::unique_lock lock(queue_mutex_);
                // 使用condition_variable_any和stop_token等待
                if (!cv_.wait(lock, stop_token, [this] {
                        return !log_queue_.empty() || finished_;
                    })) {
                    // 等待被stop_token取消
                    // 如果有的话，处理剩余消息，然后退出
                    if (log_queue_.empty())
                        break;
                }

                // 检查完成条件（可能在stop_token之后设置）
                if (finished_ && log_queue_.empty()) {
                    break;  // 如果完成且队列为空则退出
                }

                // 批量处理：将消息从队列移动到批处理缓冲区
                while (!log_queue_.empty() && batch.size() < batch_size_) {
                    batch.push_back(std::move(log_queue_.front()));
                    log_queue_.pop();
                }

                // 队列仍有消息但达到批处理大小，通知其他线程可能正在等待
                if (!log_queue_.empty()) {
                    cv_.notify_one();
                }
            }  // 在此处解锁互斥锁

            // 在锁外进行文件I/O
            if (log_file_.is_open() && !batch.empty()) {
                // 使用osyncstream进行线程安全的文件写入
                {
                    // 使用同步流，无需手动缓冲区同步
                    std::osyncstream synced_stream(log_file_);
                    for (const auto& msg : batch) {
                        synced_stream << msg << '\n';
                    }
                    // 析构函数自动刷新和同步
                }

                // 刷新主流以确保写入磁盘
                log_file_.flush();

                // 检查文件大小以进行轮替
                try {
                    if (max_file_size_ > 0) {
                        std::streampos current_pos = log_file_.tellp();
                        if (current_pos != static_cast<std::streampos>(-1) &&
                            current_pos >=
                                static_cast<std::streampos>(max_file_size_)) {
                            rotateLogFile();
                        }
                    }
                } catch (const std::ios_base::failure& e) {
                    fprintf(stderr, "Error checking log file size: %s\n",
                            e.what());
                }

                // 清空批处理缓冲区，但保留容量
                batch.clear();
            }

            // 再次检查stop token，如果请求停止并且队列为空则退出循环
            if (stop_token.stop_requested()) {
                std::lock_guard lock(queue_mutex_);
                if (log_queue_.empty())
                    break;
            }
        }
    }

    // 更新系统日志功能以支持更多平台特性
    void logToSystem(LogLevel level, std::string_view msg,
                     const std::source_location& location) const {
#ifdef _WIN32
        if (h_event_log_) {
            using enum LogLevel;
            WORD eventType;
            switch (level) {
                case CRITICAL:
                    eventType = EVENTLOG_ERROR_TYPE;
                    break;
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
                    break;
                case TRACE:
                    eventType = EVENTLOG_INFORMATION_TYPE;
                    break;
                default:
                    eventType = EVENTLOG_INFORMATION_TYPE;
                    break;
            }

            // 将带位置信息的消息转为Windows需要的宽字符串
            std::string fullMsg = std::format(
                "{}:{} - {}", location.file_name(), location.line(), msg);

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
            } else {
                fprintf(stderr,
                        "Failed to convert log message to wide string for "
                        "Windows Event Log.\n");
            }
        }
#elif defined(__linux__)
        // system_logging_enabled_检查应在调用此函数之前发生
        using enum LogLevel;

        // 优先使用systemd journal（如果可用）
        if (sd_journal_is_running() > 0) {
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
                    break;
                default:
                    priority = LOG_NOTICE;
                    break;
            }

            // 使用结构化日志记录，包括位置信息作为元数据
            sd_journal_send(
                "MESSAGE=%s", std::string(msg).c_str(), "PRIORITY=%i", priority,
                "CODE_FILE=%s", location.file_name(), "CODE_LINE=%d",
                location.line(), "CODE_FUNC=%s", location.function_name(),
                "SYSLOG_IDENTIFIER=AtomLogger", NULL);
        }
        // 回退到传统syslog
        else {
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
                    break;
                default:
                    priority = LOG_NOTICE;
                    break;
            }
            // 将位置信息添加到消息中
            std::string fullMsg = std::format(
                "{}:{} - {}", location.file_name(), location.line(), msg);
            syslog(priority, "%s", fullMsg.c_str());
        }
#elif defined(__APPLE__)
        // 使用macOS的os_log API
        if (os_log_handle_) {
            using enum LogLevel;
            os_log_type_t type;
            switch (level) {
                case CRITICAL:
                    type = OS_LOG_TYPE_FAULT;
                    break;
                case ERROR:
                    type = OS_LOG_TYPE_ERROR;
                    break;
                case WARN:
                    type = OS_LOG_TYPE_DEFAULT;
                    break;
                case INFO:
                    type = OS_LOG_TYPE_INFO;
                    break;
                case DEBUG:
                    type = OS_LOG_TYPE_DEBUG;
                    break;
                case TRACE:
                    type = OS_LOG_TYPE_DEBUG;
                    break;
                default:
                    type = OS_LOG_TYPE_DEFAULT;
                    break;
            }

            // 格式化包含位置信息的消息
            std::string fullMsg = std::format(
                "{}:{} - {}", location.file_name(), location.line(), msg);

            os_log_with_type(os_log_handle_, type, "%{public}s",
                             fullMsg.c_str());
        }
        // 回退到传统syslog
        else {
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
                    break;
                default:
                    priority = LOG_NOTICE;
                    break;
            }
            // 将位置信息添加到消息中
            std::string fullMsg = std::format(
                "{}:{} - {}", location.file_name(), location.line(), msg);
            syslog(priority, "%s", fullMsg.c_str());
        }
#elif defined(__ANDROID__)
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
                break;
            default:
                priority = ANDROID_LOG_DEFAULT;
                break;
        }

        // 格式化包含位置信息的消息
        std::string fullMsg = std::format("{}:{} - {}", location.file_name(),
                                          location.line(), msg);

        __android_log_print(priority, "AtomLogger", "%s", fullMsg.c_str());
#else
        // 抑制未使用变量警告
        (void)level;
        (void)msg;
        (void)location;
#endif
    }

    // 更新以支持source_location
    void dispatchToSinks(LogLevel level, std::string_view msg,
                         const std::source_location& location) {
        // 使用共享锁读取sinks_
        std::shared_lock lock(sinks_mutex_);
        if (sinks_.empty()) {
            return;  // 如果没有接收器则避免迭代
        }
        // 创建接收器指针的副本，以避免持有锁
        // 同时调用可能长时间运行的sink->log方法。
        auto sinks_copy = sinks_;
        lock.unlock();

        // 在循环前将string_view转换为std::string，因为sink->log期望std::string
        std::string msg_str(msg);

        for (const auto& sink : sinks_copy) {
            if (sink) {  // 检查接收器指针是否有效
                // 传递原始消息字符串和位置信息
                sink->log(level, msg_str, location);
            }
        }
    }
};

// `Logger` 类方法实现

Logger::Logger(const fs::path& file_name, LogLevel min_level,
               size_t max_file_size, int max_files)
    : impl_(std::make_shared<LoggerImpl>(file_name, min_level, max_file_size,
                                         max_files)) {}

// 定义析构函数体（即使为空），因为LoggerImpl是前向声明
Logger::~Logger() = default;

// 一致使用String
void Logger::setThreadName(const String& name) {
    // 直接传递String，LoggerImpl期望std::string
    impl_->setThreadName(name);
}

void Logger::setLevel(LogLevel level) { impl_->setLevel(level); }

// 一致使用String
void Logger::setPattern(const String& pattern) {
    // 直接传递String，LoggerImpl期望std::string
    impl_->setPattern(pattern);
}

void Logger::registerSink(const std::shared_ptr<Logger>& logger) {
    if (logger && logger.get() != this) {  // 防止自我接收
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

// 一致使用String
void Logger::registerCustomLogLevel(const String& name, int severity) {
    // 直接传递String，LoggerImpl期望std::string
    impl_->registerCustomLogLevel(name, severity);
}

// 添加source_location支持
void Logger::log(LogLevel level, const std::string& msg,
                 const std::source_location& location) {
    // 为效率传递为string_view
    impl_->log(level, std::string_view(msg), location);
}

void Logger::flush() {
    impl_->flush();
}

}  // namespace atom::log