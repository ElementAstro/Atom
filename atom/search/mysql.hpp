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
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace atom {
namespace database {

/**
 * @brief Custom exception class for MySQL-related errors
 */
class MySQLException : public std::runtime_error {
public:
    explicit MySQLException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Structure to hold database connection parameters
 */
struct ConnectionParams {
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    unsigned int port = 3306;
    std::string socket;
    unsigned long clientFlag = 0;
};

/**
 * @brief Enum for transaction isolation levels
 */
enum class TransactionIsolation {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SERIALIZABLE
};

/**
 * @brief Class representing a database row
 */
class Row {
public:
    Row(MYSQL_ROW row, unsigned long* lengths, unsigned int numFields);

    std::string getString(unsigned int index) const;
    int getInt(unsigned int index) const;
    double getDouble(unsigned int index) const;
    bool getBool(unsigned int index) const;
    bool isNull(unsigned int index) const;

    unsigned int getFieldCount() const { return numFields; }

private:
    MYSQL_ROW row;
    std::vector<unsigned long> lengths;
    unsigned int numFields;
};

/**
 * @brief Class representing a database result set
 */
/**
 * @class ResultSet
 * @brief Represents the result of a MySQL query, providing an interface to
 * access the returned rows and fields.
 *
 * This class wraps the MYSQL_RES structure and provides methods to navigate
 * through the result set, retrieve field values, field names, count rows and
 * columns. It also implements iterator support for modern C++ iteration
 * patterns.
 *
 * The class follows RAII principle, automatically freeing the result set when
 * destroyed. It is move-constructible and move-assignable, but not
 * copy-constructible or copy-assignable.
 *
 * @note This class is designed to work with MySQL C API.
 */
class ResultSet {
public:
    explicit ResultSet(MYSQL_RES* result);
    ~ResultSet();

    // Prevent copying
    ResultSet(const ResultSet&) = delete;
    ResultSet& operator=(const ResultSet&) = delete;

    // Allow moving
    ResultSet(ResultSet&& other) noexcept;
    ResultSet& operator=(ResultSet&& other) noexcept;

    bool next();
    Row getCurrentRow() const;
    unsigned int getFieldCount() const;
    std::string getFieldName(unsigned int index) const;
    unsigned long long getRowCount() const;

    // Iterator support
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

    iterator begin() {
        if (!initialized) {
            initialized = true;
            if (!next()) {
                return end();
            }
        }
        return iterator(this);
    }
    iterator end() { return iterator(this, true); }

private:
    MYSQL_RES* result;
    MYSQL_ROW currentRow;
    unsigned long* lengths;
    unsigned int numFields;
    bool initialized = false;
};

/**
 * @brief Class for prepared statements
 */
class PreparedStatement {
public:
    PreparedStatement(MYSQL* connection, const std::string& query);
    ~PreparedStatement();

    // Prevent copying
    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;

    // Allow moving
    PreparedStatement(PreparedStatement&& other) noexcept;
    PreparedStatement& operator=(PreparedStatement&& other) noexcept;

    // Binding methods
    PreparedStatement& bindString(int index, const std::string& value);
    PreparedStatement& bindInt(int index, int value);
    PreparedStatement& bindDouble(int index, double value);
    PreparedStatement& bindBool(int index, bool value);
    PreparedStatement& bindNull(int index);

    bool execute();
    std::unique_ptr<ResultSet> executeQuery();
    int executeUpdate();

    void reset();
    void clearParameters();

private:
    MYSQL_STMT* stmt;
    std::vector<MYSQL_BIND> binds;
    std::vector<std::unique_ptr<char[]>> stringBuffers;
    std::vector<unsigned long> stringLengths;
    std::vector<my_bool> isNull;
};

/**
 * @class MysqlDB
 * @brief Enhanced class for interacting with a MySQL/MariaDB database.
 */
class MysqlDB {
public:
    /**
     * @brief Constructor with connection parameters
     */
    explicit MysqlDB(const ConnectionParams& params);

    /**
     * @brief Constructor with individual connection parameters
     */
    MysqlDB(const std::string& host, const std::string& user,
            const std::string& password, const std::string& database,
            unsigned int port = 3306, const std::string& socket = "",
            unsigned long clientFlag = 0);

    /**
     * @brief Destructor that closes the database connection
     */
    ~MysqlDB();

    // Prevent copying, allow moving
    MysqlDB(const MysqlDB&) = delete;
    MysqlDB& operator=(const MysqlDB&) = delete;
    MysqlDB(MysqlDB&& other) noexcept;
    MysqlDB& operator=(MysqlDB&& other) noexcept;

    /**
     * @brief Connects to the database with stored parameters
     */
    bool connect();

    /**
     * @brief Reconnects to the database if connection was lost
     */
    bool reconnect();

    /**
     * @brief Disconnects from the database
     */
    void disconnect();

    /**
     * @brief Checks if the connection is alive
     */
    bool isConnected();

    /**
     * @brief Executes a SQL query
     */
    bool executeQuery(const std::string& query);

    /**
     * @brief Executes a query and returns results
     */
    std::unique_ptr<ResultSet> executeQueryWithResults(
        const std::string& query);

    /**
     * @brief Executes a data modification query and returns affected rows
     */
    int executeUpdate(const std::string& query);

    /**
     * @brief Gets a single integer value from a query
     */
    int getIntValue(const std::string& query);

    /**
     * @brief Gets a single double value from a query
     */
    double getDoubleValue(const std::string& query);

    /**
     * @brief Gets a single string value from a query
     */
    std::string getStringValue(const std::string& query);

    /**
     * @brief Searches for data matching criteria
     */
    bool searchData(const std::string& query, const std::string& column,
                    const std::string& searchTerm);

    /**
     * @brief Creates a prepared statement for safe query execution
     */
    std::unique_ptr<PreparedStatement> prepareStatement(
        const std::string& query);

    /**
     * @brief Transaction management methods
     */
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool setSavepoint(const std::string& savepointName);
    bool rollbackToSavepoint(const std::string& savepointName);
    bool setTransactionIsolation(TransactionIsolation level);

    /**
     * @brief Batch operations
     */
    bool executeBatch(const std::vector<std::string>& queries);
    bool executeBatchTransaction(const std::vector<std::string>& queries);

    /**
     * @brief Stored procedure execution
     */
    std::unique_ptr<ResultSet> callProcedure(
        const std::string& procedureName,
        const std::vector<std::string>& params);

    /**
     * @brief Schema information retrieval
     */
    std::vector<std::string> getDatabases();
    std::vector<std::string> getTables();
    std::vector<std::string> getColumns(const std::string& tableName);

    /**
     * @brief Error handling
     */
    std::string getLastError() const;
    unsigned int getLastErrorCode() const;
    void setErrorCallback(
        const std::function<void(const std::string&, unsigned int)>& callback);

    /**
     * @brief Utility functions
     */
    std::string escapeString(const std::string& str);
    unsigned long long getLastInsertId() const;
    unsigned long long getAffectedRows() const;
    std::unique_ptr<ResultSet> executeQueryWithPagination(
        const std::string& query, int limit, int offset);

private:
    MYSQL* db;
    ConnectionParams params;
    std::mutex mutex;
    std::function<void(const std::string&, unsigned int)> errorCallback;
    bool autoReconnect = true;

    bool handleError(const std::string& operation, bool throwOnError = false);
};

}  // namespace database
}  // namespace atom

#endif  // ATOM_SEARCH_MYSQL_HPP