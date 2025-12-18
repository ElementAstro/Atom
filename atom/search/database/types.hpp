/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Common types and abstractions for database components.
 */

#ifndef ATOM_SEARCH_DATABASE_TYPES_HPP
#define ATOM_SEARCH_DATABASE_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "atom/containers/high_performance.hpp"

namespace atom::search::database {

using atom::containers::String;
using atom::containers::Vector;

// =============================================================================
// Common Type Aliases
// =============================================================================

/**
 * @brief Type alias for a single row of query results.
 */
using RowData = Vector<String>;

/**
 * @brief Type alias for complete query result sets.
 */
using ResultSet = Vector<RowData>;

/**
 * @brief Type alias for parameter values in prepared statements.
 */
using ParamValue = std::variant<std::nullptr_t, int, int64_t, double,
                                std::string, std::vector<uint8_t>>;

/**
 * @brief Type alias for a list of parameters.
 */
using ParamList = std::vector<ParamValue>;

// =============================================================================
// Exceptions
// =============================================================================

/**
 * @brief Base exception class for database operations.
 */
class DatabaseException : public std::exception {
protected:
    String message_;
    int errorCode_{0};

public:
    explicit DatabaseException(std::string_view msg, int code = 0)
        : message_(msg), errorCode_(code) {}

    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

    [[nodiscard]] int errorCode() const noexcept { return errorCode_; }
};

/**
 * @brief Exception for connection errors.
 */
class ConnectionException : public DatabaseException {
public:
    explicit ConnectionException(std::string_view msg, int code = 0)
        : DatabaseException(
              std::string("Connection error: ") + std::string(msg), code) {}
};

/**
 * @brief Exception for query execution errors.
 */
class QueryException : public DatabaseException {
public:
    explicit QueryException(std::string_view msg, int code = 0)
        : DatabaseException(std::string("Query error: ") + std::string(msg),
                            code) {}
};

/**
 * @brief Exception for transaction errors.
 */
class TransactionException : public DatabaseException {
public:
    explicit TransactionException(std::string_view msg, int code = 0)
        : DatabaseException(
              std::string("Transaction error: ") + std::string(msg), code) {}
};

// Legacy alias for backward compatibility
using SQLiteException = DatabaseException;

// =============================================================================
// Configuration
// =============================================================================

/**
 * @brief Database connection configuration.
 */
struct DatabaseConfig {
    std::string host;
    uint16_t port{0};
    std::string database;
    std::string username;
    std::string password;
    std::string charset{"utf8mb4"};

    // Connection pool settings
    size_t minConnections{1};
    size_t maxConnections{10};
    std::chrono::seconds connectionTimeout{30};
    std::chrono::seconds idleTimeout{300};

    // Query settings
    bool autoCommit{true};
    size_t maxRetries{3};
    std::chrono::milliseconds retryDelay{100};
};

/**
 * @brief Transaction isolation levels.
 */
enum class IsolationLevel {
    ReadUncommitted,
    ReadCommitted,
    RepeatableRead,
    Serializable
};

/**
 * @brief Query result status.
 */
enum class QueryStatus { Success, NoData, Error, Timeout };

// =============================================================================
// Interfaces
// =============================================================================

/**
 * @brief Interface for database connections.
 */
class IDatabase {
public:
    virtual ~IDatabase() = default;

    // Connection management
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
    virtual void connect() = 0;
    virtual void disconnect() noexcept = 0;

    // Query execution
    [[nodiscard]] virtual bool executeQuery(std::string_view query) = 0;
    [[nodiscard]] virtual ResultSet selectData(std::string_view query) = 0;

    // Transaction management
    virtual void beginTransaction() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;
};

/**
 * @brief Extended interface with additional features.
 */
class IDatabaseExtended : public IDatabase {
public:
    // Single value retrieval
    [[nodiscard]] virtual std::optional<int> getIntValue(
        std::string_view query) = 0;
    [[nodiscard]] virtual std::optional<double> getDoubleValue(
        std::string_view query) = 0;
    [[nodiscard]] virtual std::optional<String> getTextValue(
        std::string_view query) = 0;

    // Modification operations
    [[nodiscard]] virtual int updateData(std::string_view query) = 0;
    [[nodiscard]] virtual int deleteData(std::string_view query) = 0;

    // Metadata
    [[nodiscard]] virtual bool tableExists(std::string_view tableName) = 0;
    [[nodiscard]] virtual int64_t getLastInsertId() const = 0;
    [[nodiscard]] virtual int getChanges() const = 0;
};

/**
 * @brief Interface for prepared statement support.
 */
class IPreparedStatement {
public:
    virtual ~IPreparedStatement() = default;

    virtual IPreparedStatement& bindNull(int index) = 0;
    virtual IPreparedStatement& bindInt(int index, int value) = 0;
    virtual IPreparedStatement& bindInt64(int index, int64_t value) = 0;
    virtual IPreparedStatement& bindDouble(int index, double value) = 0;
    virtual IPreparedStatement& bindString(int index,
                                           std::string_view value) = 0;
    virtual IPreparedStatement& bindBlob(int index,
                                         const std::vector<uint8_t>& value) = 0;

    [[nodiscard]] virtual bool execute() = 0;
    [[nodiscard]] virtual int executeUpdate() = 0;
    virtual void reset() = 0;
    virtual void clearBindings() = 0;

    [[nodiscard]] virtual unsigned int getParameterCount() const = 0;
};

/**
 * @brief Interface for databases supporting prepared statements.
 */
class IPreparedStatementSupport {
public:
    virtual ~IPreparedStatementSupport() = default;

    [[nodiscard]] virtual std::unique_ptr<IPreparedStatement> prepare(
        std::string_view query) = 0;
};

/**
 * @brief RAII transaction guard.
 */
template <typename DB>
class TransactionGuard {
public:
    explicit TransactionGuard(DB& db) : db_(db), committed_(false) {
        db_.beginTransaction();
    }

    ~TransactionGuard() {
        if (!committed_) {
            try {
                db_.rollback();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;

    void commit() {
        db_.commit();
        committed_ = true;
    }

private:
    DB& db_;
    bool committed_;
};

}  // namespace atom::search::database

// Expose in atom::search namespace for backward compatibility
namespace atom::search {
using database::ConnectionException;
using database::DatabaseConfig;
using database::DatabaseException;
using database::IsolationLevel;
using database::QueryException;
using database::QueryStatus;
using database::ResultSet;
using database::RowData;
using database::SQLiteException;
using database::TransactionException;
using database::TransactionGuard;
}  // namespace atom::search

#endif  // ATOM_SEARCH_DATABASE_TYPES_HPP
