/*
 * fifo_logger.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Unified logging for FIFO operations

*************************************************/

#ifndef ATOM_CONNECTION_FIFO_LOGGER_HPP
#define ATOM_CONNECTION_FIFO_LOGGER_HPP

#include <string>
#include <string_view>

#include "fifo_common.hpp"

#ifdef ATOM_USE_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace atom::connection {

/**
 * @brief Unified logger for FIFO operations
 *
 * This class provides a consistent logging interface for all FIFO components.
 * It wraps spdlog when available and falls back to stdout/stderr otherwise.
 */
class FifoLogger {
public:
    /**
     * @brief Construct a logger with the given name
     * @param name Logger name (e.g., "FifoClient", "FifoServer")
     */
    explicit FifoLogger(std::string_view name) : name_(name), level_(LogLevel::Info) {}

    /**
     * @brief Set the log level
     */
    void setLevel(LogLevel level) { level_ = level; }

    /**
     * @brief Get the current log level
     */
    [[nodiscard]] LogLevel getLevel() const { return level_; }

    /**
     * @brief Log a debug message
     */
    template <typename... Args>
    void debug(std::string_view fmt, Args&&... args) const {
        if (level_ <= LogLevel::Debug) {
            log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log an info message
     */
    template <typename... Args>
    void info(std::string_view fmt, Args&&... args) const {
        if (level_ <= LogLevel::Info) {
            log(LogLevel::Info, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a warning message
     */
    template <typename... Args>
    void warn(std::string_view fmt, Args&&... args) const {
        if (level_ <= LogLevel::Warning) {
            log(LogLevel::Warning, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log an error message
     */
    template <typename... Args>
    void error(std::string_view fmt, Args&&... args) const {
        if (level_ <= LogLevel::Error) {
            log(LogLevel::Error, fmt, std::forward<Args>(args)...);
        }
    }

private:
    template <typename... Args>
    void log(LogLevel level, std::string_view fmt, Args&&... args) const {
#ifdef ATOM_USE_SPDLOG
        auto logger = spdlog::get(std::string(name_));
        if (!logger) {
            logger = spdlog::default_logger();
        }

        switch (level) {
            case LogLevel::Debug:
                logger->debug(fmt::runtime(fmt), std::forward<Args>(args)...);
                break;
            case LogLevel::Info:
                logger->info(fmt::runtime(fmt), std::forward<Args>(args)...);
                break;
            case LogLevel::Warning:
                logger->warn(fmt::runtime(fmt), std::forward<Args>(args)...);
                break;
            case LogLevel::Error:
                logger->error(fmt::runtime(fmt), std::forward<Args>(args)...);
                break;
            case LogLevel::None:
                break;
        }
#else
        // Fallback to simple stderr output
        if (level == LogLevel::None) return;

        const char* levelStr = "";
        switch (level) {
            case LogLevel::Debug: levelStr = "DEBUG"; break;
            case LogLevel::Info: levelStr = "INFO"; break;
            case LogLevel::Warning: levelStr = "WARN"; break;
            case LogLevel::Error: levelStr = "ERROR"; break;
            default: break;
        }

        // Simple format without variadic expansion for fallback
        std::fprintf(stderr, "[%s][%.*s] %.*s\n",
                     levelStr,
                     static_cast<int>(name_.size()), name_.data(),
                     static_cast<int>(fmt.size()), fmt.data());
        (void)sizeof...(args);  // Suppress unused warning
#endif
    }

    std::string name_;
    LogLevel level_;
};

/**
 * @brief Get the default FIFO logger
 */
inline FifoLogger& getDefaultFifoLogger() {
    static FifoLogger logger("FIFO");
    return logger;
}

/**
 * @brief Convenience macros for logging
 */
#define FIFO_LOG_DEBUG(...) atom::connection::getDefaultFifoLogger().debug(__VA_ARGS__)
#define FIFO_LOG_INFO(...) atom::connection::getDefaultFifoLogger().info(__VA_ARGS__)
#define FIFO_LOG_WARN(...) atom::connection::getDefaultFifoLogger().warn(__VA_ARGS__)
#define FIFO_LOG_ERROR(...) atom::connection::getDefaultFifoLogger().error(__VA_ARGS__)

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFO_LOGGER_HPP
