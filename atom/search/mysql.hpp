/**
 * @file mysql.hpp
 * @brief A high-performance, thread-safe MySQL/MariaDB client for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_MYSQL_HPP
#define ATOM_SEARCH_MYSQL_HPP

#include <mariadb/mysql.h>

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atom::database {

/**
 * @brief Custom exception for MySQL-related errors.
 */
class MySQLException : public std::runtime_error {
public:
    explicit MySQLException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Encapsulates parameters for a MySQL database connection.
 */
struct ConnectionParams {
    std::string host = "localhost";
    std::string user;
    std::string password;
    std::string database;
    unsigned int port = 3306;
    std::string socket;
    unsigned long client_flag = 0;
    unsigned int connect_timeout = 10;
    bool auto_reconnect = true;
    std::string charset = "utf8mb4";
};

class ResultSet;
class Transaction;

/**
 * @class MysqlDB
 * @brief A high-performance, thread-safe MySQL/MariaDB client using a
 * connection pool.
 *
 * This class provides a modern C++ interface for database operations, managing a
 * pool of connections to handle concurrent requests efficiently and scale on
 * multi-core systems.
 */
class MysqlDB {
public:
    /**
     * @brief Constructs a MysqlDB object and initializes the connection pool.
     * @param params The connection parameters.
     * @param pool_size The number of connections in the pool. Defaults to
     * hardware concurrency.
     * @throws MySQLException if the connection pool cannot be initialized.
     */
    explicit MysqlDB(const ConnectionParams& params, unsigned int pool_size = 0);

    ~MysqlDB();

    MysqlDB(const MysqlDB&) = delete;
    MysqlDB& operator=(const MysqlDB&) = delete;
    MysqlDB(MysqlDB&&) = delete;
    MysqlDB& operator=(MysqlDB&&) = delete;

    /**
     * @brief Executes a query that does not return a result set (e.g., INSERT,
     * UPDATE, DELETE).
     * @param query The SQL query to execute.
     * @return The number of affected rows.
     * @throws MySQLException on failure.
     */
    uint64_t execute(std::string_view query);

    /**
     * @brief Executes a parameterized query that does not return a result set.
     * @tparam Args The types of the parameters.
     * @param query The SQL query with '?' placeholders.
     * @param params The parameters to bind.
     * @return The number of affected rows.
     * @throws MySQLException on failure.
     */
    template <typename... Args>
    uint64_t execute(std::string_view query, Args&&... params);

    /**
     * @brief Executes a query that returns a result set (e.g., SELECT).
     * @param query The SQL query to execute.
     * @return A unique_ptr to a ResultSet.
     * @throws MySQLException on failure.
     */
    [[nodiscard]] std::unique_ptr<ResultSet> query(std::string_view query);

    /**
     * @brief Executes a parameterized query that returns a result set.
     * @tparam Args The types of the parameters.
     * @param query The SQL query with '?' placeholders.
     * @param params The parameters to bind.
     * @return A unique_ptr to a ResultSet.
     * @throws MySQLException on failure.
     */
    template <typename... Args>
    [[nodiscard]] std::unique_ptr<ResultSet> query(std::string_view query,
                                                   Args&&... params);

    /**
     * @brief Begins a transaction.
     * @return A Transaction object that manages the transaction's lifetime.
     * @throws MySQLException on failure.
     */
    [[nodiscard]] std::unique_ptr<Transaction> begin_transaction();

    /**
     * @brief Executes a function within a transaction.
     * @param func The function to execute. It receives a reference to the
     * transaction connection.
     * @throws MySQLException on failure, after rolling back.
     */
    void with_transaction(const std::function<void(MYSQL&)>& func);

    /**
     * @brief Escapes a string to be safely used in a SQL query.
     * @param str The string to escape.
     * @return The escaped string.
     */
    [[nodiscard]] std::string escape(std::string_view str);

    /**
     * @brief Pings the database server to check if the connections are alive.
     * @return true if the connections are alive, false otherwise.
     */
    bool ping();

private:
    class Impl;
    std::unique_ptr<Impl> p_impl_;
};

/**
 * @class ResultSet
 * @brief Represents the result of a MySQL query.
 *
 * Provides an interface to iterate over rows and access column data.
 * This class is not thread-safe and should be used within a single thread.
 */
class ResultSet {
public:
    ~ResultSet();

    ResultSet(const ResultSet&) = delete;
    ResultSet& operator=(const ResultSet&) = delete;
    ResultSet(ResultSet&&) noexcept;
    ResultSet& operator=(ResultSet&&) noexcept;

    /**
     * @brief Advances to the next row in the result set.
     * @return true if another row is available, false otherwise.
     */
    bool next();

    [[nodiscard]] std::string_view get_string(unsigned int index) const;
    [[nodiscard]] int get_int(unsigned int index) const;
    [[nodiscard]] int64_t get_int64(unsigned int index) const;
    [[nodiscard]] double get_double(unsigned int index) const;
    [[nodiscard]] bool is_null(unsigned int index) const;

    [[nodiscard]] unsigned int get_field_count() const;
    [[nodiscard]] uint64_t get_row_count() const;

private:
    friend class MysqlDB;
    explicit ResultSet(MYSQL_RES* result);

    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> result_;
    MYSQL_ROW current_row_ = nullptr;
    unsigned long* current_lengths_ = nullptr;
    unsigned int field_count_ = 0;
};

/**
 * @class Transaction
 * @brief A RAII guard for managing database transactions.
 *
 * Commits the transaction on successful destruction, rolls back on exception or
 * explicit call to rollback().
 */
class Transaction {
public:
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) noexcept;
    Transaction& operator=(Transaction&&) noexcept;

    /**
     * @brief Commits the transaction.
     */
    void commit();

    /**
     * @brief Rolls back the transaction.
     */
    void rollback();

    /**
     * @brief Gets the underlying MYSQL connection for this transaction.
     * @return A pointer to the MYSQL connection.
     */
    MYSQL* get_connection();

private:
    friend class MysqlDB;
    explicit Transaction(std::unique_ptr<MYSQL, std::function<void(MYSQL*)>> conn);

    std::unique_ptr<MYSQL, std::function<void(MYSQL*)>> conn_;
    bool committed_or_rolled_back_ = false;
};

}  // namespace database
}  // namespace atom

#endif  // ATOM_SEARCH_MYSQL_HPP