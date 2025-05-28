#ifndef ATOM_UTILS_PRINT_HPP
#define ATOM_UTILS_PRINT_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <deque>
#include <exception>
#include <format>
#include <forward_list>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "atom/utils/time.hpp"

namespace atom::utils {

/**
 * @brief Concept to check if a type is printable to an output stream
 * @tparam T Type to check
 */
template <typename T>
concept Printable = requires(std::ostream& os, T value) {
    { os << value } -> std::convertible_to<std::ostream&>;
};

/**
 * @brief Concept to check if a type is a container
 * @tparam T Type to check
 */
template <typename T>
concept Container = requires(T a) {
    typename T::value_type;
    typename T::iterator;
    { a.begin() } -> std::input_or_output_iterator;
    { a.end() } -> std::input_or_output_iterator;
    { a.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Log levels for structured logging
 */
enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

/**
 * @brief Progress bar display styles
 */
enum class ProgressBarStyle {
    BASIC,      // [=====>     ]
    BLOCK,      // [█████▓     ]
    ARROW,      // [→→→→→→     ]
    PERCENTAGE  // 50%
};

/**
 * @brief Text styling options for console output
 */
enum class TextStyle {
    BOLD = 1,
    UNDERLINE = 4,
    BLINK = 5,
    REVERSE = 7,
    CONCEALED = 8
};

/**
 * @brief Color options for console output
 */
enum class Color {
    RED = 31,
    GREEN = 32,
    YELLOW = 33,
    BLUE = 34,
    MAGENTA = 35,
    CYAN = 36,
    WHITE = 37
};

constexpr int DEFAULT_BAR_WIDTH = 50;
constexpr int PERCENTAGE_MULTIPLIER = 100;
constexpr int MAX_LABEL_WIDTH = 15;
constexpr int THREAD_ID_WIDTH = 16;

inline std::shared_mutex log_mutex;

/**
 * @brief Thread-safe logging function with structured format
 * @tparam Stream Output stream type
 * @tparam Args Variadic template for format arguments
 * @param stream Output stream
 * @param level Log level
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename Stream, typename... Args>
inline void log(Stream& stream, LogLevel level, std::string_view fmt,
                Args&&... args) {
    std::unique_lock lock(log_mutex);

    static constexpr std::array<std::string_view, 4> level_strings = {
        "DEBUG", "INFO", "WARNING", "ERROR"};

    const auto level_str = level_strings[static_cast<size_t>(level)];
    const auto thread_id = std::this_thread::get_id();
    const auto hash_value = std::hash<std::thread::id>{}(thread_id);

    try {
        stream << "[" << atom::utils::getChinaTimestampString() << "] ["
               << level_str << "] [" << std::hex << std::setw(THREAD_ID_WIDTH)
               << std::setfill('0') << hash_value << "] "
               << std::vformat(
                      fmt, std::make_format_args(std::forward<Args>(args)...))
               << std::endl;
    } catch (const std::format_error& e) {
        stream << "[" << atom::utils::getChinaTimestampString() << "] [ERROR] "
               << "Format error occurred: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        stream << "[" << atom::utils::getChinaTimestampString() << "] [ERROR] "
               << "Exception occurred during logging: " << e.what()
               << std::endl;
    }
}

/**
 * @brief Count placeholders in format string
 * @param fmt Format string to analyze
 * @return Number of {} placeholders found
 */
inline size_t countPlaceholders(std::string_view fmt) noexcept {
    size_t count = 0;
    for (size_t pos = 0; (pos = fmt.find("{}", pos)) != std::string_view::npos;
         pos += 2) {
        ++count;
    }
    return count;
}

template <typename Stream>
inline void formatToStream(Stream& stream, std::string_view fmt) {
    stream << fmt;
}

template <typename Stream, typename T, typename... Args>
inline void formatToStream(Stream& stream, std::string_view fmt, T&& value,
                           Args&&... args) {
    if (const auto pos = fmt.find("{}"); pos != std::string_view::npos) {
        stream << fmt.substr(0, pos) << value;
        formatToStream(stream, fmt.substr(pos + 2),
                       std::forward<Args>(args)...);
    } else {
        stream << fmt;
    }
}

/**
 * @brief Print formatted text to any output stream
 * @tparam Stream Output stream type
 * @tparam Args Variadic template for format arguments
 * @param stream Output stream
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename Stream, typename... Args>
inline void printToStream(Stream& stream, std::string_view fmt,
                          Args&&... args) {
    try {
        if constexpr (sizeof...(args) > 0) {
            const auto placeholder_count = countPlaceholders(fmt);
            const auto arg_count = sizeof...(args);

            if (placeholder_count != arg_count) {
                stream << "Format error: mismatch between placeholders ("
                       << placeholder_count << ") and arguments (" << arg_count
                       << ")";
                return;
            }
        }

        formatToStream(stream, fmt, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        stream << "Error during formatting: " << e.what();
    }
}

/**
 * @brief Print formatted text to stdout
 * @tparam Args Variadic template for format arguments
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
inline void print(std::string_view fmt, Args&&... args) {
    printToStream(std::cout, fmt, std::forward<Args>(args)...);
}

/**
 * @brief Print formatted text with newline to any output stream
 * @tparam Stream Output stream type
 * @tparam Args Variadic template for format arguments
 * @param stream Output stream
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename Stream, typename... Args>
inline void printlnToStream(Stream& stream, std::string_view fmt,
                            Args&&... args) {
    printToStream(stream, fmt, std::forward<Args>(args)...);
    stream << '\n';
}

/**
 * @brief Print formatted text with newline to stdout
 * @tparam Args Variadic template for format arguments
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
inline void println(std::string_view fmt, Args&&... args) {
    printlnToStream(std::cout, fmt, std::forward<Args>(args)...);
}

/**
 * @brief Print formatted text to file
 * @tparam Args Variadic template for format arguments
 * @param fileName Target file name
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
inline void printToFile(const std::string& fileName, std::string_view fmt,
                        Args&&... args) {
    try {
        std::ofstream file(fileName, std::ios::app);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + fileName);
        }
        printToStream(file, fmt, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        std::cerr << "Error writing to file: " << e.what() << std::endl;
    }
}

/**
 * @brief Print colored text to console
 * @tparam Args Variadic template for format arguments
 * @param color Text color
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
inline void printColored(Color color, std::string_view fmt, Args&&... args) {
    std::cout << "\033[" << static_cast<int>(color) << "m";
    print(fmt, std::forward<Args>(args)...);
    std::cout << "\033[0m";
}

/**
 * @brief Print styled text to console
 * @tparam Args Variadic template for format arguments
 * @param style Text style
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
inline void printStyled(TextStyle style, std::string_view fmt, Args&&... args) {
    std::cout << "\033[" << static_cast<int>(style) << "m";
    print(fmt, std::forward<Args>(args)...);
    std::cout << "\033[0m";
}

/**
 * @brief High-precision timer for performance measurement
 */
class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;

public:
    Timer() : start_time_(std::chrono::high_resolution_clock::now()) {}

    /**
     * @brief Reset timer to current time
     */
    void reset() noexcept {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Get elapsed time in seconds
     * @return Elapsed time as double precision seconds
     */
    [[nodiscard]] double elapsed() const noexcept {
        const auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end_time - start_time_).count();
    }

    /**
     * @brief Measure execution time of a function with return value
     * @tparam Func Function type
     * @param operation_name Name of the operation for logging
     * @param func Function to measure
     * @return Function return value
     */
    template <typename Func>
    static auto measure(std::string_view operation_name, Func&& func) {
        Timer timer;
        auto result = std::forward<Func>(func)();
        println("{} completed in {:.6f} seconds", operation_name,
                timer.elapsed());
        return result;
    }

    /**
     * @brief Measure execution time of a void function
     * @tparam Func Function type
     * @param operation_name Name of the operation for logging
     * @param func Function to measure
     */
    template <typename Func>
    static void measureVoid(std::string_view operation_name, Func&& func) {
        Timer timer;
        std::forward<Func>(func)();
        println("{} completed in {:.6f} seconds", operation_name,
                timer.elapsed());
    }
};

/**
 * @brief Code block formatter with automatic indentation
 */
class CodeBlock {
private:
    int indent_level_ = 0;
    static constexpr int SPACES_PER_INDENT = 4;

public:
    constexpr void increaseIndent() noexcept { ++indent_level_; }

    constexpr void decreaseIndent() noexcept {
        if (indent_level_ > 0) {
            --indent_level_;
        }
    }

    /**
     * @brief Print with current indentation level
     * @tparam Args Variadic template for format arguments
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    void print(std::string_view fmt, Args&&... args) const {
        std::cout << std::string(
            static_cast<size_t>(indent_level_) * SPACES_PER_INDENT, ' ');
        atom::utils::print(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Print with newline and current indentation level
     * @tparam Args Variadic template for format arguments
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    void println(std::string_view fmt, Args&&... args) const {
        std::cout << std::string(
            static_cast<size_t>(indent_level_) * SPACES_PER_INDENT, ' ');
        atom::utils::println(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief RAII-style automatic indentation management
     */
    class ScopedIndent {
    private:
        CodeBlock& block_;

    public:
        explicit ScopedIndent(CodeBlock& block) : block_(block) {
            block_.increaseIndent();
        }

        ~ScopedIndent() { block_.decreaseIndent(); }

        ScopedIndent(const ScopedIndent&) = delete;
        ScopedIndent& operator=(const ScopedIndent&) = delete;
        ScopedIndent(ScopedIndent&&) = delete;
        ScopedIndent& operator=(ScopedIndent&&) = delete;
    };

    /**
     * @brief Create a scoped indentation block
     * @return RAII indentation object
     */
    [[nodiscard]] ScopedIndent indent() { return ScopedIndent(*this); }
};

/**
 * @brief Statistical analysis utilities for containers
 */
class MathStats {
public:
    /**
     * @brief Calculate arithmetic mean of container elements
     * @tparam C Container type
     * @param data Container with numeric data
     * @return Mean value as double
     */
    template <Container C>
    [[nodiscard]] static double mean(const C& data) {
        if (data.empty()) {
            throw std::invalid_argument(
                "Cannot calculate mean of empty container");
        }

        if constexpr (std::ranges::sized_range<C>) {
            const auto sum = std::accumulate(data.begin(), data.end(), 0.0);
            return sum / static_cast<double>(std::ranges::size(data));
        } else {
            double sum = 0.0;
            size_t count = 0;
            for (const auto& value : data) {
                sum += value;
                ++count;
            }
            return sum / static_cast<double>(count);
        }
    }

    /**
     * @brief Calculate median of container elements
     * @tparam C Container type
     * @param data Container with numeric data (will be modified)
     * @return Median value as double
     */
    template <std::ranges::forward_range C>
    [[nodiscard]] static double median(C data) {
        if (data.empty()) {
            throw std::invalid_argument(
                "Cannot calculate median of empty container");
        }

        const auto size = std::ranges::distance(data.begin(), data.end());
        std::ranges::sort(data);

        if (size % 2 == 0) {
            const auto mid = data.begin() + size / 2 - 1;
            const auto mid_next = std::next(mid);
            return (*mid + *mid_next) / 2.0;
        }
        return *(data.begin() + size / 2);
    }

    /**
     * @brief Calculate standard deviation of container elements
     * @tparam C Container type
     * @param data Container with numeric data
     * @return Standard deviation as double
     */
    template <Container C>
    [[nodiscard]] static double standardDeviation(const C& data) {
        if (data.empty()) {
            throw std::invalid_argument(
                "Cannot calculate standard deviation of empty container");
        }

        const double mean_value = mean(data);

        if constexpr (std::ranges::sized_range<C>) {
            if (std::ranges::size(data) > 1000) {
                return parallelStdDev(data, mean_value);
            }
        }

        double variance = 0.0;
        for (const auto& value : data) {
            const double diff = value - mean_value;
            variance += diff * diff;
        }

        return std::sqrt(variance /
                         static_cast<double>(std::ranges::size(data)));
    }

private:
    template <Container C>
    [[nodiscard]] static double parallelStdDev(const C& data,
                                               double mean_value) {
        const auto num_threads =
            std::min(std::thread::hardware_concurrency(), 8u);
        const auto chunk_size = std::ranges::size(data) / num_threads;

        std::vector<std::future<double>> futures;
        futures.reserve(num_threads);

        auto it = data.begin();
        for (size_t i = 0; i < num_threads; ++i) {
            const auto start = it;
            std::advance(it, (i < num_threads - 1)
                                 ? chunk_size
                                 : std::distance(it, data.end()));

            futures.push_back(
                std::async(std::launch::async, [start, it, mean_value]() {
                    double partial_sum = 0.0;
                    for (auto current = start; current != it; ++current) {
                        const double diff = *current - mean_value;
                        partial_sum += diff * diff;
                    }
                    return partial_sum;
                }));
        }

        double variance = 0.0;
        for (auto& future : futures) {
            variance += future.get();
        }

        return std::sqrt(variance /
                         static_cast<double>(std::ranges::size(data)));
    }
};

/**
 * @brief Memory usage tracking utility
 */
class MemoryTracker {
private:
    std::unordered_map<std::string, size_t> allocations_;
    mutable std::shared_mutex mutex_;

public:
    /**
     * @brief Register memory allocation
     * @param identifier Unique identifier for the allocation
     * @param size Size of allocation in bytes
     */
    void allocate(const std::string& identifier, size_t size) {
        std::unique_lock lock(mutex_);
        allocations_[identifier] = size;
    }

    /**
     * @brief Unregister memory allocation
     * @param identifier Identifier of allocation to remove
     */
    void deallocate(const std::string& identifier) {
        std::unique_lock lock(mutex_);
        allocations_.erase(identifier);
    }

    /**
     * @brief Print current memory usage statistics
     */
    void printUsage() const {
        std::shared_lock lock(mutex_);

        size_t total = 0;
        for (const auto& [identifier, size] : allocations_) {
            println("{}: {} bytes", identifier, size);
            total += size;
        }

        println("Total memory usage: {} bytes", total);

        if (total < 1024) {
            println("({} B)", total);
        } else if (total < 1024 * 1024) {
            println("({:.2f} KB)", total / 1024.0);
        } else if (total < 1024ULL * 1024 * 1024) {
            println("({:.2f} MB)", total / (1024.0 * 1024.0));
        } else {
            println("({:.2f} GB)", total / (1024.0 * 1024.0 * 1024.0));
        }
    }
};

/**
 * @brief Compile-time format string wrapper
 */
class FormatLiteral {
    std::string_view fmt_str_;

public:
    constexpr explicit FormatLiteral(std::string_view format)
        : fmt_str_(format) {}

    /**
     * @brief Apply format arguments to the format string
     * @tparam Args Variadic template for format arguments
     * @param args Format arguments
     * @return Formatted string
     */
    template <typename... Args>
    [[nodiscard]] std::string operator()(Args&&... args) const {
        try {
            return std::vformat(
                fmt_str_, std::make_format_args(std::forward<Args>(args)...));
        } catch (const std::format_error& e) {
            return std::string("Format error: ") + e.what();
        } catch (const std::exception& e) {
            return std::string("Error: ") + e.what();
        }
    }
};

/**
 * @brief Thread-safe singleton logger class
 */
class Logger {
private:
    std::ofstream log_file_;
    mutable std::mutex log_mutex_;
    static std::unique_ptr<Logger> instance_;
    static std::once_flag init_instance_flag_;

    Logger() = default;

public:
    /**
     * @brief Get singleton logger instance
     * @return Reference to logger instance
     */
    static Logger& getInstance() {
        std::call_once(init_instance_flag_, []() {
            instance_ = std::unique_ptr<Logger>(new Logger());
        });
        return *instance_;
    }

    /**
     * @brief Open log file for writing
     * @param filename Path to log file
     * @return True if file opened successfully
     */
    bool openLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }

        log_file_.open(filename, std::ios::app);
        return log_file_.is_open();
    }

    /**
     * @brief Write log message to file
     * @tparam Args Variadic template for format arguments
     * @param level Log level
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    void log(LogLevel level, std::string_view fmt, Args&&... args) {
        std::lock_guard<std::mutex> lock(log_mutex_);

        if (!log_file_.is_open()) {
            std::cerr << "Error: Log file not open" << std::endl;
            return;
        }

        atom::utils::log(log_file_, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Close log file
     */
    void close() {
        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    ~Logger() { close(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
};

/**
 * @brief Display progress bar with customizable styles
 * @param progress Progress value between 0.0 and 1.0
 * @param bar_width Width of the progress bar in characters
 * @param style Visual style of the progress bar
 */
void printProgressBar(float progress, int bar_width = DEFAULT_BAR_WIDTH,
                      ProgressBarStyle style = ProgressBarStyle::BASIC);

/**
 * @brief Print data in table format
 * @param data 2D vector containing table data
 */
void printTable(const std::vector<std::vector<std::string>>& data);

/**
 * @brief Pretty-print JSON with proper indentation
 * @param json JSON string to format
 * @param indent Number of spaces per indentation level
 */
void printJson(const std::string& json, int indent = 2);

/**
 * @brief Print horizontal bar chart
 * @param data Map of labels to values
 * @param max_width Maximum width of bars in characters
 */
void printBarChart(const std::map<std::string, int>& data,
                   int max_width = DEFAULT_BAR_WIDTH);

}  // namespace atom::utils

/**
 * @brief User-defined literal for format strings
 * @param str C-style string
 * @param len String length
 * @return FormatLiteral object
 */
constexpr auto operator""_fmt(const char* str, std::size_t len) {
    return atom::utils::FormatLiteral(std::string_view(str, len));
}

#if __cplusplus >= 202302L
namespace std {

template <typename T>
struct formatter<
    T,
    enable_if_t<is_same_v<T, std::vector<typename T::value_type>> ||
                    is_same_v<T, std::list<typename T::value_type>> ||
                    is_same_v<T, std::set<typename T::value_type>> ||
                    is_same_v<T, std::unordered_set<typename T::value_type>> ||
                    is_same_v<T, std::deque<typename T::value_type>> ||
                    is_same_v<T, std::forward_list<typename T::value_type>>,
                char>> : formatter<std::string_view> {
    auto format(const T& container, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        *out++ = '[';
        bool first = true;
        for (const auto& item : container) {
            if (!first) {
                *out++ = ',';
                *out++ = ' ';
            }
            out = std::format_to(out, "{}", item);
            first = false;
        }
        *out++ = ']';
        return out;
    }
};

template <typename T1, typename T2>
struct formatter<std::map<T1, T2>> : formatter<std::string_view> {
    auto format(const std::map<T1, T2>& m, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        *out++ = '{';
        bool first = true;
        for (const auto& [key, value] : m) {
            if (!first) {
                *out++ = ',';
                *out++ = ' ';
            }
            out = std::format_to(out, "{}: {}", key, value);
            first = false;
        }
        *out++ = '}';
        return out;
    }
};

template <typename T1, typename T2>
struct formatter<std::unordered_map<T1, T2>> : formatter<std::string_view> {
    auto format(const std::unordered_map<T1, T2>& m, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        *out++ = '{';
        bool first = true;
        for (const auto& [key, value] : m) {
            if (!first) {
                *out++ = ',';
                *out++ = ' ';
            }
            out = std::format_to(out, "{}: {}", key, value);
            first = false;
        }
        *out++ = '}';
        return out;
    }
};

template <typename T, std::size_t N>
struct formatter<std::array<T, N>> : formatter<std::string_view> {
    auto format(const std::array<T, N>& arr, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        *out++ = '[';
        for (std::size_t i = 0; i < N; ++i) {
            if (i > 0) {
                *out++ = ',';
                *out++ = ' ';
            }
            out = std::format_to(out, "{}", arr[i]);
        }
        *out++ = ']';
        return out;
    }
};

template <typename T1, typename T2>
struct formatter<std::pair<T1, T2>> : formatter<std::string_view> {
    auto format(const std::pair<T1, T2>& p, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        *out++ = '(';
        out = std::format_to(out, "{}", p.first);
        *out++ = ',';
        *out++ = ' ';
        out = std::format_to(out, "{}", p.second);
        *out++ = ')';
        return out;
    }
};

template <typename... Ts>
struct formatter<std::tuple<Ts...>> : formatter<std::string_view> {
    auto format(const std::tuple<Ts...>& tup, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        *out++ = '(';
        std::apply(
            [&](const Ts&... args) {
                std::size_t n = 0;
                ((void)((n++ > 0 ? (out = std::format_to(out, ", {}", args))
                                 : (out = std::format_to(out, "{}", args))),
                        0),
                 ...);
            },
            tup);
        *out++ = ')';
        return out;
    }
};

template <typename... Ts>
struct formatter<std::variant<Ts...>> : formatter<std::string_view> {
    auto format(const std::variant<Ts...>& var, format_context& ctx) const
        -> decltype(ctx.out()) {
        return std::visit(
            [&ctx](const auto& val) -> decltype(ctx.out()) {
                return std::format_to(ctx.out(), "{}", val);
            },
            var);
    }
};

template <typename T>
struct formatter<std::optional<T>> : formatter<std::string_view> {
    auto format(const std::optional<T>& opt, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto out = ctx.out();
        if (opt.has_value()) {
            return std::format_to(out, "Optional({})", opt.value());
        }
        return std::format_to(out, "Optional()");
    }
};

}  // namespace std
#endif

#endif
