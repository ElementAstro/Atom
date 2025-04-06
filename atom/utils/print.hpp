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
#include <sstream>
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

// C++20 concepts to constrain templates
template <typename T>
concept Printable = requires(std::ostream& os, T value) {
    { os << value } -> std::convertible_to<std::ostream&>;
};

template <typename T>
concept Container = requires(T a) {
    typename T::value_type;
    typename T::iterator;
    { a.begin() } -> std::input_or_output_iterator;
    { a.end() } -> std::input_or_output_iterator;
    { a.size() } -> std::convertible_to<std::size_t>;
};

enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

constexpr int DEFAULT_BAR_WIDTH = 50;
constexpr int PERCENTAGE_MULTIPLIER = 100;
constexpr int SLEEP_DURATION_MS = 200;
constexpr int MAX_LABEL_WIDTH = 15;
constexpr int BUFFER1_SIZE = 1024;
constexpr int BUFFER2_SIZE = 2048;
constexpr int BUFFER3_SIZE = 4096;
constexpr int THREAD_ID_WIDTH = 16;

// Thread-safe logging with mutex
inline std::shared_mutex log_mutex;

template <typename Stream, typename... Args>
inline void log(Stream& stream, LogLevel level, std::string_view fmt,
                Args&&... args) {
    std::unique_lock lock(log_mutex);

    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG:
            levelStr = "DEBUG";
            break;
        case LogLevel::INFO:
            levelStr = "INFO";
            break;
        case LogLevel::WARNING:
            levelStr = "WARNING";
            break;
        case LogLevel::ERROR:
            levelStr = "ERROR";
            break;
    }

    std::thread::id thisId = std::this_thread::get_id();

    std::hash<std::thread::id> hasher;
    size_t hashValue = hasher(thisId);

    std::ostringstream oss;
    oss << std::hex << std::setw(THREAD_ID_WIDTH) << std::setfill('0')
        << hashValue;
    std::string idHexStr = oss.str();

    try {
        stream << "[" << atom::utils::getChinaTimestampString() << "] ["
               << levelStr << "] [" << idHexStr << "] "
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

// TODO: Add support for other streams like std::cerr, std::clog, etc.
/*
template <typename Stream, typename... Args>
inline void printToStream(Stream& stream, std::string_view fmt,
                          Args&&... args) {
    try {
        // 使用 C++20 标准库的 std::format
        stream << std::vformat(fmt,
                      std::make_format_args(std::forward<Args>(args)...));
    } catch (const std::format_error& e) {
        stream << "Format error: " << e.what();
    } catch (const std::exception& e) {
        stream << "Error during formatting: " << e.what();
    }
}
*/

// 计算格式字符串中的占位符数量
inline size_t countPlaceholders(std::string_view fmt) {
    size_t count = 0;
    size_t pos = 0;

    while ((pos = fmt.find("{}", pos)) != std::string_view::npos) {
        ++count;
        pos += 2;  // 跳过 "{}"
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
    size_t pos = fmt.find("{}");
    if (pos == std::string_view::npos) {
        stream << fmt;
        return;
    }
    stream << fmt.substr(0, pos);
    stream << value;
    formatToStream(stream, fmt.substr(pos + 2), std::forward<Args>(args)...);
}

template <typename Stream, typename... Args>
inline void printToStream(Stream& stream, std::string_view fmt,
                          Args&&... args) {
    try {
        size_t placeholderCount = countPlaceholders(fmt);
        size_t argCount = sizeof...(args);

        if (placeholderCount != argCount) {
            stream << "Format error: mismatch between placeholders ("
                   << placeholderCount << ") and arguments (" << argCount
                   << ")";
            return;
        }

        formatToStream(stream, fmt, std::forward<Args>(args)...);
    } catch (const std::format_error& e) {
        stream << "Format error: " << e.what();
    } catch (const std::exception& e) {
        stream << "Error during formatting: " << e.what();
    }
}

template <typename... Args>
inline void print(std::string_view fmt, Args&&... args) {
    printToStream(std::cout, fmt, std::forward<Args>(args)...);
}

template <typename Stream, typename... Args>
inline void printlnToStream(Stream& stream, std::string_view fmt,
                            Args&&... args) {
    printToStream(stream, fmt, std::forward<Args>(args)...);
    stream << std::endl;
}

template <typename... Args>
inline void println(std::string_view fmt, Args&&... args) {
    printlnToStream(std::cout, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void printToFile(const std::string& fileName, std::string_view fmt,
                        Args&&... args) {
    try {
        std::ofstream file(fileName, std::ios::app);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + fileName);
        }

        printToStream(file, fmt, std::forward<Args>(args)...);
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Error writing to file: " << e.what() << std::endl;
    }
}

enum class Color {
    RED = 31,
    GREEN = 32,
    YELLOW = 33,
    BLUE = 34,
    MAGENTA = 35,
    CYAN = 36,
    WHITE = 37
};

template <typename... Args>
inline void printColored(Color color, std::string_view fmt, Args&&... args) {
    std::cout << "\033[" << static_cast<int>(color) << "m"
              << std::vformat(
                     fmt, std::make_format_args(std::forward<Args>(args)...))
              << "\033[0m";  // Reset to default color
}

class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

public:
    Timer() : startTime(std::chrono::high_resolution_clock::now()) {}

    void reset() { startTime = std::chrono::high_resolution_clock::now(); }

    [[nodiscard]] inline auto elapsed() const -> double {
        auto endTime = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(endTime - startTime).count();
    }

    // Scoped timing - automatically print elapsed time when going out of scope
    template <typename Func>
    static inline auto measure(std::string_view operation_name, Func&& func) {
        Timer timer;
        auto result = std::forward<Func>(func)();
        println("{} completed in {:.6f} seconds", operation_name,
                timer.elapsed());
        return result;
    }

    template <typename Func>
    static inline void measureVoid(std::string_view operation_name,
                                   Func&& func) {
        Timer timer;
        std::forward<Func>(func)();
        println("{} completed in {:.6f} seconds", operation_name,
                timer.elapsed());
    }
};

class CodeBlock {
private:
    int indentLevel = 0;
    static constexpr int spacesPerIndent = 4;

public:
    constexpr void increaseIndent() { ++indentLevel; }
    constexpr void decreaseIndent() {
        if (indentLevel > 0) {
            --indentLevel;
        }
    }

    template <typename... Args>
    inline void print(std::string_view fmt, Args&&... args) const {
        std::cout << std::string(
            static_cast<size_t>(indentLevel) * spacesPerIndent, ' ');
        atom::utils::print(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void println(std::string_view fmt, Args&&... args) const {
        std::cout << std::string(
            static_cast<size_t>(indentLevel) * spacesPerIndent, ' ');
        atom::utils::println(fmt, std::forward<Args>(args)...);
    }

    // RAII style indentation
    class ScopedIndent {
    private:
        CodeBlock& block;

    public:
        explicit ScopedIndent(CodeBlock& b) : block(b) {
            block.increaseIndent();
        }
        ~ScopedIndent() { block.decreaseIndent(); }
    };

    [[nodiscard]] inline ScopedIndent indent() { return ScopedIndent(*this); }
};

enum class TextStyle {
    BOLD = 1,
    UNDERLINE = 4,
    BLINK = 5,
    REVERSE = 7,
    CONCEALED = 8
};

template <typename... Args>
inline void printStyled(TextStyle style, std::string_view fmt, Args&&... args) {
    std::cout << "\033[" << static_cast<int>(style) << "m"
              << std::vformat(
                     fmt, std::make_format_args(std::forward<Args>(args)...))
              << "\033[0m";
}

class MathStats {
public:
    template <Container C>
    [[nodiscard]] static inline auto mean(const C& data) -> double {
        if (data.empty()) {
            throw std::invalid_argument(
                "Cannot calculate mean of empty container");
        }

        if constexpr (std::ranges::sized_range<C>) {
            return std::accumulate(data.begin(), data.end(), 0.0) /
                   static_cast<double>(std::ranges::size(data));
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

    template <std::ranges::forward_range C>
    [[nodiscard]] static inline auto median(C data) -> double {
        if (data.empty()) {
            throw std::invalid_argument(
                "Cannot calculate median of empty container");
        }

        auto size = std::ranges::distance(data.begin(), data.end());
        std::ranges::sort(data);

        if (size % 2 == 0) {
            auto mid = data.begin() + size / 2 - 1;
            auto midNext = std::next(mid);
            return (*mid + *midNext) / 2.0;
        } else {
            return *(data.begin() + size / 2);
        }
    }

    template <Container C>
    [[nodiscard]] static inline auto standardDeviation(const C& data)
        -> double {
        if (data.empty()) {
            throw std::invalid_argument(
                "Cannot calculate standard deviation of empty container");
        }

        double meanValue = mean(data);
        double variance = 0.0;

        // Use SIMD-friendly algorithm when possible
        if constexpr (std::ranges::sized_range<C>) {
            // Vectorized implementation for large datasets
            if (std::ranges::size(data) > 1000) {
                return parallelStdDev(data, meanValue);
            }
        }

        // Regular implementation
        for (const auto& value : data) {
            double diff = value - meanValue;
            variance += diff * diff;
        }

        return std::sqrt(variance /
                         static_cast<double>(std::ranges::size(data)));
    }

private:
    template <Container C>
    [[nodiscard]] static inline auto parallelStdDev(const C& data,
                                                    double meanValue)
        -> double {
        const size_t num_threads = std::min(std::thread::hardware_concurrency(),
                                            static_cast<unsigned>(8));
        const size_t chunk_size = std::ranges::size(data) / num_threads;

        std::vector<std::future<double>> futures;
        futures.reserve(num_threads);

        // Process chunks in parallel
        auto it = data.begin();
        for (size_t i = 0; i < num_threads; ++i) {
            auto start = it;
            std::advance(it, (i < num_threads - 1)
                                 ? chunk_size
                                 : std::distance(it, data.end()));

            futures.push_back(
                std::async(std::launch::async, [start, it, meanValue]() {
                    double partial_sum = 0.0;
                    for (auto current = start; current != it; ++current) {
                        double diff = *current - meanValue;
                        partial_sum += diff * diff;
                    }
                    return partial_sum;
                }));
        }

        // Combine results
        double variance = 0.0;
        for (auto& future : futures) {
            variance += future.get();
        }

        return std::sqrt(variance /
                         static_cast<double>(std::ranges::size(data)));
    }
};

class MemoryTracker {
private:
    std::unordered_map<std::string, size_t> allocations;
    std::shared_mutex mutex;

public:
    inline void allocate(const std::string& identifier, size_t size) {
        std::unique_lock lock(mutex);
        allocations[identifier] = size;
    }

    inline void deallocate(const std::string& identifier) {
        std::unique_lock lock(mutex);
        allocations.erase(identifier);
    }

    inline void printUsage() {
        std::shared_lock lock(mutex);

        size_t total = 0;
        for (const auto& [identifier, size] : allocations) {
            println("{}: {} bytes", identifier, size);
            total += size;
        }

        // Format the total with thousands separators
        std::stringstream ss;
        ss.imbue(std::locale(""));
        ss << std::fixed << total;

        println("Total memory usage: {} bytes", ss.str());

        // Print in human-readable format
        if (total < 1024) {
            println("({} B)", total);
        } else if (total < 1024 * 1024) {
            println("({:.2f} KB)", total / 1024.0);
        } else if (total < 1024 * 1024 * 1024) {
            println("({:.2f} MB)", total / (1024.0 * 1024.0));
        } else {
            println("({:.2f} GB)", total / (1024.0 * 1024.0 * 1024.0));
        }
    }
};

class FormatLiteral {
    std::string_view fmt_str_;

public:
    constexpr explicit FormatLiteral(std::string_view format)
        : fmt_str_(format) {}

    template <typename... Args>
    [[nodiscard]] inline auto operator()(Args&&... args) const -> std::string {
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

// Progress bar interface supporting different styles
enum class ProgressBarStyle {
    BASIC,      // [=====>     ]
    BLOCK,      // [█████▓     ]
    ARROW,      // [→→→→→→     ]
    PERCENTAGE  // 50%
};

void printProgressBar(float progress, int barWidth = DEFAULT_BAR_WIDTH,
                      ProgressBarStyle style = ProgressBarStyle::BASIC);

// Function declarations for other print utilities
void printTable(const std::vector<std::vector<std::string>>& data);
void printJson(const std::string& json, int indent = 2);
void printBarChart(const std::map<std::string, int>& data,
                   int maxWidth = DEFAULT_BAR_WIDTH);

// New: Thread-safe singleton logger class
class Logger {
private:
    std::ofstream logFile;
    std::mutex logMutex;
    static std::unique_ptr<Logger> instance;
    static std::once_flag initInstanceFlag;

    Logger() = default;

public:
    static Logger& getInstance() {
        std::call_once(initInstanceFlag, []() {
            instance = std::unique_ptr<Logger>(new Logger());
        });
        return *instance;
    }

    bool openLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) {
            logFile.close();
        }

        logFile.open(filename, std::ios::app);
        return logFile.is_open();
    }

    template <typename... Args>
    void log(LogLevel level, std::string_view fmt, Args&&... args) {
        std::lock_guard<std::mutex> lock(logMutex);

        if (!logFile.is_open()) {
            std::cerr << "Error: Log file not open" << std::endl;
            return;
        }

        atom::utils::log(logFile, level, fmt, std::forward<Args>(args)...);
    }

    void close() {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    ~Logger() { close(); }
};

}  // namespace atom::utils

// User-defined literal for format strings
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
        } else {
            return std::format_to(out, "Optional()");
        }
    }
};

}  // namespace std
#endif

#endif
