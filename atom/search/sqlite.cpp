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

#include "atom/log/loguru.hpp"

// SIMD optimization headers
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

// SQLite statement cache (prepared statement reuse)
class StatementCache {
public:
    struct CachedStatement {
        sqlite3_stmt* stmt = nullptr;
        std::chrono::steady_clock::time_point lastUsed;
    };

    explicit StatementCache(size_t maxSize = 50) : maxCacheSize(maxSize) {}

    ~StatementCache() { clear(); }

    sqlite3_stmt* get(sqlite3* db, std::string_view query) {
        std::string queryStr(query);
        auto it = cache.find(queryStr);

        if (it != cache.end()) {
            it->second.lastUsed = std::chrono::steady_clock::now();
            return it->second.stmt;
        }

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(
            db, query.data(), static_cast<int>(query.size()), &stmt, nullptr);

        if (rc != SQLITE_OK) {
            return nullptr;
        }

        // Manage cache size
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
        std::string queryStr(query);
        auto it = cache.find(queryStr);
        if (it != cache.end()) {
            sqlite3_finalize(it->second.stmt);
            cache.erase(it);
        }
    }

    void clear() {
        for (auto& pair : cache) {
            sqlite3_finalize(pair.second.stmt);
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
            sqlite3_finalize(oldest->second.stmt);
            cache.erase(oldest);
        }
    }

    std::unordered_map<std::string, CachedStatement> cache;
    size_t maxCacheSize;
};

// Implementation details
class SqliteDB::Impl {
public:
    sqlite3* db{nullptr};
    std::function<void(std::string_view)> errorCallback;
    std::atomic<bool> inTransaction{false};
    StatementCache stmtCache;

    Impl()
        : errorCallback([](std::string_view msg) { LOG_F(ERROR, "{}", msg); }) {
    }

    ~Impl() {
        try {
            stmtCache.clear();
            if (db != nullptr) {
                sqlite3_close_v2(
                    db);  // Using sqlite3_close_v2 for safer cleanup
                DLOG_F(INFO, "Database closed");
            }
        } catch (...) {
            // Ensure no exceptions escape destructor
            LOG_F(ERROR, "Exception during database cleanup");
        }
    }

    bool open(std::string_view dbPath) {
        if (dbPath.empty()) {
            errorCallback("Database path cannot be empty");
            return false;
        }

        try {
            int rc =
                sqlite3_open_v2(dbPath.data(), &db,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                    SQLITE_OPEN_FULLMUTEX,
                                nullptr);

            if (rc != SQLITE_OK) {
                std::string error = sqlite3_errmsg(db);
                errorCallback(error);
                sqlite3_close(db);
                db = nullptr;
                return false;
            }

            // Configure SQLite for better performance
            executeSimple("PRAGMA journal_mode = WAL");
            executeSimple("PRAGMA synchronous = NORMAL");
            executeSimple("PRAGMA cache_size = 10000");
            executeSimple("PRAGMA foreign_keys = ON");

            DLOG_F(INFO, "Opened database: {}", dbPath);
            return true;
        } catch (const std::exception& e) {
            errorCallback(e.what());
            return false;
        }
    }

    bool executeSimple(std::string_view query) {
        if (!db)
            return false;

        char* errorMessage = nullptr;
        int rc =
            sqlite3_exec(db, query.data(), nullptr, nullptr, &errorMessage);

        if (rc != SQLITE_OK) {
            std::string error = errorMessage ? errorMessage : "Unknown error";
            errorCallback(error);
            sqlite3_free(errorMessage);
            return false;
        }

        return true;
    }

    std::string getLastError() const {
        return db ? sqlite3_errmsg(db) : "Database not connected";
    }
};

// Constructor and destructor
SqliteDB::SqliteDB(std::string_view dbPath) : pImpl(std::make_unique<Impl>()) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    if (!pImpl->open(dbPath)) {
        throw SQLiteException("Failed to open database: " +
                              std::string(dbPath));
    }
}

SqliteDB::~SqliteDB() = default;

// Move operations
SqliteDB::SqliteDB(SqliteDB&& other) noexcept : pImpl(std::move(other.pImpl)) {}

SqliteDB& SqliteDB::operator=(SqliteDB&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

// Helper methods
void SqliteDB::validateQueryString(std::string_view query) const {
    if (query.empty()) {
        throw SQLiteException("Query string cannot be empty");
    }

    // Simple SQL injection check - this is basic and should be expanded
    if (query.find("--") != std::string_view::npos ||
        query.find(";") != query.rfind(";")) {
        throw SQLiteException("Potentially unsafe query detected");
    }
}

void SqliteDB::checkConnection() const {
    if (!pImpl->db) {
        throw SQLiteException("Database is not connected");
    }
}

// Core operations
bool SqliteDB::executeQuery(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        char* errorMessage = nullptr;
        int rc = sqlite3_exec(pImpl->db, query.data(), nullptr, nullptr,
                              &errorMessage);

        if (rc != SQLITE_OK) {
            std::string error =
                errorMessage ? errorMessage : "Unknown SQLite error";
            pImpl->errorCallback(error);
            sqlite3_free(errorMessage);
            throw SQLiteException(error);
        }

        return true;
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        std::string error = "Error executing query: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

template <typename... Args>
bool SqliteDB::executeParameterizedQuery(std::string_view query,
                                         Args&&... params) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        sqlite3_stmt* stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            throw SQLiteException("Failed to prepare statement: " +
                                  pImpl->getLastError());
        }

        // Reset statement for reuse
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        // Bind parameters - implementation varies based on parameter types
        if constexpr (sizeof...(params) > 0) {
            bindParameters(stmt, 1, std::forward<Args>(params)...);
        }

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            throw SQLiteException("Failed to execute parameterized query: " +
                                  pImpl->getLastError());
        }

        return true;
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        std::string error = "Error executing parameterized query: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

// Bind parameter implementation would go here (for different parameter types)
// This is a helper function for the template above

SqliteDB::ResultSet SqliteDB::selectData(std::string_view query) {
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        ResultSet results;
        sqlite3_stmt* stmt = nullptr;

        int rc =
            sqlite3_prepare_v2(pImpl->db, query.data(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw SQLiteException("Failed to prepare query: " +
                                  pImpl->getLastError());
        }

        // Use a smart pointer for RAII cleanup of statement
        auto stmtGuard =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(
                stmt, sqlite3_finalize);

        // Get column count
        int columnCount = sqlite3_column_count(stmt);

        // Fetch all rows
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            RowData row;
            row.reserve(columnCount);

            for (int i = 0; i < columnCount; ++i) {
                // Get column value
                const char* value =
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                row.emplace_back(value ? value : "");
            }

            results.push_back(std::move(row));
        }

        if (rc != SQLITE_DONE) {
            throw SQLiteException("Error fetching data: " +
                                  pImpl->getLastError());
        }

        return results;
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        std::string error = "Error selecting data: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

std::optional<int> SqliteDB::getIntValue(std::string_view query) {
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        sqlite3_stmt* stmt = nullptr;
        std::optional<int> result;

        int rc =
            sqlite3_prepare_v2(pImpl->db, query.data(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            pImpl->errorCallback(pImpl->getLastError());
            return std::nullopt;
        }

        auto stmtGuard =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(
                stmt, sqlite3_finalize);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                result = sqlite3_column_int(stmt, 0);
            }
        }

        return result;
    } catch (const std::exception& e) {
        std::string error = "Error getting int value: ";
        error += e.what();
        pImpl->errorCallback(error);
        return std::nullopt;
    }
}

std::optional<double> SqliteDB::getDoubleValue(std::string_view query) {
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        sqlite3_stmt* stmt = nullptr;
        std::optional<double> result;

        int rc =
            sqlite3_prepare_v2(pImpl->db, query.data(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            pImpl->errorCallback(pImpl->getLastError());
            return std::nullopt;
        }

        auto stmtGuard =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(
                stmt, sqlite3_finalize);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                result = sqlite3_column_double(stmt, 0);
            }
        }

        return result;
    } catch (const std::exception& e) {
        std::string error = "Error getting double value: ";
        error += e.what();
        pImpl->errorCallback(error);
        return std::nullopt;
    }
}

std::optional<std::string> SqliteDB::getTextValue(std::string_view query) {
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        sqlite3_stmt* stmt = nullptr;
        std::optional<std::string> result;

        int rc =
            sqlite3_prepare_v2(pImpl->db, query.data(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            pImpl->errorCallback(pImpl->getLastError());
            return std::nullopt;
        }

        auto stmtGuard =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(
                stmt, sqlite3_finalize);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                const char* text =
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (text) {
                    result = std::string(text);
                }
            }
        }

        return result;
    } catch (const std::exception& e) {
        std::string error = "Error getting text value: ";
        error += e.what();
        pImpl->errorCallback(error);
        return std::nullopt;
    }
}

bool SqliteDB::searchData(std::string_view query, std::string_view searchTerm) {
    if (searchTerm.empty()) {
        pImpl->errorCallback("Search term cannot be empty");
        return false;
    }

    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        sqlite3_stmt* stmt = nullptr;
        int rc =
            sqlite3_prepare_v2(pImpl->db, query.data(), -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            pImpl->errorCallback(pImpl->getLastError());
            return false;
        }

        auto stmtGuard =
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>(
                stmt, sqlite3_finalize);

        // Bind the search term
        rc = sqlite3_bind_text(stmt, 1, searchTerm.data(),
                               static_cast<int>(searchTerm.size()),
                               SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            pImpl->errorCallback("Failed to bind search parameter: " +
                                 pImpl->getLastError());
            return false;
        }

        // Execute the search
        rc = sqlite3_step(stmt);

        // SQLITE_ROW means we found at least one matching row
        return rc == SQLITE_ROW;
    } catch (const std::exception& e) {
        std::string error = "Error during search: ";
        error += e.what();
        pImpl->errorCallback(error);
        return false;
    }
}

int SqliteDB::updateData(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        if (!executeQuery(query)) {
            return 0;
        }

        return sqlite3_changes(pImpl->db);
    } catch (const std::exception& e) {
        std::string error = "Error updating data: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

int SqliteDB::deleteData(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        if (!executeQuery(query)) {
            return 0;
        }

        return sqlite3_changes(pImpl->db);
    } catch (const std::exception& e) {
        std::string error = "Error deleting data: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

void SqliteDB::beginTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        if (pImpl->inTransaction) {
            throw SQLiteException("Transaction already in progress");
        }

        if (!executeQuery("BEGIN TRANSACTION")) {
            throw SQLiteException("Failed to begin transaction: " +
                                  pImpl->getLastError());
        }

        pImpl->inTransaction = true;
    } catch (const std::exception& e) {
        std::string error = "Error starting transaction: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

void SqliteDB::commitTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        if (!pImpl->inTransaction) {
            throw SQLiteException("No transaction in progress to commit");
        }

        if (!executeQuery("COMMIT TRANSACTION")) {
            throw SQLiteException("Failed to commit transaction: " +
                                  pImpl->getLastError());
        }

        pImpl->inTransaction = false;
    } catch (const std::exception& e) {
        std::string error = "Error committing transaction: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

void SqliteDB::rollbackTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        if (!pImpl->inTransaction) {
            DLOG_F(WARNING, "No transaction in progress to rollback");
            return;
        }

        executeQuery("ROLLBACK TRANSACTION");
        pImpl->inTransaction = false;
    } catch (const std::exception& e) {
        // Log but don't throw from rollback
        std::string error = "Error during transaction rollback: ";
        error += e.what();
        pImpl->errorCallback(error);
        pImpl->inTransaction = false;  // Reset transaction state anyway
    }
}

void SqliteDB::withTransaction(const std::function<void()>& operations) {
    try {
        beginTransaction();
        operations();
        commitTransaction();
    } catch (const std::exception& e) {
        rollbackTransaction();
        throw;  // Re-throw the exception after rollback
    }
}

bool SqliteDB::validateData(std::string_view query,
                            std::string_view validationQuery) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        validateQueryString(validationQuery);
        checkConnection();

        if (!executeQuery(query)) {
            return false;
        }

        auto validationResult = getIntValue(validationQuery);
        return validationResult.value_or(0) > 0;
    } catch (const std::exception& e) {
        std::string error = "Error validating data: ";
        error += e.what();
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
        std::shared_lock<std::shared_mutex> lock(mtx);
        validateQueryString(query);
        checkConnection();

        // Build paginated query
        std::string queryWithPagination = std::string(query);

        // Check if query already has LIMIT clause
        auto limitPos = queryWithPagination.find(" LIMIT ");
        if (limitPos != std::string::npos) {
            throw SQLiteException("Query already contains a LIMIT clause");
        }

        queryWithPagination += " LIMIT " + std::to_string(limit) + " OFFSET " +
                               std::to_string(offset);

        return selectData(queryWithPagination);
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        std::string error = "Error in paginated query: ";
        error += e.what();
        pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

void SqliteDB::setErrorMessageCallback(
    const std::function<void(std::string_view)>& errorCallback) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    pImpl->errorCallback = errorCallback;
}

bool SqliteDB::isConnected() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return pImpl->db != nullptr;
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