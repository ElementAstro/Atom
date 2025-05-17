/*
 * mysql.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-6, updated: 2024-04-5

Description: Enhanced MySQL/MariaDB wrapper implementation

**************************************************/

#include "mysql.hpp"

#include <cstring>

#include "atom/log/loguru.hpp"

namespace atom {
namespace database {

//--------------------
// Row Implementation
//--------------------

Row::Row(MYSQL_ROW row, unsigned long* lengths, unsigned int numFields)
    : row(row), numFields(numFields) {
    this->lengths.reserve(numFields);
    for (unsigned int i = 0; i < numFields; ++i) {
        this->lengths.push_back(lengths[i]);
    }
}

std::string Row::getString(unsigned int index) const {
    if (index >= numFields || isNull(index)) {
        return "";
    }
    return std::string(row[index], lengths[index]);
}

int Row::getInt(unsigned int index) const {
    if (index >= numFields || isNull(index)) {
        return 0;
    }
    return std::stoi(getString(index));
}

double Row::getDouble(unsigned int index) const {
    if (index >= numFields || isNull(index)) {
        return 0.0;
    }
    return std::stod(getString(index));
}

bool Row::getBool(unsigned int index) const {
    if (index >= numFields || isNull(index)) {
        return false;
    }
    std::string val = getString(index);
    return !val.empty() && (val != "0");
}

bool Row::isNull(unsigned int index) const {
    return index < numFields && row[index] == nullptr;
}

//--------------------
// ResultSet Implementation
//--------------------

ResultSet::ResultSet(MYSQL_RES* result)
    : result(result), currentRow(nullptr), lengths(nullptr) {
    numFields = result ? mysql_num_fields(result) : 0;
}

ResultSet::~ResultSet() {
    if (result) {
        mysql_free_result(result);
        result = nullptr;
    }
}

ResultSet::ResultSet(ResultSet&& other) noexcept
    : result(other.result),
      currentRow(other.currentRow),
      lengths(other.lengths),
      numFields(other.numFields) {
    other.result = nullptr;
    other.currentRow = nullptr;
    other.lengths = nullptr;
    other.numFields = 0;
}

ResultSet& ResultSet::operator=(ResultSet&& other) noexcept {
    if (this != &other) {
        if (result) {
            mysql_free_result(result);
        }

        result = other.result;
        currentRow = other.currentRow;
        lengths = other.lengths;
        numFields = other.numFields;

        other.result = nullptr;
        other.currentRow = nullptr;
        other.lengths = nullptr;
        other.numFields = 0;
    }
    return *this;
}

bool ResultSet::next() {
    if (!result) {
        return false;
    }

    currentRow = mysql_fetch_row(result);
    if (currentRow) {
        lengths = mysql_fetch_lengths(result);
        return true;
    }
    return false;
}

Row ResultSet::getCurrentRow() const {
    if (!currentRow || !lengths) {
        throw std::runtime_error("No current row");
    }
    return Row(currentRow, lengths, numFields);
}

unsigned int ResultSet::getFieldCount() const { return numFields; }

std::string ResultSet::getFieldName(unsigned int index) const {
    if (!result || index >= numFields) {
        return "";
    }

    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    return fields[index].name;
}

unsigned long long ResultSet::getRowCount() const {
    return result ? mysql_num_rows(result) : 0;
}

//--------------------
// PreparedStatement Implementation
//--------------------

PreparedStatement::PreparedStatement(MYSQL* connection,
                                     const std::string& query)
    : stmt(nullptr) {
    stmt = mysql_stmt_init(connection);
    if (!stmt) {
        throw MySQLException("Failed to initialize prepared statement");
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) != 0) {
        std::string error = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw MySQLException("Failed to prepare statement: " + error);
    }

    unsigned int paramCount = mysql_stmt_param_count(stmt);
    if (paramCount > 0) {
        binds.resize(paramCount);
        stringBuffers.resize(paramCount);
        stringLengths.resize(paramCount);
        isNull.resize(paramCount);

        // Initialize binds
        for (unsigned int i = 0; i < paramCount; ++i) {
            memset(&binds[i], 0, sizeof(MYSQL_BIND));
            isNull[i] = true;
            binds[i].is_null = &isNull[i];
        }
    }
}

PreparedStatement::~PreparedStatement() {
    if (stmt) {
        mysql_stmt_close(stmt);
    }
}

PreparedStatement::PreparedStatement(PreparedStatement&& other) noexcept
    : stmt(other.stmt),
      binds(std::move(other.binds)),
      stringBuffers(std::move(other.stringBuffers)),
      stringLengths(std::move(other.stringLengths)),
      isNull(std::move(other.isNull)) {
    other.stmt = nullptr;
}

PreparedStatement& PreparedStatement::operator=(
    PreparedStatement&& other) noexcept {
    if (this != &other) {
        if (stmt) {
            mysql_stmt_close(stmt);
        }

        stmt = other.stmt;
        binds = std::move(other.binds);
        stringBuffers = std::move(other.stringBuffers);
        stringLengths = std::move(other.stringLengths);
        isNull = std::move(other.isNull);

        other.stmt = nullptr;
    }
    return *this;
}

PreparedStatement& PreparedStatement::bindString(int index,
                                                 const std::string& value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range");
    }

    // Allocate buffer for string data (should persist until execute)
    auto buffer = std::make_unique<char[]>(value.length());
    memcpy(buffer.get(), value.c_str(), value.length());

    // Set up the MYSQL_BIND structure
    binds[index].buffer_type = MYSQL_TYPE_STRING;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = value.length();
    stringLengths[index] = value.length();
    binds[index].length = &stringLengths[index];
    isNull[index] = false;

    // Store buffer pointer
    stringBuffers[index] = std::move(buffer);

    return *this;
}

PreparedStatement& PreparedStatement::bindInt(int index, int value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range");
    }

    // Allocate buffer for int data
    auto buffer = std::make_unique<int>(value);

    // Set up the MYSQL_BIND structure
    binds[index].buffer_type = MYSQL_TYPE_LONG;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(int);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    // Store buffer pointer
    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));

    return *this;
}

PreparedStatement& PreparedStatement::bindDouble(int index, double value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range");
    }

    // Allocate buffer for double data
    auto buffer = std::make_unique<double>(value);

    // Set up the MYSQL_BIND structure
    binds[index].buffer_type = MYSQL_TYPE_DOUBLE;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(double);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    // Store buffer pointer
    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));

    return *this;
}

PreparedStatement& PreparedStatement::bindBool(int index, bool value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range");
    }

    // Allocate buffer for bool data (use tiny int for MySQL)
    auto buffer = std::make_unique<my_bool>(value ? 1 : 0);

    // Set up the MYSQL_BIND structure
    binds[index].buffer_type = MYSQL_TYPE_TINY;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(my_bool);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    // Store buffer pointer
    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));

    return *this;
}

PreparedStatement& PreparedStatement::bindNull(int index) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range");
    }

    // Set the is_null flag to true
    isNull[index] = true;
    binds[index].is_null = &isNull[index];

    return *this;
}

bool PreparedStatement::execute() {
    // Bind parameters if we have any
    if (!binds.empty()) {
        if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
            throw MySQLException(std::string("Failed to bind parameters: ") +
                                 mysql_stmt_error(stmt));
        }
    }

    // Execute the statement
    if (mysql_stmt_execute(stmt) != 0) {
        return false;
    }

    return true;
}

std::unique_ptr<ResultSet> PreparedStatement::executeQuery() {
    if (!execute()) {
        throw MySQLException(std::string("Failed to execute query: ") +
                             mysql_stmt_error(stmt));
    }

    // Store the result
    if (mysql_stmt_store_result(stmt) != 0) {
        throw MySQLException(std::string("Failed to store result: ") +
                             mysql_stmt_error(stmt));
    }

    // Get the result
    MYSQL_RES* metaData = mysql_stmt_result_metadata(stmt);
    if (!metaData) {
        throw MySQLException("Statement did not return a result set");
    }

    // Create a ResultSet from the metadata
    return std::make_unique<ResultSet>(metaData);
}

int PreparedStatement::executeUpdate() {
    if (!execute()) {
        throw MySQLException(std::string("Failed to execute update: ") +
                             mysql_stmt_error(stmt));
    }

    // Return affected rows
    return mysql_stmt_affected_rows(stmt);
}

void PreparedStatement::reset() {
    if (mysql_stmt_reset(stmt) != 0) {
        throw MySQLException(std::string("Failed to reset statement: ") +
                             mysql_stmt_error(stmt));
    }
}

void PreparedStatement::clearParameters() {
    // Reset all parameter binds
    for (size_t i = 0; i < binds.size(); i++) {
        memset(&binds[i], 0, sizeof(MYSQL_BIND));
        isNull[i] = true;
        binds[i].is_null = &isNull[i];
        stringBuffers[i].reset();
    }
}

//--------------------
// MysqlDB Implementation
//--------------------

MysqlDB::MysqlDB(const ConnectionParams& params) : db(nullptr), params(params) {
    connect();
}

MysqlDB::MysqlDB(const std::string& host, const std::string& user,
                 const std::string& password, const std::string& database,
                 unsigned int port, const std::string& socket,
                 unsigned long clientFlag)
    : db(nullptr) {
    params.host = host;
    params.user = user;
    params.password = password;
    params.database = database;
    params.port = port;
    params.socket = socket;
    params.clientFlag = clientFlag;

    connect();
}

MysqlDB::~MysqlDB() { disconnect(); }

MysqlDB::MysqlDB(MysqlDB&& other) noexcept
    : db(other.db),
      params(std::move(other.params)),
      errorCallback(std::move(other.errorCallback)),
      autoReconnect(other.autoReconnect) {
    other.db = nullptr;
}

MysqlDB& MysqlDB::operator=(MysqlDB&& other) noexcept {
    if (this != &other) {
        disconnect();

        db = other.db;
        params = std::move(other.params);
        errorCallback = std::move(other.errorCallback);
        autoReconnect = other.autoReconnect;

        other.db = nullptr;
    }
    return *this;
}

bool MysqlDB::connect() {
    std::lock_guard<std::mutex> lock(mutex);

    if (db) {
        mysql_close(db);
    }

    db = mysql_init(nullptr);
    if (!db) {
        handleError("Failed to initialize MySQL", true);
        return false;
    }

    // Enable auto-reconnect
    my_bool reconnect = autoReconnect ? 1 : 0;
    mysql_options(db, MYSQL_OPT_RECONNECT, &reconnect);

    // Connect to the database
    if (!mysql_real_connect(
            db, params.host.c_str(), params.user.c_str(),
            params.password.c_str(), params.database.c_str(), params.port,
            params.socket.empty() ? nullptr : params.socket.c_str(),
            params.clientFlag)) {
        handleError("Failed to connect to database", true);
        return false;
    }

    return true;
}

bool MysqlDB::reconnect() {
    std::lock_guard<std::mutex> lock(mutex);

    if (db) {
        // Try mysql_ping first, which will auto-reconnect if enabled
        if (mysql_ping(db) == 0) {
            return true;
        }
    }

    // If ping failed or there was no connection, try to connect
    return connect();
}

void MysqlDB::disconnect() {
    std::lock_guard<std::mutex> lock(mutex);

    if (db) {
        mysql_close(db);
        db = nullptr;
    }
}

bool MysqlDB::isConnected() {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db) {
        return false;
    }

    // Use mysql_ping to check if connection is alive
    return mysql_ping(db) == 0;
}

// Implementation of remaining methods...

bool MysqlDB::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        return false;
    }

    if (mysql_query(db, query.c_str()) != 0) {
        return handleError("Failed to execute query: " + query, false);
    }

    return true;
}

std::unique_ptr<ResultSet> MysqlDB::executeQueryWithResults(
    const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    if (mysql_query(db, query.c_str()) != 0) {
        handleError("Failed to execute query: " + query, true);
        return nullptr;
    }

    MYSQL_RES* result = mysql_store_result(db);
    if (!result && mysql_field_count(db) > 0) {
        handleError("Failed to store result for query: " + query, true);
        return nullptr;
    }

    return std::make_unique<ResultSet>(result);
}

int MysqlDB::executeUpdate(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    if (mysql_query(db, query.c_str()) != 0) {
        handleError("Failed to execute update: " + query, true);
        return -1;
    }

    return mysql_affected_rows(db);
}

int MysqlDB::getIntValue(const std::string& query) {
    auto result = executeQueryWithResults(query);
    if (!result || !result->next()) {
        return 0;
    }

    return result->getCurrentRow().getInt(0);
}

double MysqlDB::getDoubleValue(const std::string& query) {
    auto result = executeQueryWithResults(query);
    if (!result || !result->next()) {
        return 0.0;
    }

    return result->getCurrentRow().getDouble(0);
}

std::string MysqlDB::getStringValue(const std::string& query) {
    auto result = executeQueryWithResults(query);
    if (!result || !result->next()) {
        return "";
    }

    return result->getCurrentRow().getString(0);
}

// Additional method implementations would follow...

bool MysqlDB::handleError(const std::string& operation, bool throwOnError) {
    if (!db) {
        std::string errorMessage = "Not connected to database";
        LOG_F(ERROR, "{}: {}", operation, errorMessage);

        if (errorCallback) {
            errorCallback(errorMessage, 0);
        }

        if (throwOnError) {
            throw MySQLException(errorMessage);
        }

        return true;
    }

    unsigned int errorCode = mysql_errno(db);
    if (errorCode == 0) {
        return false;
    }

    std::string errorMessage = mysql_error(db);
    LOG_F(ERROR, "{}: {} (Error code: {})", operation, errorMessage, errorCode);

    if (errorCallback) {
        errorCallback(errorMessage, errorCode);
    }

    if (throwOnError) {
        throw MySQLException(operation + ": " + errorMessage);
    }

    return true;
}

bool MysqlDB::searchData(const std::string& query, const std::string& column,
                         const std::string& searchTerm) {
    std::string escapedSearchTerm = escapeString(searchTerm);
    std::string searchQuery =
        query + " WHERE " + column + " LIKE '%" + escapedSearchTerm + "%'";

    auto result = executeQueryWithResults(searchQuery);
    return result && result->getRowCount() > 0;
}

std::unique_ptr<PreparedStatement> MysqlDB::prepareStatement(
    const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    return std::make_unique<PreparedStatement>(db, query);
}

bool MysqlDB::beginTransaction() { return executeQuery("START TRANSACTION"); }

bool MysqlDB::commitTransaction() { return executeQuery("COMMIT"); }

bool MysqlDB::rollbackTransaction() { return executeQuery("ROLLBACK"); }

bool MysqlDB::setSavepoint(const std::string& savepointName) {
    std::string escapedName = escapeString(savepointName);
    std::string query = "SAVEPOINT " + escapedName;
    return executeQuery(query);
}

bool MysqlDB::rollbackToSavepoint(const std::string& savepointName) {
    std::string escapedName = escapeString(savepointName);
    std::string query = "ROLLBACK TO SAVEPOINT " + escapedName;
    return executeQuery(query);
}

bool MysqlDB::setTransactionIsolation(TransactionIsolation level) {
    std::string query;

    switch (level) {
        case TransactionIsolation::READ_UNCOMMITTED:
            query = "SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
            break;
        case TransactionIsolation::READ_COMMITTED:
            query = "SET TRANSACTION ISOLATION LEVEL READ COMMITTED";
            break;
        case TransactionIsolation::REPEATABLE_READ:
            query = "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ";
            break;
        case TransactionIsolation::SERIALIZABLE:
            query = "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE";
            break;
        default:
            return false;
    }

    return executeQuery(query);
}

bool MysqlDB::executeBatch(const std::vector<std::string>& queries) {
    for (const auto& query : queries) {
        if (!executeQuery(query)) {
            return false;
        }
    }
    return true;
}

bool MysqlDB::executeBatchTransaction(const std::vector<std::string>& queries) {
    if (!beginTransaction()) {
        return false;
    }

    for (const auto& query : queries) {
        if (!executeQuery(query)) {
            rollbackTransaction();
            return false;
        }
    }

    return commitTransaction();
}

std::unique_ptr<ResultSet> MysqlDB::callProcedure(
    const std::string& procedureName, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    std::string query = "CALL " + escapeString(procedureName) + "(";

    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            query += ", ";
        }
        query += "'" + escapeString(params[i]) + "'";
    }

    query += ")";

    return executeQueryWithResults(query);
}

std::vector<std::string> MysqlDB::getDatabases() {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> databases;

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    auto result = executeQueryWithResults("SHOW DATABASES");
    if (!result) {
        return databases;
    }

    while (result->next()) {
        databases.push_back(result->getCurrentRow().getString(0));
    }

    return databases;
}

std::vector<std::string> MysqlDB::getTables() {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> tables;

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    auto result = executeQueryWithResults("SHOW TABLES");
    if (!result) {
        return tables;
    }

    while (result->next()) {
        tables.push_back(result->getCurrentRow().getString(0));
    }

    return tables;
}

std::vector<std::string> MysqlDB::getColumns(const std::string& tableName) {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> columns;

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    std::string escapedTableName = escapeString(tableName);
    std::string query = "SHOW COLUMNS FROM " + escapedTableName;

    auto result = executeQueryWithResults(query);
    if (!result) {
        return columns;
    }

    while (result->next()) {
        columns.push_back(result->getCurrentRow().getString(0));
    }

    return columns;
}

std::string MysqlDB::getLastError() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));
    return db ? mysql_error(db) : "Not connected to database";
}

unsigned int MysqlDB::getLastErrorCode() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));
    return db ? mysql_errno(db) : 0;
}

void MysqlDB::setErrorCallback(
    const std::function<void(const std::string&, unsigned int)>& callback) {
    std::lock_guard<std::mutex> lock(mutex);
    errorCallback = callback;
}

std::string MysqlDB::escapeString(const std::string& str) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        throw MySQLException("Not connected to database");
    }

    // Allocate buffer for escaped string (worst case: 2*length+1)
    std::vector<char> buffer(str.length() * 2 + 1);

    // Escape the string
    unsigned long length =
        mysql_real_escape_string(db, buffer.data(), str.c_str(), str.length());

    return std::string(buffer.data(), length);
}

unsigned long long MysqlDB::getLastInsertId() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));

    if (!db) {
        return 0;
    }

    return mysql_insert_id(db);
}

unsigned long long MysqlDB::getAffectedRows() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));

    if (!db) {
        return 0;
    }

    return mysql_affected_rows(db);
}

std::unique_ptr<ResultSet> MysqlDB::executeQueryWithPagination(
    const std::string& query, int limit, int offset) {
    std::string paginatedQuery = query;

    // Add LIMIT and OFFSET if they don't already exist
    if (paginatedQuery.find("LIMIT") == std::string::npos) {
        paginatedQuery += " LIMIT " + std::to_string(limit);
    }

    if (paginatedQuery.find("OFFSET") == std::string::npos) {
        paginatedQuery += " OFFSET " + std::to_string(offset);
    }

    return executeQueryWithResults(paginatedQuery);
}

}  // namespace database
}  // namespace atom