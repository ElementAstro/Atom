#ifndef ATOM_EXTRA_BOOST_SYSTEM_HPP
#define ATOM_EXTRA_BOOST_SYSTEM_HPP

#if __has_include(<boost/system/error_category.hpp>)
#include <boost/system/error_category.hpp>
#endif
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <unistd.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace atom::extra::boost {

/**
 * @brief Enhanced logging levels
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

/**
 * @brief System resource information
 */
struct SystemInfo {
    double cpu_usage_percent = 0.0;
    size_t memory_used_bytes = 0;
    size_t memory_total_bytes = 0;
    size_t disk_used_bytes = 0;
    size_t disk_total_bytes = 0;
    std::chrono::steady_clock::time_point timestamp;
    std::string hostname;
    std::string os_version;
    size_t process_count = 0;
    double load_average = 0.0;
};

/**
 * @brief Error context for enhanced error reporting
 */
struct ErrorContext {
    std::string function_name;
    std::string file_name;
    int line_number = 0;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<std::string> stack_trace;
};

/**
 * @brief Enhanced structured logger
 */
class StructuredLogger {
private:
    static std::mutex log_mutex_;
    static std::ofstream log_file_;
    static LogLevel min_level_;
    static std::atomic<uint64_t> log_counter_;
    static std::queue<std::string> log_queue_;
    static std::condition_variable log_cv_;
    static std::thread log_thread_;
    static std::atomic<bool> shutdown_;

public:
    /**
     * @brief Initialize the logger
     * @param filename Log file name
     * @param level Minimum log level
     */
    static void initialize(const std::string& filename,
                           LogLevel level = LogLevel::INFO) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        min_level_ = level;
        log_file_.open(filename, std::ios::app);
        shutdown_.store(false);

        // Start background logging thread
        log_thread_ = std::thread([]() {
            while (!shutdown_.load()) {
                std::unique_lock<std::mutex> lock(log_mutex_);
                log_cv_.wait(lock, []() {
                    return !log_queue_.empty() || shutdown_.load();
                });

                while (!log_queue_.empty()) {
                    if (log_file_.is_open()) {
                        log_file_ << log_queue_.front() << std::endl;
                        log_file_.flush();
                    }
                    log_queue_.pop();
                }
            }
        });
    }

    /**
     * @brief Log a message with context
     * @param level Log level
     * @param message Log message
     * @param context Error context
     */
    static void log(LogLevel level, const std::string& message,
                    const ErrorContext& context = {}) {
        if (level < min_level_)
            return;

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::ostringstream oss;
        oss << "["
            << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << "] "
            << "[" << logLevelToString(level) << "] "
            << "[" << log_counter_.fetch_add(1) << "] ";

        if (!context.function_name.empty()) {
            oss << "[" << context.function_name << "] ";
        }

        oss << message;

        if (!context.metadata.empty()) {
            oss << " {";
            bool first = true;
            for (const auto& [key, value] : context.metadata) {
                if (!first)
                    oss << ", ";
                oss << key << "=" << value;
                first = false;
            }
            oss << "}";
        }

        std::lock_guard<std::mutex> lock(log_mutex_);
        log_queue_.push(oss.str());
        log_cv_.notify_one();
    }

    /**
     * @brief Shutdown the logger
     */
    static void shutdown() {
        shutdown_.store(true);
        log_cv_.notify_all();
        if (log_thread_.joinable()) {
            log_thread_.join();
        }
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

private:
    static std::string logLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE:
                return "TRACE";
            case LogLevel::DEBUG:
                return "DEBUG";
            case LogLevel::INFO:
                return "INFO";
            case LogLevel::WARN:
                return "WARN";
            case LogLevel::ERROR:
                return "ERROR";
            case LogLevel::FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }
};

/**
 * @brief System monitor for resource tracking
 */
class SystemMonitor {
private:
    static std::atomic<bool> monitoring_;
    static std::thread monitor_thread_;
    static std::vector<SystemInfo> history_;
    static std::mutex history_mutex_;
    static std::chrono::seconds update_interval_;

public:
    /**
     * @brief Start system monitoring
     * @param interval Update interval in seconds
     */
    static void startMonitoring(
        std::chrono::seconds interval = std::chrono::seconds(5)) {
        update_interval_ = interval;
        monitoring_.store(true);

        monitor_thread_ = std::thread([]() {
            while (monitoring_.load()) {
                auto info = getCurrentSystemInfo();

                {
                    std::lock_guard<std::mutex> lock(history_mutex_);
                    history_.push_back(info);

                    // Keep only last 1000 entries
                    if (history_.size() > 1000) {
                        history_.erase(history_.begin());
                    }
                }

                std::this_thread::sleep_for(update_interval_);
            }
        });
    }

    /**
     * @brief Stop system monitoring
     */
    static void stopMonitoring() {
        monitoring_.store(false);
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }

    /**
     * @brief Get current system information
     * @return Current system info
     */
    static SystemInfo getCurrentSystemInfo() {
        SystemInfo info;
        info.timestamp = std::chrono::steady_clock::now();

        // Get hostname
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            info.hostname = hostname;
        }

        // Get memory info (Linux-specific)
        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open()) {
            std::string line;
            while (std::getline(meminfo, line)) {
                if (line.starts_with("MemTotal:")) {
                    info.memory_total_bytes = parseMemoryValue(line) * 1024;
                } else if (line.starts_with("MemAvailable:")) {
                    size_t available = parseMemoryValue(line) * 1024;
                    info.memory_used_bytes =
                        info.memory_total_bytes - available;
                }
            }
        }

        // Get CPU usage (simplified)
        info.cpu_usage_percent = getCpuUsage();

        // Get disk usage
        try {
            auto space = std::filesystem::space("/");
            info.disk_total_bytes = space.capacity;
            info.disk_used_bytes = space.capacity - space.available;
        } catch (...) {
            // Ignore filesystem errors
        }

        return info;
    }

    /**
     * @brief Get system monitoring history
     * @return Vector of historical system info
     */
    static std::vector<SystemInfo> getHistory() {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return history_;
    }

private:
    static size_t parseMemoryValue(const std::string& line) {
        std::istringstream iss(line);
        std::string label;
        size_t value;
        iss >> label >> value;
        return value;
    }

    static double getCpuUsage() {
        // Simplified CPU usage calculation
        static auto last_time = std::chrono::steady_clock::now();
        static double last_usage = 0.0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_time);

        if (elapsed.count() >= 1) {
            // In a real implementation, this would read /proc/stat
            // For demo purposes, return a simulated value
            last_usage = (last_usage + (rand() % 20 - 10)) / 2.0;
            last_usage = std::max(0.0, std::min(100.0, last_usage));
            last_time = now;
        }

        return last_usage;
    }
};

/**
 * @brief Enhanced wrapper class for Boost.System error codes with logging and
 * context
 */
class Error {
public:
    Error() noexcept = default;

    /**
     * @brief Constructs an Error from a Boost.System error code
     * @param error_code The Boost.System error code
     */
    explicit constexpr Error(
        const ::boost::system::error_code& error_code) noexcept
        : m_ec_(error_code) {
        if (m_ec_) {
            logError();
        }
    }

    /**
     * @brief Constructs an Error from an error value and category
     * @param error_value The error value
     * @param error_category The error category
     */
    constexpr Error(
        int error_value,
        const ::boost::system::error_category& error_category) noexcept
        : m_ec_(error_value, error_category) {
        if (m_ec_) {
            logError();
        }
    }

    /**
     * @brief Constructs an Error with context
     * @param error_code The Boost.System error code
     * @param context Error context
     */
    Error(const ::boost::system::error_code& error_code,
          const ErrorContext& context) noexcept
        : m_ec_(error_code), context_(context) {
        if (m_ec_) {
            logErrorWithContext();
        }
    }

    /**
     * @brief Gets the error value
     * @return The error value
     */
    [[nodiscard]] constexpr int value() const noexcept { return m_ec_.value(); }

    /**
     * @brief Gets the error category
     * @return The error category
     */
    [[nodiscard]] constexpr const ::boost::system::error_category& category()
        const noexcept {
        return m_ec_.category();
    }

    /**
     * @brief Gets the error message
     * @return The error message
     */
    [[nodiscard]] std::string message() const { return m_ec_.message(); }

    /**
     * @brief Gets the error context
     * @return The error context
     */
    [[nodiscard]] const ErrorContext& context() const noexcept {
        return context_;
    }

    /**
     * @brief Sets the error context
     * @param context The error context
     */
    void setContext(const ErrorContext& context) noexcept {
        context_ = context;
    }

    /**
     * @brief Checks if the error code is valid
     * @return True if the error code is valid
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(m_ec_);
    }

    /**
     * @brief Converts to a Boost.System error code
     * @return The Boost.System error code
     */
    [[nodiscard]] constexpr ::boost::system::error_code toBoostErrorCode()
        const noexcept {
        return m_ec_;
    }

    /**
     * @brief Gets detailed error information including context
     * @return Detailed error string
     */
    [[nodiscard]] std::string detailedMessage() const {
        std::ostringstream oss;
        oss << "Error " << m_ec_.value() << ": " << m_ec_.message();

        if (!context_.function_name.empty()) {
            oss << " in " << context_.function_name;
        }

        if (!context_.file_name.empty()) {
            oss << " at " << context_.file_name << ":" << context_.line_number;
        }

        if (!context_.metadata.empty()) {
            oss << " [";
            bool first = true;
            for (const auto& [key, value] : context_.metadata) {
                if (!first)
                    oss << ", ";
                oss << key << "=" << value;
                first = false;
            }
            oss << "]";
        }

        return oss.str();
    }

    /**
     * @brief Equality operator
     * @param other The other Error to compare
     * @return True if the errors are equal
     */
    [[nodiscard]] constexpr bool operator==(const Error& other) const noexcept {
        return m_ec_ == other.m_ec_;
    }

    /**
     * @brief Inequality operator
     * @param other The other Error to compare
     * @return True if the errors are not equal
     */
    [[nodiscard]] constexpr bool operator!=(const Error& other) const noexcept {
        return !(*this == other);
    }

private:
    ::boost::system::error_code m_ec_;
    ErrorContext context_;

    /**
     * @brief Log error without context
     */
    void logError() const noexcept {
        try {
            ErrorContext ctx;
            ctx.timestamp = std::chrono::steady_clock::now();
            StructuredLogger::log(LogLevel::ERROR,
                                  "System error: " + m_ec_.message(), ctx);
        } catch (...) {
            // Ignore logging errors
        }
    }

    /**
     * @brief Log error with context
     */
    void logErrorWithContext() const noexcept {
        try {
            StructuredLogger::log(LogLevel::ERROR,
                                  "System error: " + m_ec_.message(), context_);
        } catch (...) {
            // Ignore logging errors
        }
    }
};

/**
 * @brief A custom exception class for handling errors
 */
class Exception : public std::system_error {
public:
    /**
     * @brief Constructs an Exception from an Error
     * @param error The Error object
     */
    explicit Exception(const Error& error)
        : std::system_error(error.value(), error.category(), error.message()) {}

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] Error error() const noexcept {
        return Error(::boost::system::error_code(
            code().value(), ::boost::system::generic_category()));
    }
};

/**
 * @brief A class template for handling results with potential errors
 * @tparam T The type of the result value
 */
template <typename T>
class Result {
public:
    using value_type = T;

    /**
     * @brief Constructs a Result with a value
     * @param value The result value
     */
    explicit Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : m_value_(std::move(value)) {}

    /**
     * @brief Constructs a Result with an Error
     * @param error The Error object
     */
    explicit constexpr Result(Error error) noexcept : m_error_(error) {}

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] constexpr bool hasValue() const noexcept { return !m_error_; }

    /**
     * @brief Gets the result value
     * @return The result value
     * @throws Exception if there is an error
     */
    [[nodiscard]] const T& value() const& {
        if ((!hasValue())) [[unlikely]] {
            throw Exception(m_error_);
        }
        return *m_value_;
    }

    /**
     * @brief Gets the result value
     * @return The result value
     * @throws Exception if there is an error
     */
    [[nodiscard]] T&& value() && {
        if ((!hasValue())) [[unlikely]] {
            throw Exception(m_error_);
        }
        return std::move(*m_value_);
    }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr const Error& error() const& noexcept {
        return m_error_;
    }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr Error error() && noexcept { return m_error_; }

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return hasValue();
    }

    /**
     * @brief Gets the result value or a default value
     * @tparam U The type of the default value
     * @param default_value The default value
     * @return The result value or the default value
     */
    template <typename U>
    [[nodiscard]] T valueOr(U&& default_value) const& {
        return (hasValue()) ? value()
                            : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Applies a function to the result value if it exists
     * @tparam F The type of the function
     * @param func The function to apply
     * @return A new Result with the function applied
     */
    template <typename F>
    [[nodiscard]] auto map(F&& func) const
        -> Result<std::invoke_result_t<F, T>> {
        if ((hasValue())) [[likely]] {
            return Result<std::invoke_result_t<F, T>>(func(*m_value_));
        }
        return Result<std::invoke_result_t<F, T>>(Error(m_error_));
    }

    /**
     * @brief Applies a function to the result value if it exists
     * @tparam F The type of the function
     * @param func The function to apply
     * @return The result of the function
     */
    template <typename F>
    [[nodiscard]] auto andThen(F&& func) const -> std::invoke_result_t<F, T> {
        if ((hasValue())) [[likely]] {
            return func(*m_value_);
        }
        return std::invoke_result_t<F, T>(Error(m_error_));
    }

private:
    std::optional<T> m_value_;
    Error m_error_;
};

/**
 * @brief Specialization of the Result class for void type
 */
template <>
class Result<void> {
public:
    Result() noexcept = default;

    /**
     * @brief Constructs a Result with an Error
     * @param error The Error object
     */
    explicit constexpr Result(Error error) noexcept : m_error_(error) {}

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] constexpr bool hasValue() const noexcept { return !m_error_; }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr const Error& error() const& noexcept {
        return m_error_;
    }

    /**
     * @brief Gets the associated Error
     * @return The associated Error
     */
    [[nodiscard]] constexpr Error error() && noexcept { return m_error_; }

    /**
     * @brief Checks if the Result has a value
     * @return True if the Result has a value
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return hasValue();
    }

private:
    Error m_error_;
};

/**
 * @brief Creates a Result from a function
 * @tparam F The type of the function
 * @param func The function to execute
 * @return A Result with the function's return value or an Error
 */
template <typename F>
[[nodiscard]] auto makeResult(F&& func) -> Result<std::invoke_result_t<F>> {
    using return_type = std::invoke_result_t<F>;
    try {
        if constexpr (std::is_void_v<return_type>) {
            func();
            return Result<void>();
        } else {
            return Result<return_type>(func());
        }
    } catch (const Exception& e) {
        return Result<return_type>(e.error());
    } catch (const std::exception&) {
        return Result<return_type>(
            Error(::boost::system::errc::invalid_argument,
                  ::boost::system::generic_category()));
    }
}

// Static member definitions
inline std::mutex StructuredLogger::log_mutex_{};
inline std::ofstream StructuredLogger::log_file_{};
inline LogLevel StructuredLogger::min_level_{LogLevel::INFO};
inline std::atomic<uint64_t> StructuredLogger::log_counter_{0};
inline std::queue<std::string> StructuredLogger::log_queue_{};
inline std::condition_variable StructuredLogger::log_cv_{};
inline std::thread StructuredLogger::log_thread_{};
inline std::atomic<bool> StructuredLogger::shutdown_{false};

inline std::atomic<bool> SystemMonitor::monitoring_{false};
inline std::thread SystemMonitor::monitor_thread_{};
inline std::vector<SystemInfo> SystemMonitor::history_{};
inline std::mutex SystemMonitor::history_mutex_{};
inline std::chrono::seconds SystemMonitor::update_interval_{5};

/**
 * @brief Convenience macros for error context creation
 */
#define MAKE_ERROR_CONTEXT()                                               \
    ErrorContext {                                                         \
        __FUNCTION__, __FILE__, __LINE__, std::chrono::steady_clock::now() \
    }

#define MAKE_ERROR_WITH_CONTEXT(ec) Error(ec, MAKE_ERROR_CONTEXT())

#define LOG_ERROR(msg) \
    StructuredLogger::log(LogLevel::ERROR, msg, MAKE_ERROR_CONTEXT())

#define LOG_INFO(msg) \
    StructuredLogger::log(LogLevel::INFO, msg, MAKE_ERROR_CONTEXT())

#define LOG_WARN(msg) \
    StructuredLogger::log(LogLevel::WARN, msg, MAKE_ERROR_CONTEXT())

}  // namespace atom::extra::boost

#endif  // ATOM_EXTRA_BOOST_SYSTEM_HPP
