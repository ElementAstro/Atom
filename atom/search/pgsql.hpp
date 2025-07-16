/**
 * @file pgsql.hpp
 * @brief A high-performance, thread-safe PostgreSQL client for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_PGSQL_HPP
#define ATOM_SEARCH_PGSQL_HPP

#include <libpq-fe.h>

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
 * @brief Custom exception for PostgreSQL-related errors.
 */
class PgSqlException : public std::runtime_error {
public:
    explicit PgSqlException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Encapsulates parameters for a PostgreSQL database connection.
 */
struct PgSqlConnectionParams {
    std::string host = "localhost";
    std::string user;
    std::string password;
    std::string dbname;
    unsigned int port = 5432;
    unsigned int connect_timeout = 10;
};

class PgSqlResultSet;
class PgSqlTransaction;
class PgSqlPipeline;

/**
 * @class PgSqlDB
 * @brief A high-performance, thread-safe PostgreSQL client using a connection
 * pool.
 *
 * This class provides a modern C++ interface for database operations, managing a
 * pool of connections to handle concurrent requests efficiently and scale on
 * multi-core systems.
 */
class PgSqlDB {
public:
    /**
     * @brief Constructs a PgSqlDB object and initializes the connection pool.
     * @param params The connection parameters.
     * @param pool_size The number of connections in the pool. Defaults to
     * hardware concurrency.
     * @throws PgSqlException if the connection pool cannot be initialized.
     */
    explicit PgSqlDB(const PgSqlConnectionParams& params,
                     unsigned int pool_size = 0);

    ~PgSqlDB();

    PgSqlDB(const PgSqlDB&) = delete;
    PgSqlDB& operator=(const PgSqlDB&) = delete;
    PgSqlDB(PgSqlDB&&) = delete;
    PgSqlDB& operator=(PgSqlDB&&) = delete;

    /**
     * @brief Executes a query that does not return a result set.
     * @param query The SQL query to execute.
     * @return The number of affected rows.
     * @throws PgSqlException on failure.
     */
    uint64_t execute(std::string_view query);

    /**
     * @brief Executes a parameterized query that does not return a result set.
     * @tparam Args The types of the parameters.
     * @param query The SQL query with $1, $2, etc. placeholders.
     * @param args The parameters to bind.
     * @return The number of affected rows.
     * @throws PgSqlException on failure.
     */
    template <typename... Args>
    uint64_t execute(std::string_view query, Args&&... args);

    /**
     * @brief Executes a query that returns a result set.
     * @param query The SQL query to execute.
     * @return A unique_ptr to a PgSqlResultSet.
     * @throws PgSqlException on failure.
     */
    [[nodiscard]] std::unique_ptr<PgSqlResultSet> query(std::string_view query);

    /**
     * @brief Executes a parameterized query that returns a result set.
     * @tparam Args The types of the parameters.
     * @param query The SQL query with $1, $2, etc. placeholders.
     * @param args The parameters to bind.
     * @return A unique_ptr to a PgSqlResultSet.
     * @throws PgSqlException on failure.
     */
    template <typename... Args>
    [[nodiscard]] std::unique_ptr<PgSqlResultSet> query(std::string_view query,
                                                        Args&&... args);

    /**
     * @brief Asynchronously executes a query.
     * @param query The SQL query to execute.
     * @return A future containing the number of affected rows.
     */
    [[nodiscard]] std::future<uint64_t> async_execute(std::string_view query);

    /**
     * @brief Asynchronously executes a query that returns a result set.
     * @param query The SQL query to execute.
     * @return A future containing a unique_ptr to a PgSqlResultSet.
     */
    [[nodiscard]] std::future<std::unique_ptr<PgSqlResultSet>> async_query(
        std::string_view query);

    /**
     * @brief Begins a transaction.
     * @return A PgSqlTransaction object that manages the transaction's lifetime.
     * @throws PgSqlException on failure.
     */
    [[nodiscard]] std::unique_ptr<PgSqlTransaction> begin_transaction();

    /**
     * @brief Executes a function within a transaction.
     * @param func The function to execute.
     * @throws PgSqlException on failure, after rolling back.
     */
    void with_transaction(const std::function<void(PgSqlDB&)>& func);

    /**
     * @brief Creates a pipeline for batching commands.
     * @return A unique_ptr to a PgSqlPipeline object.
     */
    [[nodiscard]] std::unique_ptr<PgSqlPipeline> pipeline();

    /**
     * @brief Executes a series of commands in a pipeline.
     * @param func A function that takes a PgSqlPipeline reference.
     * @return A vector of result sets for each command in the pipeline.
     */
    std::vector<std::unique_ptr<PgSqlResultSet>> with_pipeline(
        const std::function<void(PgSqlPipeline&)>& func);

    /**
     * @brief Escapes a string literal for use in a SQL query.
     * @param str The string to escape.
     * @return The escaped string, including single quotes.
     */
    [[nodiscard]] std::string escape_literal(std::string_view str);

    /**
     * @brief Checks if a table exists in the database.
     * @param table_name The name of the table.
     * @return true if the table exists, false otherwise.
     */
    [[nodiscard]] bool table_exists(std::string_view table_name);

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
 * @class PgSqlResultSet
 * @brief Represents the result of a PostgreSQL query.
 */
class PgSqlResultSet {
public:
    ~PgSqlResultSet();

    PgSqlResultSet(const PgSqlResultSet&) = delete;
    PgSqlResultSet& operator=(const PgSqlResultSet&) = delete;
    PgSqlResultSet(PgSqlResultSet&&) noexcept;
    PgSqlResultSet& operator=(PgSqlResultSet&&) noexcept;

    bool next();

    [[nodiscard]] std::string_view get_string(unsigned int col) const;
    [[nodiscard]] int get_int(unsigned int col) const;
    [[nodiscard]] int64_t get_int64(unsigned int col) const;
    [[nodiscard]] double get_double(unsigned int col) const;
    [[nodiscard]] bool is_null(unsigned int col) const;

    [[nodiscard]] unsigned int get_field_count() const;
    [[nodiscard]] uint64_t get_row_count() const;

private:
    friend class PgSqlDB;
    friend class PgSqlPipeline;
    explicit PgSqlResultSet(PGresult* result);

    std::unique_ptr<PGresult, decltype(&PQclear)> result_;
    int current_row_ = -1;
    int row_count_ = 0;
    int field_count_ = 0;
};

/**
 * @class PgSqlTransaction
 * @brief A RAII guard for managing PostgreSQL database transactions.
 */
class PgSqlTransaction {
public:
    ~PgSqlTransaction();

    PgSqlTransaction(const PgSqlTransaction&) = delete;
    PgSqlTransaction& operator=(const PgSqlTransaction&) = delete;
    PgSqlTransaction(PgSqlTransaction&&) noexcept;
    PgSqlTransaction& operator=(PgSqlTransaction&&) noexcept;

    void commit();
    void rollback();
    PGconn* get_connection();

private:
    friend class PgSqlDB;
    explicit PgSqlTransaction(
        std::unique_ptr<PGconn, std::function<void(PGconn*)>> conn);

    std::unique_ptr<PGconn, std::function<void(PGconn*)>> conn_;
    bool committed_or_rolled_back_ = false;
};

/**
 * @class PgSqlPipeline
 * @brief A class for batching PostgreSQL commands.
 */
class PgSqlPipeline {
public:
    ~PgSqlPipeline();

    PgSqlPipeline(const PgSqlPipeline&) = delete;
    PgSqlPipeline& operator=(const PgSqlPipeline&) = delete;
    PgSqlPipeline(PgSqlPipeline&&) noexcept;
    PgSqlPipeline& operator=(PgSqlPipeline&&) noexcept;

    template <typename... Args>
    void append(std::string_view query, Args&&... args);

    [[nodiscard]] std::vector<std::unique_ptr<PgSqlResultSet>> execute();

private:
    friend class PgSqlDB;
    explicit PgSqlPipeline(
        std::unique_ptr<PGconn, std::function<void(PGconn*)>> conn);

    std::unique_ptr<PGconn, std::function<void(PGconn*)>> conn_;
};

}  // namespace atom::database

#endif  // ATOM_SEARCH_PGSQL_HPP
