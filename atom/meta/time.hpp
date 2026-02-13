/*!
 * \file time.hpp
 * \brief Record compile time
 * \author Max Qian <lightapt.com>
 * \date 2024-05-25
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_TIME_HPP
#define ATOM_META_TIME_HPP

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include "atom/macro.hpp"

namespace atom::meta {
ATOM_INLINE auto getCompileTime() -> std::string {
    std::string date = __DATE__;
    std::string time = __TIME__;
    std::istringstream dateStream(date);
    std::tm tm{};
    dateStream >> std::get_time(&tm, "%b %d %Y");
    std::istringstream timeStream(time);
    timeStream >> std::get_time(&tm, "%H:%M:%S");
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
//==============================================================================
// C++23 Enhanced Compile Time Utilities
//==============================================================================

/**
 * @brief Get compile date as string
 */
ATOM_INLINE auto getCompileDate() -> std::string { return __DATE__; }

/**
 * @brief Get compile time as string
 */
ATOM_INLINE auto getCompileTimeOnly() -> std::string { return __TIME__; }

/**
 * @brief Check if compiled in debug mode
 */
ATOM_INLINE constexpr bool isDebugBuild() {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

/**
 * @brief Get compiler information
 */
ATOM_INLINE auto getCompilerInfo() -> std::string {
#if defined(__clang__)
    return "Clang " + std::to_string(__clang_major__) + "." +
           std::to_string(__clang_minor__) + "." +
           std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." +
           std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "Unknown Compiler";
#endif
}

/**
 * @brief Get C++ standard version
 */
ATOM_INLINE constexpr int getCppStandard() {
#if __cplusplus >= 202302L
    return 23;
#elif __cplusplus >= 202002L
    return 20;
#elif __cplusplus >= 201703L
    return 17;
#elif __cplusplus >= 201402L
    return 14;
#elif __cplusplus >= 201103L
    return 11;
#else
    return 98;
#endif
}

/**
 * @brief Build information structure
 */
struct BuildInfo {
    std::string compile_time;
    std::string compiler;
    int cpp_standard;
    bool is_debug;

    static BuildInfo get() {
        return BuildInfo{.compile_time = getCompileTime(),
                         .compiler = getCompilerInfo(),
                         .cpp_standard = getCppStandard(),
                         .is_debug = isDebugBuild()};
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "Build Info:\n";
        oss << "  Compile Time: " << compile_time << "\n";
        oss << "  Compiler: " << compiler << "\n";
        oss << "  C++ Standard: C++" << cpp_standard << "\n";
        oss << "  Debug Build: " << (is_debug ? "Yes" : "No") << "\n";
        return oss.str();
    }
};

/**
 * @brief Compile-time string literal
 */
template <std::size_t N>
struct CompileTimeString {
    char data[N]{};

    constexpr CompileTimeString(const char (&str)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
            data[i] = str[i];
        }
    }

    constexpr operator std::string_view() const {
        return std::string_view(data, N - 1);
    }

    constexpr std::size_t size() const { return N - 1; }
};

/**
 * @brief Runtime timer for measuring durations
 */
class Timer {
    std::chrono::high_resolution_clock::time_point start_;
    std::string name_;

public:
    explicit Timer(std::string name = "Timer")
        : start_(std::chrono::high_resolution_clock::now()),
          name_(std::move(name)) {}

    [[nodiscard]] auto elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now -
                                                                    start_);
    }

    [[nodiscard]] double elapsedMs() const {
        return elapsed().count() / 1'000'000.0;
    }

    [[nodiscard]] double elapsedUs() const {
        return elapsed().count() / 1'000.0;
    }

    void reset() { start_ = std::chrono::high_resolution_clock::now(); }

    [[nodiscard]] const std::string& name() const { return name_; }
};

/**
 * @brief Scoped timer that prints duration on destruction
 */
class ScopedTimer {
    Timer timer_;
    std::function<void(const std::string&, double)> callback_;

public:
    explicit ScopedTimer(
        std::string name,
        std::function<void(const std::string&, double)> callback = nullptr)
        : timer_(std::move(name)), callback_(std::move(callback)) {}

    ~ScopedTimer() {
        if (callback_) {
            callback_(timer_.name(), timer_.elapsedMs());
        }
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};

/**
 * @brief Create a scoped timer with default logging
 */
inline auto makeScopedTimer(std::string name) {
    return ScopedTimer(std::move(name), [](const std::string& n, double ms) {
        // Default: no output, but measurement is recorded
    });
}

}  // namespace atom::meta

#endif
