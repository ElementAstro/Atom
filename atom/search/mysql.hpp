/*
 * mysql.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-6, updated: 2024-04-5

Description: Enhanced MySQL/MariaDB wrapper

**************************************************/

#ifndef ATOM_SEARCH_MYSQL_HPP
#define ATOM_SEARCH_MYSQL_HPP

#include <mariadb/mysql.h>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace atom {
namespace database {

/**
 * @brief Custom exception class for MySQL-related errors
 *
 * This exception is thrown when MySQL operations fail or encounter errors.
 * It provides detailed error messages to help with debugging.
 */
class MySQLException : public std::runtime_error {
public:
    /**
     * @brief Construct a new MySQL Exception object
     *
     * @param message Error message describing the exception
     */
    explicit MySQLException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Structure to hold database connection parameters
 *
 * This structure encapsulates all the necessary parameters needed
 * to establish a connection to a MySQL/MariaDB database.
 */
struct ConnectionParams {
    std::string host;                  ///< Database server hostname or IP
    std::string user;                  ///< Database username
    std::string password;              ///< Database password
    std::string database;              ///< Database name
    unsigned int port = 3306;          ///< Database server port
    std::string socket;                ///< Unix socket path (optional)
    unsigned long clientFlag = 0;      ///< MySQL client flags
    unsigned int connectTimeout = 30;  ///< Connection timeout in seconds
    unsigned int readTimeout = 30;     ///< Read timeout in seconds
    unsigned int writeTimeout = 30;    ///< Write timeout in seconds
    bool autoReconnect = true;         ///< Enable automatic reconnection
    std::string charset = "utf8mb4";   ///< Character set
};

/**
 * @brief Enum for transaction isolation levels
 *
 * Defines the different isolation levels available for database transactions,
 * controlling how transactions interact with each other.
 */
enum class TransactionIsolation {
    READ_UNCOMMITTED,  ///< Lowest isolation level, allows dirty reads
    READ_COMMITTED,    ///< Prevents dirty reads
    REPEATABLE_READ,   ///< Prevents dirty and non-repeatable reads
    SERIALIZABLE       ///< Highest isolation level, prevents all phenomena
};

/**
 * @brief Class representing a database row
 *
 * This class provides methods to access field values in different data types
 * from a single row of a MySQL result set.
 */
class Row {
public:
    /**
     * @brief Construct a new Row object
     *
     * @param row MySQL row data
     * @param lengths Array of field lengths
     * @param numFields Number of fields in the row
     */
    Row(MYSQL_ROW row, unsigned long* lengths, unsigned int numFields);

    /**
     * @brief Get a string value from the specified field
     *
     * @param index Field index (0-based)
     * @return std::string Field value as string, empty if null or invalid index
     */
    std::string getString(unsigned int index) const;

    /**
     * @brief Get an integer value from the specified field
     *
     * @param index Field index (0-based)
     * @return int Field value as integer, 0 if null or invalid index
     */
    int getInt(unsigned int index) const;

    /**
     * @brief Get a 64-bit integer value from the specified field
     *
     * @param index Field index (0-based)
     * @return int64_t Field value as 64-bit integer, 0 if null or invalid index
     */
    int64_t getInt64(unsigned int index) const;

    /**
     * @brief Get a double value from the specified field
     *
     * @param index Field index (0-based)
     * @return double Field value as double, 0.0 if null or invalid index
     */
    double getDouble(unsigned int index) const;

    /**
     * @brief Get a boolean value from the specified field
     *
     * @param index Field index (0-based)
     * @return bool Field value as boolean, false if null or invalid index
     */
    bool getBool(unsigned int index) const;

    /**
     * @brief Check if the specified field is null
     *
     * @param index Field index (0-based)
     * @return true if field is null, false otherwise
     */
    bool isNull(unsigned int index) const;

    /**
     * @brief Get the number of fields in this row
     *
     * @return unsigned int Number of fields
     */
    unsigned int getFieldCount() const { return numFields; }

private:
    MYSQL_ROW row;                       ///< MySQL row data
    std::vector<unsigned long> lengths;  ///< Field lengths
    unsigned int numFields;              ///< Number of fields
};

/**
 * @class ResultSet
 * @brief Represents the result of a MySQL query
 *
 * This class wraps the MYSQL_RES structure and provides methods to navigate
 * through the result set, retrieve field values, field names, count rows and
 * columns. It implements iterator support for modern C++ iteration patterns.
 *
 * The class follows RAII principle, automatically freeing the result set when
 * destroyed. It is move-constructible and move-assignable, but not
 * copy-constructible or copy-assignable.
 */
class ResultSet {
public:
    /**
     * @brief Construct a new ResultSet object
     *
     * @param result MySQL result set pointer
     */
    explicit ResultSet(MYSQL_RES* result);

    /**
     * @brief Destroy the ResultSet object
     *
     * Automatically frees the MySQL result set.
     */
    ~ResultSet();

    ResultSet(const ResultSet&) = delete;
    ResultSet& operator=(const ResultSet&) = delete;

    /**
     * @brief Move constructor
     *
     * @param other Source ResultSet to move from
     */
    ResultSet(ResultSet&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other Source ResultSet to move from
     * @return ResultSet& Reference to this object
     */
    ResultSet& operator=(ResultSet&& other) noexcept;

    /**
     * @brief Move to the next row in the result set
     *
     * @return true if there is a next row, false if end of result set
     */
    bool next();

    /**
     * @brief Get the current row
     *
     * @return Row Current row object
     * @throws std::runtime_error if no current row
     */
    Row getCurrentRow() const;

    /**
     * @brief Get the number of fields in the result set
     *
     * @return unsigned int Number of fields
     */
    unsigned int getFieldCount() const;

    /**
     * @brief Get the name of a field by index
     *
     * @param index Field index (0-based)
     * @return std::string Field name, empty if invalid index
     */
    std::string getFieldName(unsigned int index) const;

    /**
     * @brief Get the total number of rows in the result set
     *
     * @return unsigned long long Number of rows
     */
    unsigned long long getRowCount() const;

    /**
     * @brief Reset the result set to the beginning
     *
     * @return true if successful, false otherwise
     */
    bool reset();

    /**
     * @brief Iterator class for range-based loops
     */
    class iterator {
    public:
        iterator(ResultSet* rs, bool end = false) : rs(rs), isEnd(end) {}

        Row operator*() const { return rs->getCurrentRow(); }
        iterator& operator++() {
            if (!rs->next()) {
                isEnd = true;
            }
            return *this;
        }
        bool operator!=(const iterator& other) const {
            return isEnd != other.isEnd;
        }
        bool operator==(const iterator& other) const {
            return isEnd == other.isEnd;
        }

    private:
        ResultSet* rs;
        bool isEnd;
    };

    /**
     * @brief Get iterator to the beginning of the result set
     *
     * @return iterator Iterator to the first row
     */
    iterator begin() {
        if (!initialized) {
            initialized = true;
            if (!next()) {
                return end();
            }
        }
        return iterator(this);
    }

    /**
     * @brief Get iterator to the end of the result set
     *
     * @return iterator Iterator representing end
     */
    iterator end() { return iterator(this, true); }

private:
    MYSQL_RES* result;         ///< MySQL result set
    MYSQL_ROW currentRow;      ///< Current row data
    unsigned long* lengths;    ///< Field lengths for current row
    unsigned int numFields;    ///< Number of fields
    bool initialized = false;  ///< Iterator initialization flag
};

/**
 * @brief Class for prepared statements
 *
 * This class provides a safe way to execute SQL statements with parameters,
 * preventing SQL injection attacks and improving performance for repeated
 * queries.
 */
class PreparedStatement {
public:
    /**
     * @brief Construct a new PreparedStatement object
     *
     * @param connection MySQL connection handle
     * @param query SQL query with parameter placeholders (?)
     * @throws MySQLException if statement preparation fails
     */
    PreparedStatement(MYSQL* connection, const std::string& query);

    /**
     * @brief Destroy the PreparedStatement object
     *
     * Automatically closes the MySQL statement.
     */
    ~PreparedStatement();

    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;

    /**
     * @brief Move constructor
     *
     * @param other Source PreparedStatement to move from
     */
    PreparedStatement(PreparedStatement&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other Source PreparedStatement to move from
     * @return PreparedStatement& Reference to this object
     */
    PreparedStatement& operator=(PreparedStatement&& other) noexcept;

    /**
     * @brief Bind a string parameter
     *
     * @param index Parameter index (0-based)
     * @param value String value to bind
     * @return PreparedStatement& Reference to this object for method chaining
     */
    PreparedStatement& bindString(int index, const std::string& value);

    /**
     * @brief Bind an integer parameter
     *
     * @param index Parameter index (0-based)
     * @param value Integer value to bind
     * @return PreparedStatement& Reference to this object for method chaining
     */
    PreparedStatement& bindInt(int index, int value);

    /**
     * @brief Bind a 64-bit integer parameter
     *
     * @param index Parameter index (0-based)
     * @param value 64-bit integer value to bind
     * @return PreparedStatement& Reference to this object for method chaining
     */
    PreparedStatement& bindInt64(int index, int64_t value);

    /**
     * @brief Bind a double parameter
     *
     * @param index Parameter index (0-based)
     * @param value Double value to bind
     * @return PreparedStatement& Reference to this object for method chaining
     */
    PreparedStatement& bindDouble(int index, double value);

    /**
     * @brief Bind a boolean parameter
     *
     * @param index Parameter index (0-based)
     * @param value Boolean value to bind
     * @return PreparedStatement& Reference to this object for method chaining
     */
    PreparedStatement& bindBool(int index, bool value);

    /**
     * @brief Bind a null parameter
     *
     * @param index Parameter index (0-based)
     * @return PreparedStatement& Reference to this object for method chaining
     */
    PreparedStatement& bindNull(int index);

    /**
     * @brief Execute the prepared statement
     *
     * @return true if execution was successful, false otherwise
     */
    bool execute();

    /**
     * @brief Execute the prepared statement and return results
     *
     * @return std::unique_ptr<ResultSet> Result set containing query results
     * @throws MySQLException if execution fails
     */
    std::unique_ptr<ResultSet> executeQuery();

    /**
     * @brief Execute an update/insert/delete statement
     *
     * @return int Number of affected rows
     * @throws MySQLException if execution fails
     */
    int executeUpdate();

    /**
     * @brief Reset the statement for reuse
     *
     * @throws MySQLException if reset fails
     */
    void reset();

    /**
     * @brief Clear all bound parameters
     */
    void clearParameters();

    /**
     * @brief Get the number of parameters in the statement
     *
     * @return unsigned int Number of parameters
     */
    unsigned int getParameterCount() const;

private:
    MYSQL_STMT* stmt;               ///< MySQL statement handle
    std::vector<MYSQL_BIND> binds;  ///< Parameter bindings
    std::vector<std::unique_ptr<char[]>>
        stringBuffers;                         ///< String parameter buffers
    std::vector<unsigned long> stringLengths;  ///< String parameter lengths
    std::vector<my_bool> isNull;               ///< Null flags for parameters
};

/**
 * @class MysqlDB
 * @brief Enhanced class for interacting with a MySQL/MariaDB database
 *
 * This class provides a comprehensive interface for MySQL database operations
 * including connection management, query execution, transaction handling,
 * prepared statements, and error management. It is thread-safe and supports
 * automatic reconnection.
 */
class MysqlDB {
public:
    /**
     * @brief Constructor with connection parameters structure
     *
     * @param params Connection parameters
     * @throws MySQLException if connection fails
     */
    explicit MysqlDB(const ConnectionParams& params);

    /**
     * @brief Constructor with individual connection parameters
     *
     * @param host Database server hostname or IP
     * @param user Database username
     * @param password Database password
     * @param database Database name
     * @param port Database server port
     * @param socket Unix socket path (optional)
     * @param clientFlag MySQL client flags
     * @throws MySQLException if connection fails
     */
    MysqlDB(const std::string& host, const std::string& user,
            const std::string& password, const std::string& database,
            unsigned int port = 3306, const std::string& socket = "",
            unsigned long clientFlag = 0);

    /**
     * @brief Destructor that closes the database connection
     */
    ~MysqlDB();

    MysqlDB(const MysqlDB&) = delete;
    MysqlDB& operator=(const MysqlDB&) = delete;

    /**
     * @brief Move constructor
     *
     * @param other Source MysqlDB to move from
     */
    MysqlDB(MysqlDB&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * @param other Source MysqlDB to move from
     * @return MysqlDB& Reference to this object
     */
    MysqlDB& operator=(MysqlDB&& other) noexcept;

    /**
     * @brief Connect to the database with stored parameters
     *
     * @return true if connection successful, false otherwise
     */
    bool connect();

    /**
     * @brief Reconnect to the database if connection was lost
     *
     * @return true if reconnection successful, false otherwise
     */
    bool reconnect();

    /**
     * @brief Disconnect from the database
     */
    void disconnect();

    /**
     * @brief Check if the connection is alive
     *
     * @return true if connected, false otherwise
     */
    bool isConnected();

    /**
     * @brief Execute a SQL query without returning results
     *
     * @param query SQL query string
     * @return true if execution successful, false otherwise
     */
    bool executeQuery(const std::string& query);

    /**
     * @brief Execute a query and return results
     *
     * @param query SQL SELECT query string
     * @return std::unique_ptr<ResultSet> Result set containing query results
     * @throws MySQLException if execution fails
     */
    std::unique_ptr<ResultSet> executeQueryWithResults(
        const std::string& query);

    /**
     * @brief Execute a data modification query and return affected rows
     *
     * @param query SQL INSERT/UPDATE/DELETE query
     * @return int Number of affected rows, -1 if error
     * @throws MySQLException if execution fails
     */
    int executeUpdate(const std::string& query);

    /**
     * @brief Get a single integer value from a query
     *
     * @param query SQL query that returns a single integer
     * @return std::optional<int> Integer value if successful, nullopt otherwise
     */
    std::optional<int> getIntValue(const std::string& query);

    /**
     * @brief Get a single double value from a query
     *
     * @param query SQL query that returns a single double
     * @return std::optional<double> Double value if successful, nullopt
     * otherwise
     */
    std::optional<double> getDoubleValue(const std::string& query);

    /**
     * @brief Get a single string value from a query
     *
     * @param query SQL query that returns a single string
     * @return std::optional<std::string> String value if successful, nullopt
     * otherwise
     */
    std::optional<std::string> getStringValue(const std::string& query);

    /**
     * @brief Search for data matching criteria
     *
     * @param query Base SQL query
     * @param column Column name to search in
     * @param searchTerm Term to search for
     * @return true if matching data found, false otherwise
     */
    bool searchData(const std::string& query, const std::string& column,
                    const std::string& searchTerm);

    /**
     * @brief Create a prepared statement for safe query execution
     *
     * @param query SQL query with parameter placeholders (?)
     * @return std::unique_ptr<PreparedStatement> Prepared statement object
     * @throws MySQLException if preparation fails
     */
    std::unique_ptr<PreparedStatement> prepareStatement(
        const std::string& query);

    /**
     * @brief Begin a database transaction
     *
     * @return true if transaction started successfully, false otherwise
     */
    bool beginTransaction();

    /**
     * @brief Commit the current transaction
     *
     * @return true if transaction committed successfully, false otherwise
     */
    bool commitTransaction();

    /**
     * @brief Rollback the current transaction
     *
     * @return true if transaction rolled back successfully, false otherwise
     */
    bool rollbackTransaction();

    /**
     * @brief Set a savepoint within a transaction
     *
     * @param savepointName Name of the savepoint
     * @return true if savepoint created successfully, false otherwise
     */
    bool setSavepoint(const std::string& savepointName);

    /**
     * @brief Rollback to a specific savepoint
     *
     * @param savepointName Name of the savepoint
     * @return true if rollback successful, false otherwise
     */
    bool rollbackToSavepoint(const std::string& savepointName);

    /**
     * @brief Set transaction isolation level
     *
     * @param level Isolation level to set
     * @return true if isolation level set successfully, false otherwise
     */
    bool setTransactionIsolation(TransactionIsolation level);

    /**
     * @brief Execute multiple queries in sequence
     *
     * @param queries Vector of SQL queries to execute
     * @return true if all queries executed successfully, false otherwise
     */
    bool executeBatch(const std::vector<std::string>& queries);

    /**
     * @brief Execute multiple queries within a transaction
     *
     * @param queries Vector of SQL queries to execute
     * @return true if all queries executed successfully, false if any failed
     * (transaction rolled back)
     */
    bool executeBatchTransaction(const std::vector<std::string>& queries);

    /**
     * @brief Execute operations within a transaction with automatic rollback
     *
     * @param operations Function containing database operations to execute
     * @throws Re-throws any exceptions from operations after rollback
     */
    void withTransaction(const std::function<void()>& operations);

    /**
     * @brief Call a stored procedure
     *
     * @param procedureName Name of the stored procedure
     * @param params Vector of parameters for the procedure
     * @return std::unique_ptr<ResultSet> Result set if procedure returns data
     * @throws MySQLException if procedure call fails
     */
    std::unique_ptr<ResultSet> callProcedure(
        const std::string& procedureName,
        const std::vector<std::string>& params);

    /**
     * @brief Get list of databases on the server
     *
     * @return std::vector<std::string> Vector of database names
     * @throws MySQLException if query fails
     */
    std::vector<std::string> getDatabases();

    /**
     * @brief Get list of tables in the current database
     *
     * @return std::vector<std::string> Vector of table names
     * @throws MySQLException if query fails
     */
    std::vector<std::string> getTables();

    /**
     * @brief Get list of columns for a specific table
     *
     * @param tableName Name of the table
     * @return std::vector<std::string> Vector of column names
     * @throws MySQLException if query fails
     */
    std::vector<std::string> getColumns(const std::string& tableName);

    /**
     * @brief Check if a table exists in the database
     *
     * @param tableName Name of the table to check
     * @return true if table exists, false otherwise
     */
    bool tableExists(const std::string& tableName);

    /**
     * @brief Get the last error message
     *
     * @return std::string Error message
     */
    std::string getLastError() const;

    /**
     * @brief Get the last error code
     *
     * @return unsigned int Error code
     */
    unsigned int getLastErrorCode() const;

    /**
     * @brief Set a custom error callback function
     *
     * @param callback Function to call when errors occur
     */
    void setErrorCallback(
        const std::function<void(const std::string&, unsigned int)>& callback);

    /**
     * @brief Escape a string for safe use in SQL queries
     *
     * @param str String to escape
     * @return std::string Escaped string
     * @throws MySQLException if not connected
     */
    std::string escapeString(const std::string& str);

    /**
     * @brief Get the ID of the last inserted row
     *
     * @return unsigned long long Last insert ID
     */
    unsigned long long getLastInsertId() const;

    /**
     * @brief Get the number of rows affected by the last statement
     *
     * @return unsigned long long Number of affected rows
     */
    unsigned long long getAffectedRows() const;

    /**
     * @brief Execute a query with pagination
     *
     * @param query Base SQL SELECT query
     * @param limit Maximum number of rows to return
     * @param offset Number of rows to skip
     * @return std::unique_ptr<ResultSet> Paginated result set
     * @throws MySQLException if query fails
     */
    std::unique_ptr<ResultSet> executeQueryWithPagination(
        const std::string& query, int limit, int offset);

    /**
     * @brief Get database server version
     *
     * @return std::string Server version string
     */
    std::string getServerVersion() const;

    /**
     * @brief Get client library version
     *
     * @return std::string Client library version string
     */
    std::string getClientVersion() const;

    /**
     * @brief Ping the server to check connection
     *
     * @return true if connection is alive, false otherwise
     */
    bool ping();

    /**
     * @brief Set connection timeout
     *
     * @param timeout Timeout in seconds
     * @return true if timeout set successfully, false otherwise
     */
    bool setConnectionTimeout(unsigned int timeout);

private:
    MYSQL* db;                 ///< MySQL connection handle
    ConnectionParams params;   ///< Connection parameters
    mutable std::mutex mutex;  ///< Thread safety mutex
    std::function<void(const std::string&, unsigned int)>
        errorCallback;          ///< Error callback function
    bool autoReconnect = true;  ///< Auto-reconnect flag

    /**
     * @brief Handle database errors
     *
     * @param operation Description of the operation that failed
     * @param throwOnError Whether to throw exception on error
     * @return true if error occurred, false otherwise
     */
    bool handleError(const std::string& operation, bool throwOnError = false);

    /**
     * @brief Configure connection options
     */
    void configureConnection();
};

}  // namespace database
}  // namespace atom

#endif  // ATOM_SEARCH_MYSQL_HPP
