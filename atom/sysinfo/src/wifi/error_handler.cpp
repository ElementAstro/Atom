/*
 * error_handler.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - WiFi Error Handling Implementation

**************************************************/

#include "error_handler.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace atom::system {

void WiFiErrorHandler::registerErrorCallback(ErrorCallback callback) {
    std::lock_guard lock(error_mutex_);
    error_callbacks_.push_back(std::move(callback));
    LOG_F(INFO, "Error callback registered. Total callbacks: {}", error_callbacks_.size());
}

void WiFiErrorHandler::logError(WiFiError error_code, const std::string& message, const ErrorContext& context) {
    ErrorEvent event;
    event.error_code = error_code;
    event.error_message = message;
    event.context = context;
    event.timestamp = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(error_mutex_);
        error_history_.push_back(event);

        // Maintain history size limit
        if (error_history_.size() > MAX_ERROR_HISTORY) {
            error_history_.erase(error_history_.begin());
        }
    }

    // Log to system logger
    LOG_F(ERROR, "WiFi Error in {}:{} [{}:{}] - {}: {}",
          context.file_name, context.line_number, context.function_name,
          wifiErrorToString(error_code), message);

    // Notify callbacks
    notifyCallbacks(event);
}

void WiFiErrorHandler::logError(WiFiError error_code, const std::string& message,
                               const std::string& function_name, const std::string& file_name, int line_number) {
    ErrorContext context;
    context.function_name = function_name;
    context.file_name = file_name;
    context.line_number = line_number;
    context.timestamp = std::chrono::steady_clock::now();

    logError(error_code, message, context);
}

auto WiFiErrorHandler::getRecentErrors(std::chrono::minutes duration) const -> std::vector<ErrorEvent> {
    std::lock_guard lock(error_mutex_);
    std::vector<ErrorEvent> recent_errors;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    for (const auto& event : error_history_) {
        if (event.timestamp >= cutoff_time) {
            recent_errors.push_back(event);
        }
    }

    return recent_errors;
}

auto WiFiErrorHandler::getErrorStatistics(std::chrono::hours duration) const -> std::unordered_map<WiFiError, int> {
    std::lock_guard lock(error_mutex_);
    std::unordered_map<WiFiError, int> statistics;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    for (const auto& event : error_history_) {
        if (event.timestamp >= cutoff_time) {
            statistics[event.error_code]++;
        }
    }

    return statistics;
}

void WiFiErrorHandler::clearErrorHistory() {
    std::lock_guard lock(error_mutex_);
    error_history_.clear();
    LOG_F(INFO, "Error history cleared");
}

auto WiFiErrorHandler::hasRecentError(WiFiError error_code, std::chrono::minutes duration) const -> bool {
    std::lock_guard lock(error_mutex_);
    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    return std::any_of(error_history_.begin(), error_history_.end(),
                      [error_code, cutoff_time](const ErrorEvent& event) {
                          return event.error_code == error_code && event.timestamp >= cutoff_time;
                      });
}

auto WiFiErrorHandler::getErrorRate(WiFiError error_code, std::chrono::hours duration) const -> double {
    std::lock_guard lock(error_mutex_);
    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    int error_count = 0;
    for (const auto& event : error_history_) {
        if (event.error_code == error_code && event.timestamp >= cutoff_time) {
            error_count++;
        }
    }

    return static_cast<double>(error_count) / duration.count();
}

auto WiFiErrorHandler::generateErrorReport(std::chrono::hours duration) const -> std::string {
    std::ostringstream report;
    report << "WiFi Error Report (Last " << duration.count() << " hours)\n";
    report << "================================================\n\n";

    auto statistics = getErrorStatistics(duration);
    auto recent_errors = getRecentErrors(std::chrono::minutes(duration.count() * 60));

    if (statistics.empty()) {
        report << "No errors recorded in the specified time period.\n";
        return report.str();
    }

    // Error summary
    report << "Error Summary:\n";
    report << "--------------\n";
    int total_errors = 0;
    for (const auto& [error_code, count] : statistics) {
        report << "- " << wifiErrorToString(error_code) << ": " << count << " occurrences\n";
        total_errors += count;
    }
    report << "\nTotal Errors: " << total_errors << "\n\n";

    // Error rates
    report << "Error Rates (per hour):\n";
    report << "-----------------------\n";
    for (const auto& [error_code, count] : statistics) {
        double rate = getErrorRate(error_code, duration);
        report << "- " << wifiErrorToString(error_code) << ": " << std::fixed << std::setprecision(2) << rate << "/hour\n";
    }
    report << "\n";

    // Recent errors (last 10)
    report << "Recent Errors (Last 10):\n";
    report << "-------------------------\n";
    auto recent_subset = recent_errors;
    if (recent_subset.size() > 10) {
        recent_subset.resize(10);
    }

    for (const auto& event : recent_subset) {
        auto time_since_epoch = event.timestamp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch).count();

        report << "- [" << seconds << "] " << wifiErrorToString(event.error_code)
               << " in " << event.context.function_name << ": " << event.error_message << "\n";
    }

    return report.str();
}

void WiFiErrorHandler::notifyCallbacks(const ErrorEvent& event) {
    std::lock_guard lock(error_mutex_);
    for (const auto& callback : error_callbacks_) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error in error callback: {}", e.what());
        }
    }
}

// ErrorContextGuard implementation
ErrorContextGuard::ErrorContextGuard(const std::string& function_name, const std::string& file_name, int line_number) {
    context_.function_name = function_name;
    context_.file_name = file_name;
    context_.line_number = line_number;
    context_.timestamp = std::chrono::steady_clock::now();
}

ErrorContextGuard::~ErrorContextGuard() = default;

void ErrorContextGuard::addContext(const std::string& key, const std::string& value) {
    context_.additional_info[key] = value;
}

auto ErrorContextGuard::getContext() const -> const ErrorContext& {
    return context_;
}

// Global error handler
static std::unique_ptr<WiFiErrorHandler> global_error_handler;
static std::mutex global_error_handler_mutex;

auto getGlobalErrorHandler() -> WiFiErrorHandler& {
    std::lock_guard lock(global_error_handler_mutex);
    if (!global_error_handler) {
        global_error_handler = std::make_unique<WiFiErrorHandler>();
    }
    return *global_error_handler;
}

auto initializeErrorHandling() -> bool {
    std::lock_guard lock(global_error_handler_mutex);

    if (global_error_handler) {
        LOG_F(WARNING, "Error handling is already initialized");
        return true;
    }

    try {
        global_error_handler = std::make_unique<WiFiErrorHandler>();

        // Register a default error callback that logs to the system
        global_error_handler->registerErrorCallback([](const ErrorEvent& event) {
            LOG_F(ERROR, "WiFi Error Event: {} - {}",
                  wifiErrorToString(event.error_code), event.error_message);
        });

        LOG_F(INFO, "WiFi error handling initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize WiFi error handling: {}", e.what());
        return false;
    }
}

void shutdownErrorHandling() {
    std::lock_guard lock(global_error_handler_mutex);

    if (global_error_handler) {
        global_error_handler.reset();
        LOG_F(INFO, "WiFi error handling shutdown completed");
    }
}

} // namespace atom::system
