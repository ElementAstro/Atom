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

// Implementation of remaining methods...

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

}  // namespace database
}  // namespace atom