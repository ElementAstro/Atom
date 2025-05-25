/*
 * sqlite.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "sqlite.hpp"

#include <sqlite3.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <spdlog/spdlog.h>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

class StatementCache {
public:
    struct CachedStatement {
        sqlite3_stmt* stmt = nullptr;
        std::chrono::steady_clock::time_point lastUsed;
    };

    explicit StatementCache(size_t maxSize = 50) : maxCacheSize(maxSize) {}

    ~StatementCache() { clear(); }

    sqlite3_stmt* get(sqlite3* db, std::string_view query) {
        String queryStr(query);
        auto it = cache.find(queryStr);

        if (it != cache.end()) {
            it->second.lastUsed = std::chrono::steady_clock::now();
            sqlite3_reset(it->second.stmt);
            sqlite3_clear_bindings(it->second.stmt);
            return it->second.stmt;
        }

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, queryStr.c_str(),
                                    static_cast<int>(queryStr.size()), &stmt,
                                    nullptr);

        if (rc != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}, Query: {}",
                          sqlite3_errmsg(db), queryStr.c_str());
            return nullptr;
        }

        if (cache.size() >= maxCacheSize) {
            evictOldest();
        }

        CachedStatement cached;
        cached.stmt = stmt;
        cached.lastUsed = std::chrono::steady_clock::now();
        cache[queryStr] = cached;

        return stmt;
    }

    void remove(std::string_view query) {
        String queryStr(query);
        auto it = cache.find(queryStr);
        if (it != cache.end()) {
            sqlite3_finalize(it->second.stmt);
            cache.erase(it);
        }
    }

    void clear() {
        for (auto& pair : cache) {
            if (pair.second.stmt) {
                sqlite3_finalize(pair.second.stmt);
            }
        }
        cache.clear();
    }

private:
    void evictOldest() {
        if (cache.empty())
            return;

        auto oldest = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.lastUsed < oldest->second.lastUsed) {
                oldest = it;
            }
        }

        if (oldest != cache.end()) {
            if (oldest->second.stmt) {
                sqlite3_finalize(oldest->second.stmt);
            }
            cache.erase(oldest);
        }
    }

    HashMap<String, CachedStatement> cache;
    size_t maxCacheSize;
};

inline void bindParameters(sqlite3_stmt* /*stmt*/, int /*index*/) {}

template <typename T, typename... Args>
void bindParameters(sqlite3_stmt* stmt, int index, T&& value, Args&&... args) {
    int rc = SQLITE_OK;
    using DecayedT = std::decay_t<T>;

    if constexpr (std::is_same_v<DecayedT, int>) {
        rc = sqlite3_bind_int(stmt, index, value);
    } else if constexpr (std::is_same_v<DecayedT, int64_t>) {
        rc = sqlite3_bind_int64(stmt, index, value);
    } else if constexpr (std::is_same_v<DecayedT, double>) {
        rc = sqlite3_bind_double(stmt, index, value);
    } else if constexpr (std::is_same_v<DecayedT, const char*>) {
        rc = sqlite3_bind_text(stmt, index, value, -1, SQLITE_STATIC);
    } else if constexpr (std::is_same_v<DecayedT, String> ||
                         std::is_same_v<DecayedT, std::string>) {
        rc =
            sqlite3_bind_text(stmt, index, value.c_str(),
                              static_cast<int>(value.size()), SQLITE_TRANSIENT);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        rc =
            sqlite3_bind_text(stmt, index, value.data(),
                              static_cast<int>(value.size()), SQLITE_TRANSIENT);
    } else if constexpr (std::is_null_pointer_v<DecayedT>) {
        rc = sqlite3_bind_null(stmt, index);
    } else {
        throw std::runtime_error(
            "Unsupported parameter type for SQLite binding");
    }

    if (rc != SQLITE_OK) {
        throw SQLiteException(String("Failed to bind parameter at index ") +
                              String(std::to_string(index)) + ": " +
                              sqlite3_errmsg(sqlite3_db_handle(stmt)));
    }

    bindParameters(stmt, index + 1, std::forward<Args>(args)...);
}

class SqliteDB::Impl {
public:
    sqlite3* db{nullptr};
    std::function<void(std::string_view)> errorCallback;
    std::atomic<bool> inTransaction{false};
    StatementCache stmtCache;

    Impl()
        : errorCallback([](std::string_view msg) {
              spdlog::error("SQLite Error: {}", msg);
          }) {}

    ~Impl() {
        try {
            if (db != nullptr) {
                int rc = sqlite3_close_v2(db);
                if (rc != SQLITE_OK) {
                    spdlog::error("Failed to close database cleanly: {}",
                                  sqlite3_errmsg(db));
                } else {
                    spdlog::debug("Database closed successfully");
                }
                db = nullptr;
            }
        } catch (...) {
            spdlog::error("Unknown exception during database cleanup");
        }
    }

    bool open(std::string_view dbPath) {
        if (dbPath.empty()) {
            errorCallback("Database path cannot be empty");
            return false;
        }

        String dbPathStr(dbPath);

        try {
            int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_FULLMUTEX;
            int rc = sqlite3_open_v2(dbPathStr.c_str(), &db, flags, nullptr);

            if (rc != SQLITE_OK) {
                String error = sqlite3_errmsg(db);
                errorCallback(error);
                sqlite3_close(db);
                db = nullptr;
                return false;
            }

            executeSimple("PRAGMA journal_mode = WAL");
            executeSimple("PRAGMA synchronous = NORMAL");
            executeSimple("PRAGMA cache_size = -10000");
            executeSimple("PRAGMA foreign_keys = ON");
            executeSimple("PRAGMA busy_timeout = 5000");

            spdlog::debug("Opened database: {}", dbPathStr.c_str());
            return true;
        } catch (const std::exception& e) {
            errorCallback(e.what());
            if (db) {
                sqlite3_close(db);
                db = nullptr;
            }
            return false;
        }
    }

    bool executeSimple(std::string_view query) {
        if (!db) {
            errorCallback("Database not connected for executeSimple");
            return false;
        }

        String queryStr(query);
        char* errorMessage = nullptr;
        int rc =
            sqlite3_exec(db, queryStr.c_str(), nullptr, nullptr, &errorMessage);

        if (rc != SQLITE_OK) {
            String error = errorMessage ? String(errorMessage)
                                        : String("Unknown SQLite error");
            errorCallback(error);
            sqlite3_free(errorMessage);
            return false;
        }

        return true;
    }

    String getLastError() const {
        return db ? String(sqlite3_errmsg(db))
                  : String("Database not connected");
    }
};

SqliteDB::SqliteDB(std::string_view dbPath) : pImpl(std::make_unique<Impl>()) {
    if (!pImpl->open(dbPath)) {
        throw SQLiteException(String("Failed to open database: ") +
                              String(dbPath));
    }
}

SqliteDB::~SqliteDB() = default;

SqliteDB::SqliteDB(SqliteDB&& other) noexcept
    : pImpl(std::move(other.pImpl)), mtx() {}

SqliteDB& SqliteDB::operator=(SqliteDB&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mtx, other.mtx);
        pImpl = nullptr;
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

void SqliteDB::validateQueryString(std::string_view query) const {
    if (query.empty()) {
        throw SQLiteException("Query string cannot be empty");
    }

    if (query.find("--") != std::string_view::npos) {
        spdlog::warn("Query contains '--': {}", query);
    }

    size_t firstSemicolon = query.find(';');
    if (firstSemicolon != std::string_view::npos &&
        firstSemicolon < query.size() - 1) {
        throw SQLiteException(
            "Multiple SQL statements (;) are not allowed in a single query");
    }
}

void SqliteDB::checkConnection() const {
    if (!pImpl || !pImpl->db) {
        throw SQLiteException("Database is not connected");
    }
}

bool SqliteDB::executeQuery(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();
        validateQueryString(query);

        if (!pImpl->executeSimple(query)) {
            throw SQLiteException(pImpl->getLastError());
        }

        return true;
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        String error = "Error executing query: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

template <typename... Args>
bool SqliteDB::executeParameterizedQuery(std::string_view query,
                                         Args&&... params) {
    sqlite3_stmt* stmt = nullptr;
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            throw SQLiteException(String("Failed to prepare statement: ") +
                                  pImpl->getLastError());
        }

        bindParameters(stmt, 1, std::forward<Args>(params)...);

        int rc = sqlite3_step(stmt);

        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            String error = String("Failed to execute parameterized query: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            throw SQLiteException(error);
        }

        sqlite3_reset(stmt);
        return true;
    } catch (const SQLiteException& e) {
        if (pImpl)
            pImpl->errorCallback(e.what());
        throw;
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);
        String error = "Error executing parameterized query: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

template bool SqliteDB::executeParameterizedQuery<>(std::string_view query);
template bool SqliteDB::executeParameterizedQuery<int>(std::string_view query,
                                                       int&&);
template bool SqliteDB::executeParameterizedQuery<double>(
    std::string_view query, double&&);
template bool SqliteDB::executeParameterizedQuery<const char*>(
    std::string_view query, const char*&&);
template bool SqliteDB::executeParameterizedQuery<String>(
    std::string_view query, String&&);
template bool SqliteDB::executeParameterizedQuery<std::string_view>(
    std::string_view query, std::string_view&&);

SqliteDB::ResultSet SqliteDB::selectData(std::string_view query) {
    sqlite3_stmt* stmt = nullptr;
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            throw SQLiteException(String("Failed to prepare query: ") +
                                  pImpl->getLastError());
        }

        ResultSet results;
        int columnCount = sqlite3_column_count(stmt);
        int rc;

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            RowData row;
            row.reserve(columnCount);

            for (int i = 0; i < columnCount; ++i) {
                const unsigned char* value_uchar = sqlite3_column_text(stmt, i);
                if (value_uchar) {
                    row.emplace_back(
                        reinterpret_cast<const char*>(value_uchar));
                } else {
                    row.emplace_back(String());
                }
            }
            results.push_back(std::move(row));
        }

        if (rc != SQLITE_DONE) {
            String error = String("Error fetching data: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            throw SQLiteException(error);
        }

        sqlite3_reset(stmt);
        return results;
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);
        String error = "Error selecting data: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

template <typename... Args>
SqliteDB::ResultSet SqliteDB::selectParameterizedData(std::string_view query,
                                                      Args&&... params) {
    sqlite3_stmt* stmt = nullptr;
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            throw SQLiteException(String("Failed to prepare query: ") +
                                  pImpl->getLastError());
        }

        bindParameters(stmt, 1, std::forward<Args>(params)...);

        ResultSet results;
        int columnCount = sqlite3_column_count(stmt);
        int rc;

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            RowData row;
            row.reserve(columnCount);

            for (int i = 0; i < columnCount; ++i) {
                const unsigned char* value_uchar = sqlite3_column_text(stmt, i);
                if (value_uchar) {
                    row.emplace_back(
                        reinterpret_cast<const char*>(value_uchar));
                } else {
                    row.emplace_back(String());
                }
            }
            results.push_back(std::move(row));
        }

        if (rc != SQLITE_DONE) {
            String error = String("Error fetching parameterized data: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            throw SQLiteException(error);
        }

        sqlite3_reset(stmt);
        return results;
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);
        String error = "Error selecting parameterized data: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

template <typename T>
std::optional<T> SqliteDB::getSingleValue(std::string_view query,
                                          T (*columnFunc)(sqlite3_stmt*, int)) {
    sqlite3_stmt* stmt = nullptr;
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            pImpl->errorCallback(
                String("Failed to prepare query for single value: ") +
                pImpl->getLastError());
            return std::nullopt;
        }

        std::optional<T> result;
        int rc = sqlite3_step(stmt);

        if (rc == SQLITE_ROW) {
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                result = columnFunc(stmt, 0);
            }
            while (sqlite3_step(stmt) == SQLITE_ROW)
                ;
        } else if (rc != SQLITE_DONE) {
            String error = String("Error getting single value: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            pImpl->errorCallback(error);
            return std::nullopt;
        }

        sqlite3_reset(stmt);
        return result;
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);
        String error = "Error getting single value: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        return std::nullopt;
    }
}

std::optional<int> SqliteDB::getIntValue(std::string_view query) {
    return getSingleValue<int>(query, sqlite3_column_int);
}

std::optional<double> SqliteDB::getDoubleValue(std::string_view query) {
    return getSingleValue<double>(query, sqlite3_column_double);
}

std::optional<String> SqliteDB::getTextValue(std::string_view query) {
    auto getTextFunc = [](sqlite3_stmt* stmt, int col) -> String {
        const unsigned char* text = sqlite3_column_text(stmt, col);
        return text ? String(reinterpret_cast<const char*>(text)) : String();
    };
    return getSingleValue<String>(query, getTextFunc);
}

bool SqliteDB::searchData(std::string_view query, std::string_view searchTerm) {
    if (searchTerm.empty()) {
        if (pImpl)
            pImpl->errorCallback("Search term cannot be empty");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            pImpl->errorCallback(String("Failed to prepare search query: ") +
                                 pImpl->getLastError());
            return false;
        }

        int rc_bind = sqlite3_bind_text(stmt, 1, searchTerm.data(),
                                        static_cast<int>(searchTerm.size()),
                                        SQLITE_TRANSIENT);
        if (rc_bind != SQLITE_OK) {
            String error = String("Failed to bind search parameter: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            pImpl->errorCallback(error);
            return false;
        }

        int rc_step = sqlite3_step(stmt);
        sqlite3_reset(stmt);

        if (rc_step == SQLITE_ROW) {
            while (rc_step == SQLITE_ROW) {
                rc_step = sqlite3_step(stmt);
            }
            sqlite3_reset(stmt);
            return true;
        } else if (rc_step == SQLITE_DONE) {
            return false;
        } else {
            pImpl->errorCallback(String("Error during search execution: ") +
                                 String(sqlite3_errmsg(pImpl->db)));
            return false;
        }
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);
        String error = "Error during search: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        return false;
    }
}

int SqliteDB::executeAndGetChanges(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();
        validateQueryString(query);

        if (!pImpl->executeSimple(query)) {
            throw SQLiteException(pImpl->getLastError());
        }

        return sqlite3_changes(pImpl->db);
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        String error = "Error executing update/delete: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

int SqliteDB::updateData(std::string_view query) {
    return executeAndGetChanges(query);
}

int SqliteDB::deleteData(std::string_view query) {
    return executeAndGetChanges(query);
}

void SqliteDB::beginTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        if (pImpl->inTransaction.load()) {
            throw SQLiteException("Transaction already in progress");
        }

        if (!pImpl->executeSimple("BEGIN IMMEDIATE TRANSACTION")) {
            throw SQLiteException(String("Failed to begin transaction: ") +
                                  pImpl->getLastError());
        }

        pImpl->inTransaction.store(true);
        spdlog::debug("Transaction started");
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        String error = "Error starting transaction: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

void SqliteDB::commitTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        if (!pImpl->inTransaction.load()) {
            throw SQLiteException("No transaction in progress to commit");
        }

        if (!pImpl->executeSimple("COMMIT TRANSACTION")) {
            spdlog::error("Commit failed, attempting rollback...");
            ATOM_UNUSED_RESULT(pImpl->executeSimple("ROLLBACK TRANSACTION"));
            pImpl->inTransaction.store(false);
            throw SQLiteException(
                String("Failed to commit transaction (rolled back): ") +
                pImpl->getLastError());
        }

        pImpl->inTransaction.store(false);
        spdlog::debug("Transaction committed");
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        String error = "Error committing transaction: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        if (pImpl && pImpl->inTransaction.load()) {
            spdlog::error("Exception during commit, attempting rollback...");
            ATOM_UNUSED_RESULT(pImpl->executeSimple("ROLLBACK TRANSACTION"));
            pImpl->inTransaction.store(false);
        }
        throw SQLiteException(error);
    }
}

void SqliteDB::rollbackTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        if (!pImpl || !pImpl->db) {
            spdlog::error("Rollback attempted on disconnected database");
            return;
        }

        if (!pImpl->inTransaction.load()) {
            spdlog::warn("No transaction in progress to rollback");
            return;
        }

        spdlog::debug("Rolling back transaction...");
        ATOM_UNUSED_RESULT(pImpl->executeSimple("ROLLBACK TRANSACTION"));
        pImpl->inTransaction.store(false);
    } catch (const std::exception& e) {
        spdlog::critical("CRITICAL: Exception during transaction rollback: {}",
                         e.what());
        if (pImpl)
            pImpl->inTransaction.store(false);
    } catch (...) {
        spdlog::critical(
            "CRITICAL: Unknown exception during transaction rollback");
        if (pImpl)
            pImpl->inTransaction.store(false);
    }
}

void SqliteDB::withTransaction(const std::function<void()>& operations) {
    beginTransaction();
    try {
        operations();
        commitTransaction();
    } catch (...) {
        try {
            rollbackTransaction();
        } catch (...) {
            spdlog::critical(
                "CRITICAL: Exception during rollback within withTransaction");
        }
        throw;
    }
}

bool SqliteDB::validateData(std::string_view query,
                            std::string_view validationQuery) {
    try {
        if (!executeQuery(query)) {
            return false;
        }

        auto validationResult = getIntValue(validationQuery);
        return validationResult.value_or(0) != 0;
    } catch (const std::exception& e) {
        String error = "Error validating data: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        return false;
    }
}

SqliteDB::ResultSet SqliteDB::selectDataWithPagination(std::string_view query,
                                                       int limit, int offset) {
    if (limit <= 0) {
        throw SQLiteException("Pagination limit must be positive");
    }
    if (offset < 0) {
        throw SQLiteException("Pagination offset cannot be negative");
    }

    try {
        String queryWithPagination(query);

        if (queryWithPagination.find(" LIMIT ") != String::npos ||
            queryWithPagination.find(" limit ") != String::npos) {
            throw SQLiteException("Query already contains a LIMIT clause");
        }

        queryWithPagination += " LIMIT ";
        queryWithPagination += String(std::to_string(limit));
        queryWithPagination += " OFFSET ";
        queryWithPagination += String(std::to_string(offset));

        return selectData(queryWithPagination);
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        String error = "Error in paginated query: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

void SqliteDB::setErrorMessageCallback(
    const std::function<void(std::string_view)>& errorCallback) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (pImpl) {
        pImpl->errorCallback = errorCallback;
    }
}

bool SqliteDB::isConnected() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return pImpl && pImpl->db != nullptr;
}

int64_t SqliteDB::getLastInsertRowId() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    checkConnection();
    return sqlite3_last_insert_rowid(pImpl->db);
}

int SqliteDB::getChanges() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    checkConnection();
    return sqlite3_changes(pImpl->db);
}

int SqliteDB::getTotalChanges() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    checkConnection();
    return sqlite3_total_changes(pImpl->db);
}

bool SqliteDB::tableExists(std::string_view tableName) {
    try {
        String query =
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?";
        auto result = getSingleValue<int>(query.c_str(), sqlite3_column_int);
        return result.value_or(0) > 0;
    } catch (const std::exception& e) {
        spdlog::error("Error checking table existence: {}", e.what());
        return false;
    }
}

SqliteDB::ResultSet SqliteDB::getTableSchema(std::string_view tableName) {
    String query = "PRAGMA table_info(";
    query += String(tableName);
    query += ")";
    return selectData(query);
}

bool SqliteDB::vacuum() {
    try {
        return executeQuery("VACUUM");
    } catch (const std::exception& e) {
        spdlog::error("Error executing VACUUM: {}", e.what());
        return false;
    }
}

bool SqliteDB::analyze() {
    try {
        return executeQuery("ANALYZE");
    } catch (const std::exception& e) {
        spdlog::error("Error executing ANALYZE: {}", e.what());
        return false;
    }
}

template std::optional<int> SqliteDB::getSingleValue<int>(
    std::string_view query, int (*columnFunc)(sqlite3_stmt*, int));
template std::optional<double> SqliteDB::getSingleValue<double>(
    std::string_view query, double (*columnFunc)(sqlite3_stmt*, int));