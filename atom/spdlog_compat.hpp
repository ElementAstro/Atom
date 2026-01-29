#ifndef ATOM_SPDLOG_COMPAT_HPP
#define ATOM_SPDLOG_COMPAT_HPP

// Compatibility header for spdlog
// Provides fallback implementations when spdlog is not available

#ifdef _MSC_VER
// Check if spdlog is available
#if __has_include(<spdlog/spdlog.h>)
#define ATOM_HAS_SPDLOG 1
#include <spdlog/spdlog.h>
#else
#define ATOM_HAS_SPDLOG 0
// Provide minimal fallback implementations
#include <format>
#include <iostream>
#include <string>

namespace spdlog {
// Minimal logger interface for compatibility
inline void trace(const std::string& msg) {
    std::cout << "[TRACE] " << msg << std::endl;
}

inline void debug(const std::string& msg) {
    std::cout << "[DEBUG] " << msg << std::endl;
}

inline void info(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

inline void warn(const std::string& msg) {
    std::cout << "[WARN] " << msg << std::endl;
}

inline void error(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

inline void critical(const std::string& msg) {
    std::cerr << "[CRITICAL] " << msg << std::endl;
}

// Template versions for formatted output
template <typename... Args>
inline void trace(const std::string& fmt, Args&&... args) {
    try {
        std::cout << "[TRACE] "
                  << std::vformat(fmt, std::make_format_args(args...))
                  << std::endl;
    } catch (...) {
        std::cout << "[TRACE] " << fmt << std::endl;
    }
}

template <typename... Args>
inline void debug(const std::string& fmt, Args&&... args) {
    try {
        std::cout << "[DEBUG] "
                  << std::vformat(fmt, std::make_format_args(args...))
                  << std::endl;
    } catch (...) {
        std::cout << "[DEBUG] " << fmt << std::endl;
    }
}

template <typename... Args>
inline void info(const std::string& fmt, Args&&... args) {
    try {
        std::cout << "[INFO] "
                  << std::vformat(fmt, std::make_format_args(args...))
                  << std::endl;
    } catch (...) {
        std::cout << "[INFO] " << fmt << std::endl;
    }
}

template <typename... Args>
inline void warn(const std::string& fmt, Args&&... args) {
    try {
        std::cout << "[WARN] "
                  << std::vformat(fmt, std::make_format_args(args...))
                  << std::endl;
    } catch (...) {
        std::cout << "[WARN] " << fmt << std::endl;
    }
}

template <typename... Args>
inline void error(const std::string& fmt, Args&&... args) {
    try {
        std::cerr << "[ERROR] "
                  << std::vformat(fmt, std::make_format_args(args...))
                  << std::endl;
    } catch (...) {
        std::cerr << "[ERROR] " << fmt << std::endl;
    }
}

template <typename... Args>
inline void critical(const std::string& fmt, Args&&... args) {
    try {
        std::cerr << "[CRITICAL] "
                  << std::vformat(fmt, std::make_format_args(args...))
                  << std::endl;
    } catch (...) {
        std::cerr << "[CRITICAL] " << fmt << std::endl;
    }
}
}  // namespace spdlog

#endif  // __has_include(<spdlog/spdlog.h>)

#else
// Non-MSVC compilers: assume spdlog is available
#define ATOM_HAS_SPDLOG 1
#include <spdlog/spdlog.h>
#endif  // _MSC_VER

#endif  // ATOM_SPDLOG_COMPAT_HPP
