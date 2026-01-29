/*
 * base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file base.hpp
 * @brief Base class and common utilities for database implementations.
 */

#ifndef ATOM_SEARCH_DATABASE_BASE_HPP
#define ATOM_SEARCH_DATABASE_BASE_HPP

#include <cctype>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

#include "retry.hpp"
#include "types.hpp"

namespace atom::search::database {

/**
 * @brief Base class for database implementations.
 *
 * Provides common functionality like error handling, retry logic,
 * and transaction management.
 */
class DatabaseBase : public IDatabase {
public:
    using ErrorCallback = std::function<void(std::string_view, int)>;

    virtual ~DatabaseBase() = default;

    /**
     * @brief Set custom error callback.
     */
    void setErrorCallback(ErrorCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        errorCallback_ = std::move(callback);
    }

    /**
     * @brief Get retry policy.
     */
    [[nodiscard]] const RetryPolicy& getRetryPolicy() const noexcept {
        return retryPolicy_;
    }

    /**
     * @brief Set retry policy.
     */
    void setRetryPolicy(RetryPolicy policy) {
        retryPolicy_ = std::move(policy);
    }

    /**
     * @brief Execute operations within a transaction with automatic rollback.
     */
    void withTransaction(const std::function<void()>& operations) {
        beginTransaction();
        try {
            operations();
            commit();
        } catch (...) {
            try {
                rollback();
            } catch (const std::exception& e) {
                spdlog::critical("Failed to rollback transaction: {}",
                                 e.what());
            }
            throw;
        }
    }

    /**
     * @brief Execute with retry logic.
     */
    template <typename Func>
    auto executeWithRetry(Func&& operation) {
        RetryExecutor executor(retryPolicy_);
        return executor.executeOrThrow(std::forward<Func>(operation));
    }

protected:
    DatabaseBase() = default;

    /**
     * @brief Report an error through the callback and optionally throw.
     */
    void reportError(std::string_view message, int code = 0,
                     bool shouldThrow = false) {
        spdlog::error("Database error [{}]: {}", code, message);

        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (errorCallback_) {
                errorCallback_(message, code);
            }
        }

        if (shouldThrow) {
            throw DatabaseException(message, code);
        }
    }

    /**
     * @brief Log a debug message.
     */
    static void logDebug(std::string_view message) {
        spdlog::debug("{}", message);
    }

    /**
     * @brief Log an info message.
     */
    static void logInfo(std::string_view message) {
        spdlog::info("{}", message);
    }

    /**
     * @brief Log a warning message.
     */
    static void logWarn(std::string_view message) {
        spdlog::warn("{}", message);
    }

    RetryPolicy retryPolicy_;

private:
    ErrorCallback errorCallback_;
    mutable std::mutex callbackMutex_;
};

/**
 * @brief Mixin for thread-safe database operations.
 */
template <typename Derived>
class ThreadSafeMixin {
protected:
    /**
     * @brief Execute a read operation with shared lock.
     */
    template <typename Func>
    auto withReadLock(Func&& operation) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return operation();
    }

    /**
     * @brief Execute a write operation with exclusive lock.
     */
    template <typename Func>
    auto withWriteLock(Func&& operation) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return operation();
    }

    mutable std::shared_mutex mutex_;
};

/**
 * @brief Helper for escaping SQL strings.
 */
class SqlEscaper {
public:
    /**
     * @brief Escape a string for safe SQL usage (basic escaping).
     * @note For production use, prefer prepared statements.
     */
    static std::string escape(std::string_view input) {
        std::string result;
        result.reserve(input.size() * 2);

        for (char c : input) {
            switch (c) {
                case '\'':
                    result += "''";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\0':
                    result += "\\0";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\x1a':
                    result += "\\Z";
                    break;
                default:
                    result += c;
                    break;
            }
        }

        return result;
    }

    /**
     * @brief Quote and escape a string value.
     */
    static std::string quote(std::string_view input) {
        return "'" + escape(input) + "'";
    }

    /**
     * @brief Quote an identifier (table/column name).
     */
    static std::string quoteIdentifier(std::string_view identifier,
                                       char quoteChar = '"') {
        std::string result;
        result += quoteChar;
        for (char c : identifier) {
            if (c == quoteChar) {
                result += quoteChar;
                result += quoteChar;
            } else {
                result += c;
            }
        }
        result += quoteChar;
        return result;
    }
};

/**
 * @brief Query validation utilities.
 */
class QueryValidator {
public:
    /**
     * @brief Validate a query string for basic security checks.
     * @throws DatabaseException if query is invalid.
     */
    static void validate(std::string_view query) {
        if (query.empty()) {
            throw QueryException("Query string cannot be empty");
        }

        // Check for SQL comment injection
        if (query.find("--") != std::string_view::npos) {
            spdlog::warn("Query contains '--' comment marker: {}",
                         query.substr(0, 100));
        }

        // Check for multiple statements
        size_t semicolonPos = query.find(';');
        if (semicolonPos != std::string_view::npos &&
            semicolonPos < query.size() - 1) {
            // Check if there's non-whitespace after semicolon
            auto remaining = query.substr(semicolonPos + 1);
            for (char c : remaining) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    throw QueryException(
                        "Multiple SQL statements are not allowed");
                }
            }
        }
    }

    /**
     * @brief Check if query is a SELECT statement.
     */
    static bool isSelect(std::string_view query) {
        auto trimmed = trimLeft(query);
        return startsWithIgnoreCase(trimmed, "SELECT");
    }

    /**
     * @brief Check if query is a modification statement (INSERT/UPDATE/DELETE).
     */
    static bool isModification(std::string_view query) {
        auto trimmed = trimLeft(query);
        return startsWithIgnoreCase(trimmed, "INSERT") ||
               startsWithIgnoreCase(trimmed, "UPDATE") ||
               startsWithIgnoreCase(trimmed, "DELETE");
    }

private:
    static std::string_view trimLeft(std::string_view str) {
        size_t start = 0;
        while (start < str.size() &&
               std::isspace(static_cast<unsigned char>(str[start]))) {
            ++start;
        }
        return str.substr(start);
    }

    static bool startsWithIgnoreCase(std::string_view str,
                                     std::string_view prefix) {
        if (str.size() < prefix.size())
            return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(str[i])) !=
                std::toupper(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace atom::search::database

#endif  // ATOM_SEARCH_DATABASE_BASE_HPP
