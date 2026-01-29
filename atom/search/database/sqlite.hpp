/*
 * sqlite.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file sqlite.hpp
 * @brief Thread-safe SQLite database wrapper with advanced features.
 * @details Provides a high-level interface for SQLite database operations
 *          including prepared statement caching, transaction management,
 *          and thread safety using the Pimpl design pattern.
 */

#ifndef ATOM_SEARCH_DATABASE_SQLITE_HPP
#define ATOM_SEARCH_DATABASE_SQLITE_HPP

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string_view>

#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include "base.hpp"
#include "statement_cache.hpp"
#include "types.hpp"

namespace atom::search {

// Import types from database namespace
using database::DatabaseException;
using database::ResultSet;
using database::RowData;
using database::String;
using database::Vector;

// Legacy alias for backward compatibility
using SQLiteException = database::DatabaseException;

/**
 * @class SqliteDB
 * @brief A thread-safe SQLite database wrapper with advanced features
 *
 * This class provides a high-level interface for SQLite database operations
 * including prepared statement caching, transaction management, and thread
 * safety. It uses the Pimpl design pattern for implementation hiding and better
 * compilation times.
 *
 * Features:
 * - Thread-safe operations with shared_mutex
 * - Prepared statement caching for performance
 * - Transaction support with RAII guards
 * - Parameterized queries for SQL injection prevention
 * - Pagination support
 * - Table schema introspection
 */
class SqliteDB : public database::ThreadSafeMixin<SqliteDB> {
public:
    /**
     * @brief Type alias for a single row of query results
     */
    using RowData = Vector<String>;

    /**
     * @brief Type alias for complete query result sets
     */
    using ResultSet = Vector<RowData>;

    /**
     * @brief Construct a new SqliteDB object
     *
     * @param dbPath Path to the SQLite database file
     * @throws SQLiteException if the database cannot be opened
     */
    explicit SqliteDB(std::string_view dbPath);

    /**
     * @brief Destroy the SqliteDB object
     *
     * Automatically closes the database connection and cleans up resources.
     */
    ~SqliteDB();

    SqliteDB(const SqliteDB&) = delete;
    SqliteDB& operator=(const SqliteDB&) = delete;

    /**
     * @brief Move constructor
     *
     * @param other Source object to move from
     */
    SqliteDB(SqliteDB&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other Source object to move from
     * @return SqliteDB& Reference to this object
     */
    SqliteDB& operator=(SqliteDB&& other) noexcept;

    /**
     * @brief Execute a simple SQL query without parameters
     *
     * @param query SQL query string to execute
     * @return true if execution was successful
     * @throws SQLiteException on execution error
     */
    [[nodiscard]] bool executeQuery(std::string_view query);

    /**
     * @brief Execute a parameterized SQL query with bound values
     *
     * This method uses prepared statements for security and performance.
     * Parameters are automatically bound based on their types.
     *
     * @tparam Args Parameter types to bind
     * @param query SQL query with placeholders (?)
     * @param params Parameters to bind to the query
     * @return true if execution was successful
     * @throws SQLiteException on execution error
     */
    template <typename... Args>
    [[nodiscard]] bool executeParameterizedQuery(std::string_view query,
                                                 Args&&... params);

    /**
     * @brief Execute a SELECT query and return all results
     *
     * @param query SQL SELECT query string
     * @return ResultSet containing all rows from the query
     * @throws SQLiteException on query error
     */
    [[nodiscard]] ResultSet selectData(std::string_view query);

    /**
     * @brief Execute a parameterized SELECT query and return results
     *
     * @tparam Args Parameter types to bind
     * @param query SQL SELECT query with placeholders
     * @param params Parameters to bind to the query
     * @return ResultSet containing all matching rows
     * @throws SQLiteException on query error
     */
    template <typename... Args>
    [[nodiscard]] ResultSet selectParameterizedData(std::string_view query,
                                                    Args&&... params);

    /**
     * @brief Helper function to retrieve a single value of any type
     *
     * @tparam T Type of value to retrieve
     * @param query SQL query that returns a single value
     * @param columnFunc Function to extract value from SQLite column
     * @return Optional value (empty if query fails or result is NULL)
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getSingleValue(std::string_view query,
                                                  T (*columnFunc)(sqlite3_stmt*,
                                                                  int));

    /**
     * @brief Retrieve a single integer value from a query
     *
     * @param query SQL query that returns a single integer
     * @return Optional integer value
     */
    [[nodiscard]] std::optional<int> getIntValue(std::string_view query);

    /**
     * @brief Retrieve a single floating-point value from a query
     *
     * @param query SQL query that returns a single double
     * @return Optional double value
     */
    [[nodiscard]] std::optional<double> getDoubleValue(std::string_view query);

    /**
     * @brief Retrieve a single text value from a query
     *
     * @param query SQL query that returns a single text value
     * @return Optional String value
     */
    [[nodiscard]] std::optional<String> getTextValue(std::string_view query);

    /**
     * @brief Search for data matching a specific term
     *
     * @param query SQL query with a single parameter placeholder
     * @param searchTerm Term to search for
     * @return true if matching data was found
     */
    [[nodiscard]] bool searchData(std::string_view query,
                                  std::string_view searchTerm);

    /**
     * @brief Execute an UPDATE statement and return affected row count
     *
     * @param query SQL UPDATE statement
     * @return Number of rows affected by the update
     * @throws SQLiteException on update error
     */
    [[nodiscard]] int updateData(std::string_view query);

    /**
     * @brief Execute a DELETE statement and return affected row count
     *
     * @param query SQL DELETE statement
     * @return Number of rows affected by the delete
     * @throws SQLiteException on delete error
     */
    [[nodiscard]] int deleteData(std::string_view query);

    /**
     * @brief Begin a database transaction
     *
     * Uses IMMEDIATE transaction mode for better concurrency control.
     *
     * @throws SQLiteException if transaction cannot be started
     */
    void beginTransaction();

    /**
     * @brief Commit the current transaction
     *
     * @throws SQLiteException if transaction cannot be committed
     */
    void commitTransaction();

    /**
     * @brief Rollback the current transaction
     *
     * This method does not throw exceptions to ensure it can be safely
     * called from destructors and error handlers.
     */
    void rollbackTransaction();

    /**
     * @brief Execute operations within a transaction with automatic rollback
     *
     * Automatically begins a transaction, executes the provided operations,
     * and commits. If any exception occurs, the transaction is rolled back.
     *
     * @param operations Function containing database operations to execute
     * @throws Re-throws any exceptions from operations after rollback
     */
    void withTransaction(const std::function<void()>& operations);

    /**
     * @brief Validate data using a validation query
     *
     * Executes the main query, then runs a validation query to check
     * if the operation was successful.
     *
     * @param query Main SQL query to execute
     * @param validationQuery Query that should return non-zero for success
     * @return true if validation passes
     */
    [[nodiscard]] bool validateData(std::string_view query,
                                    std::string_view validationQuery);

    /**
     * @brief Execute a SELECT query with pagination
     *
     * @param query Base SQL SELECT query (without LIMIT/OFFSET)
     * @param limit Maximum number of rows to return
     * @param offset Number of rows to skip
     * @return ResultSet containing the paginated results
     * @throws SQLiteException on query error or invalid parameters
     */
    [[nodiscard]] ResultSet selectDataWithPagination(std::string_view query,
                                                     int limit, int offset);

    /**
     * @brief Set a custom error message callback
     *
     * @param errorCallback Function to call when errors occur
     */
    void setErrorMessageCallback(
        const std::function<void(std::string_view)>& errorCallback);

    /**
     * @brief Check if the database connection is active
     *
     * @return true if connected to a database
     */
    [[nodiscard]] bool isConnected() const noexcept;

    /**
     * @brief Get the rowid of the last inserted row
     *
     * @return Row ID of the last insert operation
     * @throws SQLiteException if not connected
     */
    [[nodiscard]] int64_t getLastInsertRowId() const;

    /**
     * @brief Get the number of rows modified by the last statement
     *
     * @return Number of rows affected by the last INSERT/UPDATE/DELETE
     * @throws SQLiteException if not connected
     */
    [[nodiscard]] int getChanges() const;

    /**
     * @brief Get the total number of rows modified since database opened
     *
     * @return Total number of rows modified
     * @throws SQLiteException if not connected
     */
    [[nodiscard]] int getTotalChanges() const;

    /**
     * @brief Check if a table exists in the database
     *
     * @param tableName Name of the table to check
     * @return true if the table exists
     */
    [[nodiscard]] bool tableExists(std::string_view tableName);

    /**
     * @brief Get the schema information for a table
     *
     * @param tableName Name of the table
     * @return ResultSet containing column information
     */
    [[nodiscard]] ResultSet getTableSchema(std::string_view tableName);

    /**
     * @brief Execute VACUUM command to optimize database
     *
     * @return true if VACUUM was successful
     */
    [[nodiscard]] bool vacuum();

    /**
     * @brief Execute ANALYZE command to update query planner statistics
     *
     * @return true if ANALYZE was successful
     */
    [[nodiscard]] bool analyze();

    /**
     * @brief Get list of all tables in the database.
     * @return Vector of table names.
     */
    [[nodiscard]] std::vector<String> getTables();

    /**
     * @brief Get list of columns for a table.
     * @param tableName Name of the table.
     * @return Vector of column names.
     */
    [[nodiscard]] std::vector<String> getColumns(std::string_view tableName);

    /**
     * @brief Execute a batch of queries.
     * @param queries Vector of SQL queries to execute.
     * @return true if all queries succeeded.
     */
    [[nodiscard]] bool executeBatch(const std::vector<std::string>& queries);

    /**
     * @brief Execute a batch of queries within a transaction.
     * @param queries Vector of SQL queries to execute.
     * @return true if all queries succeeded, false if any failed (rolled back).
     */
    [[nodiscard]] bool executeBatchTransaction(
        const std::vector<std::string>& queries);

    /**
     * @brief Get SQLite version string.
     * @return SQLite library version.
     */
    [[nodiscard]] static std::string getVersion();

    /**
     * @brief Check database integrity.
     * @return true if database passes integrity check.
     */
    [[nodiscard]] bool integrityCheck();

    /**
     * @brief Backup database to a file.
     * @param destPath Destination file path.
     * @return true if backup succeeded.
     */
    [[nodiscard]] bool backup(std::string_view destPath);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    mutable std::shared_mutex mtx;

    /**
     * @brief Validate query string for basic security checks
     *
     * @param query Query string to validate
     * @throws SQLiteException if query is invalid
     */
    void validateQueryString(std::string_view query) const;

    /**
     * @brief Check database connection before operations
     *
     * @throws SQLiteException if database is not connected
     */
    void checkConnection() const;

    /**
     * @brief Helper for update/delete operations
     *
     * @param query SQL statement to execute
     * @return Number of rows affected
     * @throws SQLiteException on error
     */
    [[nodiscard]] int executeAndGetChanges(std::string_view query);

#if defined(TEST_F)
    friend class SqliteDBTest;
#endif
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_DATABASE_SQLITE_HPP
