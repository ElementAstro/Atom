/*
 * mysql.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mysql.hpp"

#include <spdlog/spdlog.h>
#include <cstring>

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
    try {
        return std::stoi(getString(index));
    } catch (const std::exception& e) {
        spdlog::warn("Failed to convert field {} to int: {}", index, e.what());
        return 0;
    }
}

int64_t Row::getInt64(unsigned int index) const {
    if (index >= numFields || isNull(index)) {
        return 0;
    }
    try {
        return std::stoll(getString(index));
    } catch (const std::exception& e) {
        spdlog::warn("Failed to convert field {} to int64: {}", index,
                     e.what());
        return 0;
    }
}

double Row::getDouble(unsigned int index) const {
    if (index >= numFields || isNull(index)) {
        return 0.0;
    }
    try {
        return std::stod(getString(index));
    } catch (const std::exception& e) {
        spdlog::warn("Failed to convert field {} to double: {}", index,
                     e.what());
        return 0.0;
    }
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
      numFields(other.numFields),
      initialized(other.initialized) {
    other.result = nullptr;
    other.currentRow = nullptr;
    other.lengths = nullptr;
    other.numFields = 0;
    other.initialized = false;
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
        initialized = other.initialized;

        other.result = nullptr;
        other.currentRow = nullptr;
        other.lengths = nullptr;
        other.numFields = 0;
        other.initialized = false;
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
        throw std::runtime_error("No current row available");
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

bool ResultSet::reset() {
    if (!result) {
        return false;
    }

    mysql_data_seek(result, 0);
    currentRow = nullptr;
    lengths = nullptr;
    initialized = false;
    return true;
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

        for (unsigned int i = 0; i < paramCount; ++i) {
            memset(&binds[i], 0, sizeof(MYSQL_BIND));
            isNull[i] = true;
            binds[i].is_null = &isNull[i];
        }
    }

    spdlog::debug("Prepared statement created with {} parameters", paramCount);
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
        throw MySQLException("Parameter index out of range: " +
                             std::to_string(index));
    }

    auto buffer = std::make_unique<char[]>(value.length());
    memcpy(buffer.get(), value.c_str(), value.length());

    binds[index].buffer_type = MYSQL_TYPE_STRING;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = value.length();
    stringLengths[index] = value.length();
    binds[index].length = &stringLengths[index];
    isNull[index] = false;

    stringBuffers[index] = std::move(buffer);
    return *this;
}

PreparedStatement& PreparedStatement::bindInt(int index, int value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range: " +
                             std::to_string(index));
    }

    auto buffer = std::make_unique<int>(value);

    binds[index].buffer_type = MYSQL_TYPE_LONG;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(int);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));
    return *this;
}

PreparedStatement& PreparedStatement::bindInt64(int index, int64_t value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range: " +
                             std::to_string(index));
    }

    auto buffer = std::make_unique<int64_t>(value);

    binds[index].buffer_type = MYSQL_TYPE_LONGLONG;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(int64_t);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));
    return *this;
}

PreparedStatement& PreparedStatement::bindDouble(int index, double value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range: " +
                             std::to_string(index));
    }

    auto buffer = std::make_unique<double>(value);

    binds[index].buffer_type = MYSQL_TYPE_DOUBLE;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(double);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));
    return *this;
}

PreparedStatement& PreparedStatement::bindBool(int index, bool value) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range: " +
                             std::to_string(index));
    }

    auto buffer = std::make_unique<my_bool>(value ? 1 : 0);

    binds[index].buffer_type = MYSQL_TYPE_TINY;
    binds[index].buffer = buffer.get();
    binds[index].buffer_length = sizeof(my_bool);
    isNull[index] = false;
    binds[index].is_null = &isNull[index];

    stringBuffers[index] =
        std::unique_ptr<char[]>(reinterpret_cast<char*>(buffer.release()));
    return *this;
}

PreparedStatement& PreparedStatement::bindNull(int index) {
    if (index < 0 || static_cast<size_t>(index) >= binds.size()) {
        throw MySQLException("Parameter index out of range: " +
                             std::to_string(index));
    }

    isNull[index] = true;
    binds[index].is_null = &isNull[index];
    return *this;
}

bool PreparedStatement::execute() {
    if (!binds.empty()) {
        if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
            throw MySQLException(std::string("Failed to bind parameters: ") +
                                 mysql_stmt_error(stmt));
        }
    }

    if (mysql_stmt_execute(stmt) != 0) {
        spdlog::error("Failed to execute prepared statement: {}",
                      mysql_stmt_error(stmt));
        return false;
    }

    return true;
}

std::unique_ptr<ResultSet> PreparedStatement::executeQuery() {
    if (!execute()) {
        throw MySQLException(std::string("Failed to execute query: ") +
                             mysql_stmt_error(stmt));
    }

    if (mysql_stmt_store_result(stmt) != 0) {
        throw MySQLException(std::string("Failed to store result: ") +
                             mysql_stmt_error(stmt));
    }

    MYSQL_RES* metaData = mysql_stmt_result_metadata(stmt);
    if (!metaData) {
        throw MySQLException("Statement did not return a result set");
    }

    return std::make_unique<ResultSet>(metaData);
}

int PreparedStatement::executeUpdate() {
    if (!execute()) {
        throw MySQLException(std::string("Failed to execute update: ") +
                             mysql_stmt_error(stmt));
    }

    return static_cast<int>(mysql_stmt_affected_rows(stmt));
}

void PreparedStatement::reset() {
    if (mysql_stmt_reset(stmt) != 0) {
        throw MySQLException(std::string("Failed to reset statement: ") +
                             mysql_stmt_error(stmt));
    }
}

void PreparedStatement::clearParameters() {
    for (size_t i = 0; i < binds.size(); i++) {
        memset(&binds[i], 0, sizeof(MYSQL_BIND));
        isNull[i] = true;
        binds[i].is_null = &isNull[i];
        stringBuffers[i].reset();
    }
}

unsigned int PreparedStatement::getParameterCount() const {
    return stmt ? mysql_stmt_param_count(stmt) : 0;
}

//--------------------
// MysqlDB Implementation
//--------------------

MysqlDB::MysqlDB(const ConnectionParams& params)
    : db(nullptr), params(params), autoReconnect(params.autoReconnect) {
    if (!connect()) {
        throw MySQLException("Failed to connect to database");
    }
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

    if (!connect()) {
        throw MySQLException("Failed to connect to database");
    }
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

void MysqlDB::configureConnection() {
    if (!db)
        return;

    my_bool reconnect = autoReconnect ? 1 : 0;
    mysql_options(db, MYSQL_OPT_RECONNECT, &reconnect);

    if (params.connectTimeout > 0) {
        mysql_options(db, MYSQL_OPT_CONNECT_TIMEOUT, &params.connectTimeout);
    }

    if (params.readTimeout > 0) {
        mysql_options(db, MYSQL_OPT_READ_TIMEOUT, &params.readTimeout);
    }

    if (params.writeTimeout > 0) {
        mysql_options(db, MYSQL_OPT_WRITE_TIMEOUT, &params.writeTimeout);
    }

    if (!params.charset.empty()) {
        mysql_options(db, MYSQL_SET_CHARSET_NAME, params.charset.c_str());
    }
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

    configureConnection();

    if (!mysql_real_connect(
            db, params.host.c_str(), params.user.c_str(),
            params.password.c_str(), params.database.c_str(), params.port,
            params.socket.empty() ? nullptr : params.socket.c_str(),
            params.clientFlag)) {
        handleError("Failed to connect to database", true);
        return false;
    }

    spdlog::info("Connected to MySQL database: {}@{}:{}/{}", params.user,
                 params.host, params.port, params.database);
    return true;
}

bool MysqlDB::reconnect() {
    std::lock_guard<std::mutex> lock(mutex);

    if (db) {
        if (mysql_ping(db) == 0) {
            return true;
        }
    }

    spdlog::warn("Connection lost, attempting to reconnect...");
    return connect();
}

void MysqlDB::disconnect() {
    std::lock_guard<std::mutex> lock(mutex);

    if (db) {
        mysql_close(db);
        db = nullptr;
        spdlog::debug("Disconnected from MySQL database");
    }
}

bool MysqlDB::isConnected() {
    std::lock_guard<std::mutex> lock(mutex);
    return db && mysql_ping(db) == 0;
}

bool MysqlDB::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db && !reconnect()) {
        return false;
    }

    if (mysql_query(db, query.c_str()) != 0) {
        return !handleError("Failed to execute query: " + query, false);
    }

    spdlog::debug("Query executed successfully: {}",
                  query.length() > 100 ? query.substr(0, 100) + "..." : query);
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

    int affected = static_cast<int>(mysql_affected_rows(db));
    spdlog::debug("Update query affected {} rows", affected);
    return affected;
}

std::optional<int> MysqlDB::getIntValue(const std::string& query) {
    auto result = executeQueryWithResults(query);
    if (!result || !result->next()) {
        return std::nullopt;
    }
    return result->getCurrentRow().getInt(0);
}

std::optional<double> MysqlDB::getDoubleValue(const std::string& query) {
    auto result = executeQueryWithResults(query);
    if (!result || !result->next()) {
        return std::nullopt;
    }
    return result->getCurrentRow().getDouble(0);
}

std::optional<std::string> MysqlDB::getStringValue(const std::string& query) {
    auto result = executeQueryWithResults(query);
    if (!result || !result->next()) {
        return std::nullopt;
    }
    return result->getCurrentRow().getString(0);
}

bool MysqlDB::handleError(const std::string& operation, bool throwOnError) {
    if (!db) {
        std::string errorMessage = "Not connected to database";
        spdlog::error("{}: {}", operation, errorMessage);

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
    spdlog::error("{}: {} (Error code: {})", operation, errorMessage,
                  errorCode);

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

bool MysqlDB::beginTransaction() {
    bool success = executeQuery("START TRANSACTION");
    if (success) {
        spdlog::debug("Transaction started");
    }
    return success;
}

bool MysqlDB::commitTransaction() {
    bool success = executeQuery("COMMIT");
    if (success) {
        spdlog::debug("Transaction committed");
    }
    return success;
}

bool MysqlDB::rollbackTransaction() {
    bool success = executeQuery("ROLLBACK");
    if (success) {
        spdlog::debug("Transaction rolled back");
    }
    return success;
}

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
            spdlog::error("Invalid transaction isolation level");
            return false;
    }

    return executeQuery(query);
}

bool MysqlDB::executeBatch(const std::vector<std::string>& queries) {
    for (const auto& query : queries) {
        if (!executeQuery(query)) {
            spdlog::error("Batch execution failed at query: {}", query);
            return false;
        }
    }
    spdlog::debug("Batch execution completed successfully, {} queries",
                  queries.size());
    return true;
}

bool MysqlDB::executeBatchTransaction(const std::vector<std::string>& queries) {
    if (!beginTransaction()) {
        return false;
    }

    for (const auto& query : queries) {
        if (!executeQuery(query)) {
            spdlog::error("Batch transaction failed, rolling back at query: {}",
                          query);
            rollbackTransaction();
            return false;
        }
    }

    bool success = commitTransaction();
    if (success) {
        spdlog::debug("Batch transaction completed successfully, {} queries",
                      queries.size());
    }
    return success;
}

void MysqlDB::withTransaction(const std::function<void()>& operations) {
    if (!beginTransaction()) {
        throw MySQLException("Failed to begin transaction");
    }

    try {
        operations();
        if (!commitTransaction()) {
            throw MySQLException("Failed to commit transaction");
        }
    } catch (...) {
        try {
            rollbackTransaction();
        } catch (const std::exception& e) {
            spdlog::critical("Failed to rollback transaction: {}", e.what());
        }
        throw;
    }
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
    std::vector<std::string> databases;

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
    std::vector<std::string> tables;

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
    std::vector<std::string> columns;

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

bool MysqlDB::tableExists(const std::string& tableName) {
    try {
        std::string query =
            "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema "
            "= ? AND table_name = ?";
        auto stmt = prepareStatement(query);
        stmt->bindString(0, params.database);
        stmt->bindString(1, tableName);

        auto result = stmt->executeQuery();
        if (result && result->next()) {
            return result->getCurrentRow().getInt(0) > 0;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error checking table existence: {}", e.what());
    }
    return false;
}

std::string MysqlDB::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex);
    return db ? mysql_error(db) : "Not connected to database";
}

unsigned int MysqlDB::getLastErrorCode() const {
    std::lock_guard<std::mutex> lock(mutex);
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

    std::vector<char> buffer(str.length() * 2 + 1);
    unsigned long length =
        mysql_real_escape_string(db, buffer.data(), str.c_str(), str.length());
    return std::string(buffer.data(), length);
}

unsigned long long MysqlDB::getLastInsertId() const {
    std::lock_guard<std::mutex> lock(mutex);
    return db ? mysql_insert_id(db) : 0;
}

unsigned long long MysqlDB::getAffectedRows() const {
    std::lock_guard<std::mutex> lock(mutex);
    return db ? mysql_affected_rows(db) : 0;
}

std::unique_ptr<ResultSet> MysqlDB::executeQueryWithPagination(
    const std::string& query, int limit, int offset) {
    std::string paginatedQuery = query;

    if (paginatedQuery.find("LIMIT") == std::string::npos) {
        paginatedQuery += " LIMIT " + std::to_string(limit);
    }

    if (paginatedQuery.find("OFFSET") == std::string::npos) {
        paginatedQuery += " OFFSET " + std::to_string(offset);
    }

    return executeQueryWithResults(paginatedQuery);
}

std::string MysqlDB::getServerVersion() const {
    std::lock_guard<std::mutex> lock(mutex);
    return db ? mysql_get_server_info(db) : "Not connected";
}

std::string MysqlDB::getClientVersion() const {
    return mysql_get_client_info();
}

bool MysqlDB::ping() {
    std::lock_guard<std::mutex> lock(mutex);
    return db && mysql_ping(db) == 0;
}

bool MysqlDB::setConnectionTimeout(unsigned int timeout) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!db) {
        params.connectTimeout = timeout;
        return true;
    }

    return mysql_options(db, MYSQL_OPT_CONNECT_TIMEOUT, &timeout) == 0;
}

}  // namespace database
}  // namespace atom
