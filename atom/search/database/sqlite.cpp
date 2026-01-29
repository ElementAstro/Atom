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
#include "base.hpp"
#include "retry.hpp"
#include "statement_cache.hpp"

using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;
using atom::search::database::QueryValidator;
using atom::search::database::RetryExecutor;
using atom::search::database::RetryPolicy;

namespace atom::search {

/**
 * @brief SQLite-specific statement cache using the generic StatementCache.
 *
 * This class wraps the generic StatementCache template with SQLite-specific
 * statement preparation and finalization logic.
 */
class SqliteStatementCache {
public:
    explicit SqliteStatementCache(size_t maxSize = 50)
        : cache_(maxSize, [](sqlite3_stmt* stmt) {
              if (stmt) {
                  sqlite3_finalize(stmt);
              }
          }) {}

    ~SqliteStatementCache() = default;

    sqlite3_stmt* get(sqlite3* db, std::string_view query) {
        // Try to get from cache first
        sqlite3_stmt* cached = cache_.get(query);
        if (cached) {
            sqlite3_reset(cached);
            sqlite3_clear_bindings(cached);
            return cached;
        }

        // Prepare new statement
        sqlite3_stmt* stmt = nullptr;
        String queryStr(query);
        int rc = sqlite3_prepare_v2(db, queryStr.c_str(),
                                    static_cast<int>(queryStr.size()), &stmt,
                                    nullptr);

        if (rc != SQLITE_OK) {
            String error = String("Failed to prepare statement: ") +
                           String(sqlite3_errmsg(db));
            spdlog::error("{}, Query: {}", error.c_str(), queryStr.c_str());
            throw SQLiteException(error, rc);
        }

        // Add to cache
        cache_.put(query, stmt);
        return stmt;
    }

    void remove(std::string_view query) { cache_.remove(query); }

    void clear() { cache_.clear(); }

    auto getStats() const { return cache_.getStats(); }

private:
    atom::search::database::StatementCache<sqlite3_stmt> cache_;
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
                                  sqlite3_errmsg(sqlite3_db_handle(stmt)),
                              rc);
    }

    bindParameters(stmt, index + 1, std::forward<Args>(args)...);
}

class SqliteDB::Impl {
public:
    sqlite3* db{nullptr};
    std::function<void(std::string_view)> errorCallback;
    std::atomic<bool> inTransaction{false};
    SqliteStatementCache stmtCache;

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

    void executeSimpleOrThrow(std::string_view query) {
        if (!db) {
            throw SQLiteException("Database not connected", SQLITE_MISUSE);
        }

        String queryStr(query);
        char* errorMessage = nullptr;
        int rc =
            sqlite3_exec(db, queryStr.c_str(), nullptr, nullptr, &errorMessage);

        if (rc != SQLITE_OK) {
            String error = errorMessage ? String(errorMessage)
                                        : String(sqlite3_errmsg(db));
            sqlite3_free(errorMessage);
            throw SQLiteException(error, rc);
        }
    }

    String getLastError() const {
        return db ? String(sqlite3_errmsg(db))
                  : String("Database not connected");
    }
};

namespace {

[[nodiscard]] bool shouldRetrySqlite(const std::exception& e) {
    const auto* dbEx =
        dynamic_cast<const atom::search::database::DatabaseException*>(&e);
    if (!dbEx) {
        return false;
    }

    switch (dbEx->errorCode()) {
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
        case SQLITE_IOERR:
        case SQLITE_PROTOCOL:
            return true;
        default:
            return false;
    }
}

}  // namespace

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
    QueryValidator::validate(query);
}

void SqliteDB::checkConnection() const {
    if (!pImpl || !pImpl->db) {
        throw SQLiteException("Database is not connected");
    }
}

bool SqliteDB::executeQuery(std::string_view query) {
    try {
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                std::unique_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);
                pImpl->executeSimpleOrThrow(query);
                return true;
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
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
    try {
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                sqlite3_stmt* stmt = nullptr;

                std::unique_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);

                stmt = pImpl->stmtCache.get(pImpl->db, query);
                bindParameters(stmt, 1, std::forward<Args>(params)...);

                int rc = sqlite3_step(stmt);

                if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
                    String error =
                        String("Failed to execute parameterized query: ") +
                        String(sqlite3_errmsg(pImpl->db));
                    sqlite3_reset(stmt);
                    throw SQLiteException(error, rc);
                }

                sqlite3_reset(stmt);
                return true;
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
        }

        return true;
    } catch (const SQLiteException& e) {
        if (pImpl)
            pImpl->errorCallback(e.what());
        throw;
    } catch (const std::exception& e) {
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
    try {
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                sqlite3_stmt* stmt = nullptr;

                std::shared_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);

                stmt = pImpl->stmtCache.get(pImpl->db, query);

                ResultSet results;
                int columnCount = sqlite3_column_count(stmt);
                int rc;

                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                    RowData row;
                    row.reserve(columnCount);

                    for (int i = 0; i < columnCount; ++i) {
                        const unsigned char* value_uchar =
                            sqlite3_column_text(stmt, i);
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
                    throw SQLiteException(error, rc);
                }

                sqlite3_reset(stmt);
                return results;
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
        }

        return std::move(*result.value);
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
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
    try {
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                sqlite3_stmt* stmt = nullptr;

                std::shared_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);

                stmt = pImpl->stmtCache.get(pImpl->db, query);
                bindParameters(stmt, 1, std::forward<Args>(params)...);

                ResultSet results;
                int columnCount = sqlite3_column_count(stmt);
                int rc;

                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                    RowData row;
                    row.reserve(columnCount);

                    for (int i = 0; i < columnCount; ++i) {
                        const unsigned char* value_uchar =
                            sqlite3_column_text(stmt, i);
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
                    String error =
                        String("Error fetching parameterized data: ") +
                        String(sqlite3_errmsg(pImpl->db));
                    sqlite3_reset(stmt);
                    throw SQLiteException(error, rc);
                }

                sqlite3_reset(stmt);
                return results;
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
        }

        return std::move(*result.value);
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
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
    try {
        RetryExecutor executor(RetryPolicy{});
        auto retryResult = executor.execute(
            [&]() -> std::optional<T> {
                sqlite3_stmt* stmt = nullptr;

                std::shared_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);

                stmt = pImpl->stmtCache.get(pImpl->db, query);

                std::optional<T> result;
                int rc = sqlite3_step(stmt);

                if (rc == SQLITE_ROW) {
                    if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                        result = columnFunc(stmt, 0);
                    }
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                    }
                } else if (rc != SQLITE_DONE) {
                    String error = String("Error getting single value: ") +
                                   String(sqlite3_errmsg(pImpl->db));
                    sqlite3_reset(stmt);
                    throw SQLiteException(error, rc);
                }

                sqlite3_reset(stmt);
                return result;
            },
            shouldRetrySqlite);

        if (!retryResult.success) {
            if (pImpl) {
                pImpl->errorCallback(retryResult.lastError);
            }
            return std::nullopt;
        }

        return std::move(*retryResult.value);
    } catch (const std::exception& e) {
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
    try {
        RetryExecutor executor(RetryPolicy{});
        auto retryResult = executor.execute(
            [&]() {
                sqlite3_stmt* stmt = nullptr;
                bool found = false;

                std::shared_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);

                stmt = pImpl->stmtCache.get(pImpl->db, query);

                int rc_bind = sqlite3_bind_text(
                    stmt, 1, searchTerm.data(),
                    static_cast<int>(searchTerm.size()), SQLITE_TRANSIENT);
                if (rc_bind != SQLITE_OK) {
                    String error = String("Failed to bind search parameter: ") +
                                   String(sqlite3_errmsg(pImpl->db));
                    sqlite3_reset(stmt);
                    throw SQLiteException(error, rc_bind);
                }

                int rc_step;
                while ((rc_step = sqlite3_step(stmt)) == SQLITE_ROW) {
                    found = true;
                }

                if (rc_step != SQLITE_DONE) {
                    String error = String("Error during search execution: ") +
                                   String(sqlite3_errmsg(pImpl->db));
                    sqlite3_reset(stmt);
                    throw SQLiteException(error, rc_step);
                }

                sqlite3_reset(stmt);
                return found;
            },
            shouldRetrySqlite);

        if (!retryResult.success) {
            if (pImpl) {
                pImpl->errorCallback(retryResult.lastError);
            }
            return false;
        }

        return std::move(*retryResult.value);
    } catch (const std::exception& e) {
        String error = "Error during search: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        return false;
    }
}

int SqliteDB::executeAndGetChanges(std::string_view query) {
    try {
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                std::unique_lock<std::shared_mutex> lock(mtx);
                checkConnection();
                validateQueryString(query);

                pImpl->executeSimpleOrThrow(query);
                return sqlite3_changes(pImpl->db);
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
        }

        return std::move(*result.value);
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
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                std::unique_lock<std::shared_mutex> lock(mtx);
                checkConnection();

                if (pImpl->inTransaction.load()) {
                    throw SQLiteException("Transaction already in progress",
                                          SQLITE_MISUSE);
                }

                pImpl->executeSimpleOrThrow("BEGIN IMMEDIATE TRANSACTION");
                pImpl->inTransaction.store(true);
                spdlog::debug("Transaction started");
                return true;
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
        }
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
        RetryExecutor executor(RetryPolicy{});
        auto result = executor.execute(
            [&]() {
                std::unique_lock<std::shared_mutex> lock(mtx);
                checkConnection();

                if (!pImpl->inTransaction.load()) {
                    throw SQLiteException(
                        "No transaction in progress to commit", SQLITE_MISUSE);
                }

                try {
                    pImpl->executeSimpleOrThrow("COMMIT TRANSACTION");
                    pImpl->inTransaction.store(false);
                    spdlog::debug("Transaction committed");
                    return true;
                } catch (const SQLiteException& e) {
                    if (!shouldRetrySqlite(e)) {
                        spdlog::error("Commit failed, attempting rollback...");
                        ATOM_UNUSED_RESULT(
                            pImpl->executeSimple("ROLLBACK TRANSACTION"));
                        pImpl->inTransaction.store(false);
                    }
                    throw;
                }
            },
            shouldRetrySqlite);

        if (!result.success) {
            if (pImpl) {
                pImpl->errorCallback(result.lastError);
            }
            throw SQLiteException(result.lastError);
        }
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

bool SqliteDB::tableExists(std::string_view /*tableName*/) {
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

std::vector<String> SqliteDB::getTables() {
    std::vector<String> tables;
    try {
        auto result = selectData(
            "SELECT name FROM sqlite_master WHERE type='table' "
            "AND name NOT LIKE 'sqlite_%' ORDER BY name");
        tables.reserve(result.size());
        for (const auto& row : result) {
            if (!row.empty()) {
                tables.push_back(row[0]);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error getting tables: {}", e.what());
    }
    return tables;
}

std::vector<String> SqliteDB::getColumns(std::string_view tableName) {
    std::vector<String> columns;
    try {
        String query = "PRAGMA table_info(";
        query += String(tableName);
        query += ")";
        auto result = selectData(query);
        columns.reserve(result.size());
        for (const auto& row : result) {
            if (row.size() > 1) {
                columns.push_back(row[1]);  // Column name is at index 1
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error getting columns for table {}: {}", tableName,
                      e.what());
    }
    return columns;
}

bool SqliteDB::executeBatch(const std::vector<std::string>& queries) {
    for (const auto& query : queries) {
        try {
            if (!executeQuery(query)) {
                spdlog::error("Batch execution failed at query: {}", query);
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("Batch execution error: {}", e.what());
            return false;
        }
    }
    spdlog::debug("Batch execution completed successfully, {} queries",
                  queries.size());
    return true;
}

bool SqliteDB::executeBatchTransaction(
    const std::vector<std::string>& queries) {
    try {
        beginTransaction();
        for (const auto& query : queries) {
            if (!executeQuery(query)) {
                spdlog::error(
                    "Batch transaction failed, rolling back at query: {}",
                    query);
                rollbackTransaction();
                return false;
            }
        }
        commitTransaction();
        spdlog::debug("Batch transaction completed successfully, {} queries",
                      queries.size());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Batch transaction error: {}", e.what());
        try {
            rollbackTransaction();
        } catch (...) {
            spdlog::critical(
                "Failed to rollback after batch transaction error");
        }
        return false;
    }
}

std::string SqliteDB::getVersion() { return sqlite3_libversion(); }

bool SqliteDB::integrityCheck() {
    try {
        auto result = selectData("PRAGMA integrity_check");
        if (!result.empty() && !result[0].empty()) {
            const auto& status = result[0][0];
            if (status == "ok") {
                spdlog::debug("Database integrity check passed");
                return true;
            }
            spdlog::error("Database integrity check failed: {}",
                          status.c_str());
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error during integrity check: {}", e.what());
        return false;
    }
}

bool SqliteDB::backup(std::string_view destPath) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        sqlite3* destDb = nullptr;
        String destPathStr(destPath);
        int rc = sqlite3_open(destPathStr.c_str(), &destDb);
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to open backup destination: {}",
                          sqlite3_errmsg(destDb));
            sqlite3_close(destDb);
            return false;
        }

        sqlite3_backup* backup =
            sqlite3_backup_init(destDb, "main", pImpl->db, "main");
        if (!backup) {
            spdlog::error("Failed to initialize backup: {}",
                          sqlite3_errmsg(destDb));
            sqlite3_close(destDb);
            return false;
        }

        rc = sqlite3_backup_step(backup, -1);  // Copy all pages
        sqlite3_backup_finish(backup);
        sqlite3_close(destDb);

        if (rc != SQLITE_DONE) {
            spdlog::error("Backup failed with code: {}", rc);
            return false;
        }

        spdlog::info("Database backed up to: {}", destPathStr.c_str());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error during backup: {}", e.what());
        return false;
    }
}

template std::optional<int> SqliteDB::getSingleValue<int>(
    std::string_view query, int (*columnFunc)(sqlite3_stmt*, int));
template std::optional<double> SqliteDB::getSingleValue<double>(
    std::string_view query, double (*columnFunc)(sqlite3_stmt*, int));

// Additional explicit template instantiations for test cases
template bool SqliteDB::executeParameterizedQuery<const char*, int>(
    std::string_view, const char*&&, int&&);
template bool SqliteDB::executeParameterizedQuery<const char*&, const char*&,
                                                  int>(std::string_view,
                                                       const char*&,
                                                       const char*&, int&&);
template bool SqliteDB::executeParameterizedQuery<int, const char*&>(
    std::string_view, int&&, const char*&);
template bool SqliteDB::executeParameterizedQuery<std::string, std::string,
                                                  int>(std::string_view,
                                                       std::string&&,
                                                       std::string&&, int&&);

// Final template instantiations for remaining string literal combinations
template bool
SqliteDB::executeParameterizedQuery<std::string&, char const (&)[15], int>(
    std::string_view, std::string&, char const (&)[15], int&&);
template bool
SqliteDB::executeParameterizedQuery<std::string&, char const (&)[17], int>(
    std::string_view, std::string&, char const (&)[17], int&&);

// selectParameterizedData instantiations (these are new)
template SqliteDB::ResultSet SqliteDB::selectParameterizedData<int>(
    std::string_view, int&&);
template SqliteDB::ResultSet SqliteDB::selectParameterizedData<std::string>(
    std::string_view, std::string&&);
template SqliteDB::ResultSet SqliteDB::selectParameterizedData<const char*>(
    std::string_view, const char*&&);
template SqliteDB::ResultSet
SqliteDB::selectParameterizedData<const char*, int>(std::string_view,
                                                    const char*&&, int&&);

}  // namespace atom::search
