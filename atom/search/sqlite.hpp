/*
 * sqlite.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SEARCH_SQLITE_HPP
#define ATOM_SEARCH_SQLITE_HPP

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Custom exception class for SQLite operations
 */
class SQLiteException : public std::exception {
private:
    std::string message;

public:
    explicit SQLiteException(std::string_view msg) : message(msg) {}
    [[nodiscard]] const char* what() const noexcept override {
        return message.c_str();
    }
};

/**
 * @class SqliteDB
 * @brief A thread-safe class for managing SQLite database operations using the
 * Pimpl design pattern.
 */
class SqliteDB {
public:
    /**
     * @brief Represents a row of data from a query result
     */
    using RowData = std::vector<std::string>;

    /**
     * @brief Represents a complete result set from a query
     */
    using ResultSet = std::vector<RowData>;

    /**
     * @brief Constructor
     * @param dbPath Path to the database file
     * @throws SQLiteException if database cannot be opened
     */
    explicit SqliteDB(std::string_view dbPath);

    /**
     * @brief Destructor
     */
    ~SqliteDB();

    /**
     * @brief SqliteDB is non-copyable
     */
    SqliteDB(const SqliteDB&) = delete;
    SqliteDB& operator=(const SqliteDB&) = delete;

    /**
     * @brief SqliteDB is movable
     */
    SqliteDB(SqliteDB&&) noexcept;
    SqliteDB& operator=(SqliteDB&&) noexcept;

    /**
     * @brief Execute a SQL query
     * @param query SQL query string
     * @return Whether the query was executed successfully
     * @throws SQLiteException on execution error
     */
    [[nodiscard]] bool executeQuery(std::string_view query);

    /**
     * @brief Execute a parameterized query with binding values
     * @param query SQL query with placeholders
     * @param params Parameters to bind to the query
     * @return Whether the query was executed successfully
     * @throws SQLiteException on execution error
     */
    template <typename... Args>
    [[nodiscard]] bool executeParameterizedQuery(std::string_view query,
                                                 Args&&... params);

    /**
     * @brief Query and retrieve data
     * @param query SQL query string
     * @return Result set containing all rows from the query
     * @throws SQLiteException on query error
     */
    [[nodiscard]] ResultSet selectData(std::string_view query);

    /**
     * @brief Retrieve an integer value
     * @param query SQL query string
     * @return Optional integer value (empty if query fails)
     * @throws SQLiteException on serious errors
     */
    [[nodiscard]] std::optional<int> getIntValue(std::string_view query);

    /**
     * @brief Retrieve a floating-point value
     * @param query SQL query string
     * @return Optional double value (empty if query fails)
     * @throws SQLiteException on serious errors
     */
    [[nodiscard]] std::optional<double> getDoubleValue(std::string_view query);

    /**
     * @brief Retrieve a text value
     * @param query SQL query string
     * @return Optional text value (empty if query fails)
     * @throws SQLiteException on serious errors
     */
    [[nodiscard]] std::optional<std::string> getTextValue(
        std::string_view query);

    /**
     * @brief Search for a specific item in the query results
     * @param query SQL query string
     * @param searchTerm Term to search for
     * @return Whether a matching item was found
     * @throws SQLiteException on serious errors
     */
    [[nodiscard]] bool searchData(std::string_view query,
                                  std::string_view searchTerm);

    /**
     * @brief Update data in the database
     * @param query SQL update statement
     * @return Number of rows affected by the update
     * @throws SQLiteException on update error
     */
    [[nodiscard]] int updateData(std::string_view query);

    /**
     * @brief Delete data from the database
     * @param query SQL delete statement
     * @return Number of rows affected by the delete
     * @throws SQLiteException on delete error
     */
    [[nodiscard]] int deleteData(std::string_view query);

    /**
     * @brief Begin a database transaction
     * @throws SQLiteException if transaction cannot be started
     */
    void beginTransaction();

    /**
     * @brief Commit a database transaction
     * @throws SQLiteException if transaction cannot be committed
     */
    void commitTransaction();

    /**
     * @brief Rollback a database transaction
     * @throws SQLiteException if transaction cannot be rolled back
     */
    void rollbackTransaction();

    /**
     * @brief Execute operations within a transaction
     * @param operations Function containing database operations to execute
     * @throws Re-throws any exceptions from operations after rollback
     */
    void withTransaction(const std::function<void()>& operations);

    /**
     * @brief Validate data against a specified query condition
     * @param query SQL query string
     * @param validationQuery Validation condition query string
     * @return Validation result
     * @throws SQLiteException on validation error
     */
    [[nodiscard]] bool validateData(std::string_view query,
                                    std::string_view validationQuery);

    /**
     * @brief Perform paginated data query and retrieval
     * @param query SQL query string
     * @param limit Number of records per page
     * @param offset Offset for pagination
     * @return Result set containing the paginated rows
     * @throws SQLiteException on query error or invalid parameters
     */
    [[nodiscard]] ResultSet selectDataWithPagination(std::string_view query,
                                                     int limit, int offset);

    /**
     * @brief Set an error message callback function
     * @param errorCallback Error message callback function
     */
    void setErrorMessageCallback(
        const std::function<void(std::string_view)>& errorCallback);

    /**
     * @brief Check if the database connection is valid
     * @return true if connected, false otherwise
     */
    [[nodiscard]] bool isConnected() const noexcept;

    /**
     * @brief Get the last insert rowid
     * @return The rowid of the last inserted row
     */
    [[nodiscard]] int64_t getLastInsertRowId() const;

    /**
     * @brief Get the number of rows modified by the last query
     * @return The number of rows modified
     */
    [[nodiscard]] int getChanges() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl; /**< Pointer to implementation */
    mutable std::shared_mutex
        mtx; /**< Shared mutex for read/write operations */

    /**
     * @brief Validate query string
     * @param query Query string to validate
     * @throws SQLiteException if query is invalid
     */
    void validateQueryString(std::string_view query) const;

    /**
     * @brief Check database connection before operations
     * @throws SQLiteException if database is not connected
     */
    void checkConnection() const;

#if defined(TEST_F)
    // Allow Mock class to access private members for testing
    friend class SqliteDBTest;
#endif
};

#endif  // ATOM_SEARCH_SQLITE_HPP