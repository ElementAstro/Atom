/**
 * @file sqlite.hpp
 * @brief A high-performance, thread-safe SQLite database wrapper for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_SQLITE_HPP
#define ATOM_SEARCH_SQLITE_HPP

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

#include <spdlog/spdlog.h>

#include "atom/containers/high_performance.hpp"

namespace atom::search {

using atom::containers::String;
using atom::containers::Vector;

/**
 * @brief Custom exception class for SQLite-related errors.
 *
 * This exception is thrown when a SQLite operation fails, providing a detailed
 * error message.
 */
class SQLiteException : public std::runtime_error {
public:
    /**
     * @brief Constructs a new SQLiteException.
     * @param msg The error message.
     */
    explicit SQLiteException(const std::string& msg) : std::runtime_error(msg) {}
};

class TransactionContext;

/**
 * @class SqliteDB
 * @brief A thread-safe SQLite database wrapper featuring a connection pool.
 *
 * This class provides a high-level interface for SQLite database operations,
 * using a connection pool to manage concurrent access, which enhances
 * performance and scalability on multi-core architectures. It is designed for
 * safety and efficiency, with support for transactions and prepared statements.
 */
class SqliteDB {
public:
    using RowData = Vector<String>;
    using ResultSet = Vector<RowData>;

    /**
     * @brief Constructs a new SqliteDB object and initializes the connection
     * pool.
     *
     * @param db_path Path to the SQLite database file.
     * @param pool_size The number of connections in the pool. If 0, it defaults
     * to the hardware concurrency.
     * @throws SQLiteException if the database cannot be opened.
     */
    explicit SqliteDB(std::string_view db_path, unsigned int pool_size = 0);

    /**
     * @brief Destroys the SqliteDB object, closing all database connections.
     */
    ~SqliteDB();

    SqliteDB(const SqliteDB&) = delete;
    SqliteDB& operator=(const SqliteDB&) = delete;
    SqliteDB(SqliteDB&&) = delete;
    SqliteDB& operator=(SqliteDB&&) = delete;

    /**
     * @brief Executes a simple SQL query without parameters.
     *
     * @param query The SQL query string to execute.
     * @throws SQLiteException on execution error.
     */
    void execute_query(std::string_view query);

    /**
     * @brief Executes a parameterized SQL query with bound values.
     *
     * @tparam Args The types of the parameters to bind.
     * @param query The SQL query with '?' placeholders.
     * @param params The parameters to bind to the query.
     * @throws SQLiteException on execution error.
     */
    template <typename... Args>
    void execute_parameterized_query(std::string_view query, Args&&... params);

    /**
     * @brief Executes a SELECT query and returns all results.
     *
     * @param query The SQL SELECT query string.
     * @return A ResultSet containing all rows from the query.
     * @throws SQLiteException on query error.
     */
    [[nodiscard]] ResultSet select_data(std::string_view query);

    /**
     * @brief Executes a parameterized SELECT query and returns the results.
     *
     * @tparam Args The types of the parameters to bind.
     * @param query The SQL SELECT query with '?' placeholders.
     * @param params The parameters to bind to the query.
     * @return A ResultSet containing all matching rows.
     * @throws SQLiteException on query error.
     */
    template <typename... Args>
    [[nodiscard]] ResultSet select_parameterized_data(std::string_view query,
                                                      Args&&... params);

    /**
     * @brief Retrieves a single integer value from a query.
     *
     * @param query The SQL query that should return a single integer value.
     * @return An optional containing the integer value, or std::nullopt if no
     * result.
     */
    [[nodiscard]] std::optional<int> get_int_value(std::string_view query);

    /**
     * @brief Retrieves a single double value from a query.
     *
     * @param query The SQL query that should return a single double value.
     * @return An optional containing the double value, or std::nullopt if no
     * result.
     */
    [[nodiscard]] std::optional<double> get_double_value(std::string_view query);

    /**
     * @brief Retrieves a single text value from a query.
     *
     * @param query The SQL query that should return a single text value.
     * @return An optional containing the String value, or std::nullopt if no
     * result.
     */
    [[nodiscard]] std::optional<String> get_text_value(std::string_view query);

    /**
     * @brief Executes operations within a transaction.
     *
     * @param operations A function containing the database operations to execute
     * transactionally.
     * @throws SQLiteException if any operation fails, after rolling back.
     */
    void with_transaction(const std::function<void(TransactionContext&)>& operations);

    /**
     * @brief Checks if the database connection pool is active.
     *
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] bool is_connected() const noexcept;

    /**
     * @brief Gets the rowid of the last inserted row on the current thread's
     * connection.
     *
     * @return The row ID of the last insert operation.
     * @throws SQLiteException if not connected.
     */
    [[nodiscard]] int64_t get_last_insert_rowid() const;

    /**
     * @brief Checks if a table exists in the database.
     *
     * @param table_name The name of the table to check.
     * @return True if the table exists, false otherwise.
     */
    [[nodiscard]] bool table_exists(std::string_view table_name);

    /**
     * @brief Rebuilds and optimizes the database.
     *
     * @return True if VACUUM was successful, false otherwise.
     */
    [[nodiscard]] bool vacuum();

private:
    class Impl;
    std::unique_ptr<Impl> p_impl_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_SQLITE_HPP