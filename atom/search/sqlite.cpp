/*
 * sqlite.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "sqlite.hpp"

#include <sqlite3.h>
#include <atomic>
#include <chrono>
#include <cstring>  // For std::strlen
#include <mutex>
#include <stdexcept>    // For std::runtime_error in bindParameters
#include <type_traits>  // For std::is_same_v, std::decay_t
#include <utility>      // For std::move

#include "atom/containers/high_performance.hpp"  // Include high performance containers
#include "atom/log/loguru.hpp"
#include "atom/macro.hpp"

// Use type aliases from high_performance.hpp
using atom::containers::HashMap;  // Use HashMap for the cache
using atom::containers::String;
using atom::containers::Vector;

// SIMD optimization headers (kept as is)
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
        String queryStr(query);  // Use String for key
        auto it = cache.find(queryStr);

        if (it != cache.end()) {
            it->second.lastUsed = std::chrono::steady_clock::now();
            sqlite3_reset(it->second.stmt);           // Reset before returning
            sqlite3_clear_bindings(it->second.stmt);  // Clear bindings
            return it->second.stmt;
        }

        sqlite3_stmt* stmt = nullptr;
        // Use queryStr.c_str() if String doesn't implicitly convert to const
        // char*
        int rc = sqlite3_prepare_v2(db, queryStr.c_str(),
                                    static_cast<int>(queryStr.size()), &stmt,
                                    nullptr);

        if (rc != SQLITE_OK) {
            // Log error or handle appropriately
            LOG_F(ERROR, "Failed to prepare statement: %s, Query: %s",
                  sqlite3_errmsg(db), queryStr.c_str());
            return nullptr;
        }

        // Manage cache size
        if (cache.size() >= maxCacheSize) {
            evictOldest();
        }

        CachedStatement cached;
        cached.stmt = stmt;
        cached.lastUsed = std::chrono::steady_clock::now();
        cache[queryStr] = cached;  // Insert using String key

        return stmt;
    }

    void remove(std::string_view query) {
        String queryStr(query);  // Use String for key
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

    // Use HashMap<String, CachedStatement>
    HashMap<String, CachedStatement> cache;
    size_t maxCacheSize;
};

// Helper function for binding parameters recursively
inline void bindParameters(sqlite3_stmt* /*stmt*/, int /*index*/) {
    // Base case: no more parameters
}

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
        rc = sqlite3_bind_text(
            stmt, index, value, -1,
            SQLITE_STATIC);  // Assume static lifetime or copy needed
    } else if constexpr (std::is_same_v<DecayedT, String> ||
                         std::is_same_v<DecayedT, std::string>) {
        // Assuming String has c_str() and size()
        rc = sqlite3_bind_text(stmt, index, value.c_str(),
                               static_cast<int>(value.size()),
                               SQLITE_TRANSIENT);  // Copy data
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        rc = sqlite3_bind_text(stmt, index, value.data(),
                               static_cast<int>(value.size()),
                               SQLITE_TRANSIENT);  // Copy data
    } else if constexpr (std::is_null_pointer_v<DecayedT>) {
        rc = sqlite3_bind_null(stmt, index);
    }
    // Add more types as needed (e.g., BLOB)
    else {
        // This will cause a compile-time error if an unsupported type is used.
        // static_assert(false, "Unsupported parameter type for SQLite
        // binding"); Or throw a runtime error if preferred, though compile-time
        // is better.
        throw std::runtime_error(
            "Unsupported parameter type for SQLite binding");
    }

    if (rc != SQLITE_OK) {
        throw SQLiteException(String("Failed to bind parameter at index ") +
                              String(std::to_string(index)) + ": " +
                              sqlite3_errmsg(sqlite3_db_handle(stmt)));
    }

    // Recursively bind the rest
    bindParameters(stmt, index + 1, std::forward<Args>(args)...);
}

// Implementation details
class SqliteDB::Impl {
public:
    sqlite3* db{nullptr};
    std::function<void(std::string_view)> errorCallback;
    std::atomic<bool> inTransaction{false};
    StatementCache stmtCache;

    Impl()
        : errorCallback([](std::string_view msg) {
              LOG_F(ERROR, "SQLite Error: %.*s", static_cast<int>(msg.size()),
                    msg.data());
          }) {}

    ~Impl() {
        try {
            // Statement cache cleared by its own destructor
            if (db != nullptr) {
                int rc = sqlite3_close_v2(
                    db);  // Using sqlite3_close_v2 for safer cleanup
                if (rc != SQLITE_OK) {
                    // Log error if closing fails (e.g., unfinalized statements)
                    LOG_F(ERROR, "Failed to close database cleanly: %s",
                          sqlite3_errmsg(db));
                    // Force close if v2 fails? Depends on desired behavior.
                    // sqlite3_close(db);
                } else {
                    DLOG_F(INFO, "Database closed");
                }
                db = nullptr;  // Ensure db is null even if close failed
            }
        } catch (...) {
            // Ensure no exceptions escape destructor
            LOG_F(ERROR, "Unknown exception during database cleanup");
        }
    }

    bool open(std::string_view dbPath) {
        if (dbPath.empty()) {
            errorCallback("Database path cannot be empty");
            return false;
        }
        // Ensure null termination for sqlite3_open_v2 if dbPath might not be
        // null-terminated
        String dbPathStr(dbPath);

        try {
            // Use SQLITE_OPEN_URI for more flexibility if needed in the future
            int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_FULLMUTEX;
            int rc = sqlite3_open_v2(dbPathStr.c_str(), &db, flags, nullptr);

            if (rc != SQLITE_OK) {
                String error =
                    sqlite3_errmsg(db);  // Capture error before closing
                errorCallback(error);
                sqlite3_close(
                    db);  // Close handle even if open failed partially
                db = nullptr;
                return false;
            }

            // Configure SQLite for better performance and safety
            executeSimple("PRAGMA journal_mode = WAL");  // Write-Ahead Logging
            executeSimple("PRAGMA synchronous = NORMAL");  // Less strict than
                                                           // FULL, faster
            executeSimple(
                "PRAGMA cache_size = -10000");  // Suggest 10MB cache (negative
                                                // value is KiB)
            executeSimple(
                "PRAGMA foreign_keys = ON");  // Enforce foreign key constraints
            executeSimple(
                "PRAGMA busy_timeout = 5000");  // Wait 5 seconds if locked

            DLOG_F(INFO, "Opened database: %s", dbPathStr.c_str());
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

    // Executes simple SQL that doesn't return data and doesn't need parameters
    bool executeSimple(std::string_view query) {
        if (!db) {
            errorCallback("Database not connected for executeSimple");
            return false;
        }
        // Ensure null termination for sqlite3_exec
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

// Constructor and destructor
SqliteDB::SqliteDB(std::string_view dbPath) : pImpl(std::make_unique<Impl>()) {
    // Lock is acquired within pImpl->open if needed, avoid double locking
    if (!pImpl->open(dbPath)) {
        // Use String for exception message
        throw SQLiteException(String("Failed to open database: ") +
                              String(dbPath));
    }
}

SqliteDB::~SqliteDB() = default;

// Move operations - Implemented manually because std::shared_mutex is not
// movable
SqliteDB::SqliteDB(SqliteDB&& other) noexcept
    : pImpl(std::move(other.pImpl)),
      mtx()  // Default construct the mutex for the new object
{}

SqliteDB& SqliteDB::operator=(SqliteDB&& other) noexcept {
    if (this != &other) {
        // Lock both mutexes to prevent deadlock (using std::scoped_lock)
        // Note: This might be overly cautious depending on usage, but safer.
        // If move assignment is only called in specific non-contended
        // scenarios, locking might be simplified or removed. Consider if
        // `other` might be accessed concurrently during the move.
        std::scoped_lock lock(mtx, other.mtx);

        // Release resources of the current object
        pImpl = nullptr;  // unique_ptr handles deletion

        // Move resources from the other object
        pImpl = std::move(other.pImpl);

        // The current object's mutex `mtx` remains as it is (already locked by
        // scoped_lock). The `other` object's mutex `other.mtx` is left in a
        // valid, unlocked state. The `other` object is left in a valid, but
        // unspecified state (pImpl is null).
    }
    return *this;
}

// Helper methods
void SqliteDB::validateQueryString(std::string_view query) const {
    if (query.empty()) {
        throw SQLiteException("Query string cannot be empty");
    }

    // Basic SQL injection check - enhance as needed
    // Check for comments and multiple statements (simple version)
    // Note: This is NOT foolproof. Use prepared statements for safety.
    if (query.find("--") != std::string_view::npos) {
        // Allow comments only at the end? Or disallow completely?
        // For simplicity, let's disallow for now in non-prepared statements.
        // throw SQLiteException("SQL comments (--) are not allowed in this
        // context");
        DLOG_F(WARNING, "Query contains '--': %.*s",
               static_cast<int>(query.size()), query.data());
    }
    // Check for semicolons not at the very end
    size_t firstSemicolon = query.find(';');
    if (firstSemicolon != std::string_view::npos &&
        firstSemicolon < query.size() - 1) {
        throw SQLiteException(
            "Multiple SQL statements (;) are not allowed in a single query");
    }
}

void SqliteDB::checkConnection() const {
    if (!pImpl || !pImpl->db) {  // Check pImpl as well
        throw SQLiteException("Database is not connected");
    }
}

// Core operations
bool SqliteDB::executeQuery(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();
        validateQueryString(query);  // Validate before executing

        // Use executeSimple for non-parameterized queries without results
        if (!pImpl->executeSimple(query)) {
            // executeSimple already calls errorCallback
            throw SQLiteException(pImpl->getLastError());
        }

        return true;
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        String error = "Error executing query: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);  // Check pImpl exists
        throw SQLiteException(error);
    }
}

// Explicit instantiation for the template function if needed, or keep in header
// template bool SqliteDB::executeParameterizedQuery<>(std::string_view query);
// // Example for no args

template <typename... Args>
bool SqliteDB::executeParameterizedQuery(std::string_view query,
                                         Args&&... params) {
    sqlite3_stmt* stmt = nullptr;  // Keep stmt pointer outside try block for
                                   // potential finalization
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();
        // No need to validate query string here, prepared statements handle it

        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            throw SQLiteException(String("Failed to prepare statement: ") +
                                  pImpl->getLastError());
        }

        // Bind parameters
        // bindParameters is exception-safe, throws SQLiteException on error
        bindParameters(stmt, 1, std::forward<Args>(params)...);

        int rc = sqlite3_step(stmt);

        // Check for success (DONE for non-SELECT, ROW for SELECT)
        // Allow ROW as well, as some execute queries might return a status row
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            String error = String("Failed to execute parameterized query: ") +
                           String(sqlite3_errmsg(pImpl->db));
            // Reset statement before throwing to release locks if any
            sqlite3_reset(stmt);
            throw SQLiteException(error);
        }

        // Reset statement after successful execution (or failure handled by
        // throw) This makes it ready for the next use or cache eviction.
        sqlite3_reset(stmt);

        return true;
    } catch (const SQLiteException& e) {
        // Don't reset here if already reset in the catch block above
        if (pImpl)
            pImpl->errorCallback(e.what());  // Log the specific bind/step error
        throw;                               // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);  // Attempt reset on general exceptions
        String error = "Error executing parameterized query: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
    // Note: Statement is not finalized here; it's managed by the
    // StatementCache.
}

// Explicitly instantiate common cases to avoid linker errors if definition is
// not in header
template bool SqliteDB::executeParameterizedQuery<>(std::string_view query);
template bool SqliteDB::executeParameterizedQuery<int>(std::string_view query,
                                                       int&&);
template bool SqliteDB::executeParameterizedQuery<double>(
    std::string_view query, double&&);
template bool SqliteDB::executeParameterizedQuery<const char*>(
    std::string_view query, const char*&&);
template bool SqliteDB::executeParameterizedQuery<String>(
    std::string_view query, String&&);
// Removed duplicate instantiation for std::string
// template bool SqliteDB::executeParameterizedQuery<std::string>(
//     std::string_view query, std::string&&);
template bool SqliteDB::executeParameterizedQuery<std::string_view>(
    std::string_view query, std::string_view&&);
// Add more instantiations as needed for common parameter combinations

SqliteDB::ResultSet SqliteDB::selectData(std::string_view query) {
    sqlite3_stmt* stmt = nullptr;
    try {
        std::shared_lock<std::shared_mutex> lock(mtx);
        checkConnection();
        // No need to validate query string here, prepared statements handle it

        // Use cache for SELECT statements too
        stmt = pImpl->stmtCache.get(pImpl->db, query);
        if (!stmt) {
            throw SQLiteException(String("Failed to prepare query: ") +
                                  pImpl->getLastError());
        }

        ResultSet results;
        int columnCount = sqlite3_column_count(stmt);
        int rc;

        // Fetch rows
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            RowData row;
            // Assuming Vector has reserve
            row.reserve(columnCount);

            for (int i = 0; i < columnCount; ++i) {
                const unsigned char* value_uchar = sqlite3_column_text(stmt, i);
                // Handle NULL values explicitly
                if (value_uchar) {
                    // Construct String directly from const char*
                    // Assuming String has a constructor for const char*
                    row.emplace_back(
                        reinterpret_cast<const char*>(value_uchar));
                } else {
                    // Add an empty String for NULL values
                    row.emplace_back(String());  // Assuming default constructor
                                                 // creates empty string
                }
            }
            // Assuming Vector has push_back with move semantics
            results.push_back(std::move(row));
        }

        // Check if loop terminated because of an error
        if (rc != SQLITE_DONE) {
            String error = String("Error fetching data: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);  // Reset before throwing
            throw SQLiteException(error);
        }

        // Reset statement after successful completion
        sqlite3_reset(stmt);

        return results;
    } catch (const SQLiteException&) {
        // Don't reset here if already reset in the catch block above
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        if (stmt)
            sqlite3_reset(stmt);  // Attempt reset on general exceptions
        String error = "Error selecting data: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);
    }
}

// Helper for single value retrieval
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
            // Consume any other potential rows (shouldn't be any for single
            // value query)
            while (sqlite3_step(stmt) == SQLITE_ROW)
                ;
        } else if (rc != SQLITE_DONE) {
            String error = String("Error getting single value: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            pImpl->errorCallback(error);  // Log error but return nullopt
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
        return std::nullopt;  // Return nullopt on general exceptions
    }
}

// NOTE: Calls below depend on getSingleValue being correctly defined/declared.
//       If getSingleValue is moved to the header, these should compile.
std::optional<int> SqliteDB::getIntValue(std::string_view query) {
    // Assuming getSingleValue is correctly declared in the header
    return getSingleValue<int>(query, sqlite3_column_int);
}

std::optional<double> SqliteDB::getDoubleValue(std::string_view query) {
    // Assuming getSingleValue is correctly declared in the header
    return getSingleValue<double>(query, sqlite3_column_double);
}

std::optional<String> SqliteDB::getTextValue(std::string_view query) {
    // Custom lambda because sqlite3_column_text returns const unsigned char*
    auto getTextFunc = [](sqlite3_stmt* stmt, int col) -> String {
        const unsigned char* text = sqlite3_column_text(stmt, col);
        return text ? String(reinterpret_cast<const char*>(text)) : String();
    };
    // Assuming getSingleValue is correctly declared in the header
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

        // Bind the search term to the first parameter (index 1)
        int rc_bind = sqlite3_bind_text(
            stmt, 1, searchTerm.data(), static_cast<int>(searchTerm.size()),
            SQLITE_TRANSIENT);  // Use TRANSIENT to copy data
        if (rc_bind != SQLITE_OK) {
            String error = String("Failed to bind search parameter: ") +
                           String(sqlite3_errmsg(pImpl->db));
            sqlite3_reset(stmt);
            pImpl->errorCallback(error);
            return false;
        }

        // Execute the search
        int rc_step = sqlite3_step(stmt);

        // Reset statement regardless of outcome
        sqlite3_reset(stmt);

        // SQLITE_ROW means we found at least one matching row
        if (rc_step == SQLITE_ROW) {
            // Consume any remaining rows to allow reset
            while (rc_step == SQLITE_ROW) {
                rc_step = sqlite3_step(stmt);
            }
            sqlite3_reset(stmt);  // Reset again after consuming all rows
            return true;
        } else if (rc_step == SQLITE_DONE) {
            return false;  // No rows found
        } else {
            // Error occurred during step
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

// Helper for update/delete
// NOTE: Ensure the declaration of this function in sqlite.hpp matches
//       this definition (int executeAndGetChanges(std::string_view query);).
int SqliteDB::executeAndGetChanges(std::string_view query) {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();
        validateQueryString(query);

        // Use executeSimple which handles errors and logging
        if (!pImpl->executeSimple(query)) {
            // Error already logged by executeSimple
            throw SQLiteException(
                pImpl->getLastError());  // Throw to indicate failure
        }

        // Return changes only if executeSimple succeeded
        return sqlite3_changes(pImpl->db);
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
    } catch (const std::exception& e) {
        String error = "Error executing update/delete: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        throw SQLiteException(error);  // Throw custom exception
    }
}

int SqliteDB::updateData(std::string_view query) {
    // NOTE: This call depends on executeAndGetChanges being correctly
    // declared/defined.
    return executeAndGetChanges(query);
}

int SqliteDB::deleteData(std::string_view query) {
    // NOTE: This call depends on executeAndGetChanges being correctly
    // declared/defined.
    return executeAndGetChanges(query);
}

// Transaction Management
void SqliteDB::beginTransaction() {
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        checkConnection();

        if (pImpl->inTransaction.load()) {  // Use atomic load
            throw SQLiteException("Transaction already in progress");
        }

        // Use executeSimple for BEGIN
        if (!pImpl->executeSimple(
                "BEGIN IMMEDIATE TRANSACTION")) {  // Use IMMEDIATE for better
                                                   // locking
            throw SQLiteException(String("Failed to begin transaction: ") +
                                  pImpl->getLastError());
        }

        pImpl->inTransaction.store(true);  // Use atomic store
        DLOG_F(INFO, "Transaction started");
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

        // Use executeSimple for COMMIT
        if (!pImpl->executeSimple("COMMIT TRANSACTION")) {
            // Attempt rollback if commit fails
            DLOG_F(ERROR, "Commit failed, attempting rollback...");
            ATOM_UNUSED_RESULT(pImpl->executeSimple("ROLLBACK TRANSACTION"));
            pImpl->inTransaction.store(false);  // Ensure state is reset
            throw SQLiteException(
                String("Failed to commit transaction (rolled back): ") +
                pImpl->getLastError());
        }

        pImpl->inTransaction.store(false);
        DLOG_F(INFO, "Transaction committed");
    } catch (const SQLiteException&) {
        throw;
    } catch (const std::exception& e) {
        String error = "Error committing transaction: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        // Should we attempt rollback here too? Depends on where the exception
        // occurred. If it was before COMMIT, rollback might be needed. If
        // after, maybe not. For safety, attempt rollback if a transaction might
        // still be active.
        if (pImpl && pImpl->inTransaction.load()) {
            DLOG_F(ERROR, "Exception during commit, attempting rollback...");
            ATOM_UNUSED_RESULT(pImpl->executeSimple("ROLLBACK TRANSACTION"));
            pImpl->inTransaction.store(false);
        }
        throw SQLiteException(error);
    }
}

void SqliteDB::rollbackTransaction() {
    // This function should generally not throw exceptions itself
    try {
        std::unique_lock<std::shared_mutex> lock(mtx);
        if (!pImpl || !pImpl->db) {
            LOG_F(ERROR, "Rollback attempted on disconnected database");
            return;  // Nothing to do if not connected
        }

        if (!pImpl->inTransaction.load()) {
            DLOG_F(WARNING, "No transaction in progress to rollback");
            return;  // Not an error, just nothing to do
        }

        DLOG_F(INFO, "Rolling back transaction...");
        // Use executeSimple for ROLLBACK, ignore return value as we can't do
        // much if it fails
        ATOM_UNUSED_RESULT(pImpl->executeSimple("ROLLBACK TRANSACTION"));
        pImpl->inTransaction.store(
            false);  // Reset state regardless of rollback success
    } catch (const std::exception& e) {
        // Log critical error if rollback itself causes an exception
        LOG_F(FATAL, "CRITICAL: Exception during transaction rollback: %s",
              e.what());
        // Ensure transaction state is reset even if logging fails
        if (pImpl)
            pImpl->inTransaction.store(false);
    } catch (...) {
        LOG_F(FATAL, "CRITICAL: Unknown exception during transaction rollback");
        if (pImpl)
            pImpl->inTransaction.store(false);
    }
}

void SqliteDB::withTransaction(const std::function<void()>& operations) {
    beginTransaction();  // Throws on failure
    try {
        operations();
        commitTransaction();  // Throws on failure
    } catch (...) {
        // Catch any exception from operations() or commitTransaction()
        try {
            rollbackTransaction();  // Attempt rollback (should not throw)
        } catch (...) {
            // Log if rollback itself throws, but prioritize original exception
            LOG_F(FATAL,
                  "CRITICAL: Exception during rollback within withTransaction");
        }
        throw;  // Re-throw the original exception
    }
}

bool SqliteDB::validateData(std::string_view query,
                            std::string_view validationQuery) {
    try {
        // Use a single lock for the whole operation if possible,
        // but getIntValue might acquire its own lock.
        // Let's rely on the locks within executeQuery and getIntValue.
        // std::unique_lock<std::shared_mutex> lock(mtx); // Avoid redundant
        // lock

        // Execute the main query (might modify data)
        if (!executeQuery(query)) {
            // Error already logged by executeQuery
            return false;
        }

        // Execute the validation query (should be SELECT)
        // NOTE: This call depends on getIntValue being correctly
        // declared/defined.
        auto validationResult = getIntValue(validationQuery);
        // Return true if validation query returned a non-zero integer
        return validationResult.value_or(0) != 0;
    } catch (const std::exception& e) {
        // Catch exceptions from executeQuery or getIntValue
        String error = "Error validating data: ";
        error += e.what();
        if (pImpl)
            pImpl->errorCallback(error);
        return false;  // Return false on any error during validation
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
        // Construct the paginated query using String
        String queryWithPagination(query);

        // 修复: 使用 String::npos 而不是 std::string::npos
        if (queryWithPagination.find(" LIMIT ") != String::npos ||
            queryWithPagination.find(" limit ") != String::npos) {
            throw SQLiteException("Query already contains a LIMIT clause");
        }

        queryWithPagination += " LIMIT ";
        queryWithPagination += String(std::to_string(limit));
        queryWithPagination += " OFFSET ";
        queryWithPagination += String(std::to_string(offset));

        // selectData handles locking and uses prepared statements via cache
        return selectData(queryWithPagination);
    } catch (const SQLiteException&) {
        throw;  // Re-throw SQLiteExceptions
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
    // Use shared lock for read-only access
    std::shared_lock<std::shared_mutex> lock(mtx);
    return pImpl && pImpl->db != nullptr;
}

int64_t SqliteDB::getLastInsertRowId() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    checkConnection();  // Throws if not connected
    return sqlite3_last_insert_rowid(pImpl->db);
}

int SqliteDB::getChanges() const {
    std::shared_lock<std::shared_mutex> lock(mtx);
    checkConnection();  // Throws if not connected
    // sqlite3_changes() returns the number of rows modified by the *most
    // recent* INSERT, UPDATE, or DELETE statement. It's reset by other
    // statements.
    return sqlite3_changes(pImpl->db);
}

// Explicit instantiation for getSingleValue<int> (if definition remains in
// .cpp)
template std::optional<int> SqliteDB::getSingleValue<int>(
    std::string_view query, int (*columnFunc)(sqlite3_stmt*, int));
// Explicit instantiation for getSingleValue<double> (if definition remains in
// .cpp)
template std::optional<double> SqliteDB::getSingleValue<double>(
    std::string_view query, double (*columnFunc)(sqlite3_stmt*, int));
// Explicit instantiation for getSingleValue<String> (if definition remains in
// .cpp) template std::optional<String>
// SqliteDB::getSingleValue<String>(std::string_view query, String
// (*columnFunc)(sqlite3_stmt*, int));