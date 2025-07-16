/**
 * @file sqlite.cpp
 * @brief Implementation of the high-performance, thread-safe SQLite database wrapper.
 * @date 2025-07-16
 */

#include "sqlite.hpp"

#include <sqlite3.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

#include <spdlog/spdlog.h>

namespace atom::search {

// Helper to bind parameters to a prepared statement
namespace {
void bind_parameters(sqlite3_stmt* /*stmt*/, int /*index*/) {}

template <typename T, typename... Args>
void bind_parameters(sqlite3_stmt* stmt, int index, T&& value, Args&&... args) {
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
        rc = sqlite3_bind_text(stmt, index, value.c_str(),
                               static_cast<int>(value.size()), SQLITE_TRANSIENT);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        rc = sqlite3_bind_text(stmt, index, value.data(),
                               static_cast<int>(value.size()), SQLITE_TRANSIENT);
    } else if constexpr (std::is_null_pointer_v<DecayedT>) {
        rc = sqlite3_bind_null(stmt, index);
    } else {
        throw SQLiteException("Unsupported parameter type for SQLite binding");
    }

    if (rc != SQLITE_OK) {
        throw SQLiteException(std::string("Failed to bind parameter at index ") +
                              std::to_string(index) + ": " +
                              sqlite3_errmsg(sqlite3_db_handle(stmt)));
    }

    bind_parameters(stmt, index + 1, std::forward<Args>(args)...);
}
}  // namespace

/**
 * @class ConnectionPool
 * @brief Manages a pool of SQLite database connections for concurrent access.
 */
class ConnectionPool {
public:
    /**
     * @brief Constructs a ConnectionPool.
     * @param db_path The path to the database file.
     * @param pool_size The number of connections to create.
     */
    ConnectionPool(std::string_view db_path, unsigned int pool_size)
        : db_path_(db_path) {
        for (unsigned int i = 0; i < pool_size; ++i) {
            sqlite3* db = nullptr;
            int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_FULLMUTEX;
            if (sqlite3_open_v2(db_path_.c_str(), &db, flags, nullptr) !=
                SQLITE_OK) {
                throw SQLiteException(std::string("Failed to open database: ") +
                                      sqlite3_errmsg(db));
            }
            // Enable WAL mode for better concurrency
            sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            pool_.push_back(db);
        }
    }

    /**
     * @brief Destructor, closes all connections.
     */
    ~ConnectionPool() {
        for (sqlite3* db : pool_) {
            sqlite3_close(db);
        }
    }

    /**
     * @brief Acquires a database connection from the pool.
     * @return A unique_ptr to a sqlite3 connection handle.
     */
    std::unique_ptr<sqlite3, std::function<void(sqlite3*)>> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !pool_.empty(); });
        sqlite3* db = pool_.front();
        pool_.pop_front();
        return {
            db, [this](sqlite3* db_to_release) { release(db_to_release); }};
    }

private:
    /**
     * @brief Releases a database connection back to the pool.
     * @param db The connection to release.
     */
    void release(sqlite3* db) {
        std::unique_lock<std::mutex> lock(mutex_);
        pool_.push_back(db);
        lock.unlock();
        cv_.notify_one();
    }

    std::string db_path_;
    std::deque<sqlite3*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class SqliteDB::Impl {
public:
    std::unique_ptr<ConnectionPool> pool;
    std::atomic<bool> is_connected{false};

    Impl(std::string_view db_path, unsigned int pool_size)
        : pool(std::make_unique<ConnectionPool>(db_path, pool_size)) {
        is_connected = true;
    }
};

SqliteDB::SqliteDB(std::string_view db_path, unsigned int pool_size)
    : p_impl_(std::make_unique<Impl>(
          db_path, pool_size > 0 ? pool_size
                                 : std::thread::hardware_concurrency())) {}

SqliteDB::~SqliteDB() = default;

void SqliteDB::execute_query(std::string_view query) {
    auto conn = p_impl_->pool->acquire();
    char* err_msg = nullptr;
    if (sqlite3_exec(conn.get(), query.data(), nullptr, nullptr, &err_msg) !=
        SQLITE_OK) {
        std::string error = err_msg;
        sqlite3_free(err_msg);
        throw SQLiteException(error);
    }
}

template <typename... Args>
void SqliteDB::execute_parameterized_query(std::string_view query,
                                           Args&&... params) {
    auto conn = p_impl_->pool->acquire();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), query.data(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        throw SQLiteException(sqlite3_errmsg(conn.get()));
    }
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt_ptr(
        stmt, &sqlite3_finalize);

    bind_parameters(stmt, 1, std::forward<Args>(params)...);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw SQLiteException(sqlite3_errmsg(conn.get()));
    }
}

SqliteDB::ResultSet SqliteDB::select_data(std::string_view query) {
    auto conn = p_impl_->pool->acquire();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), query.data(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        throw SQLiteException(sqlite3_errmsg(conn.get()));
    }
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt_ptr(
        stmt, &sqlite3_finalize);

    ResultSet results;
    int col_count = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RowData row;
        row.reserve(col_count);
        for (int i = 0; i < col_count; ++i) {
            const unsigned char* text = sqlite3_column_text(stmt, i);
            row.emplace_back(text ? reinterpret_cast<const char*>(text) : "");
        }
        results.push_back(std::move(row));
    }
    return results;
}

template <typename... Args>
SqliteDB::ResultSet SqliteDB::select_parameterized_data(std::string_view query,
                                                        Args&&... params) {
    auto conn = p_impl_->pool->acquire();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn.get(), query.data(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        throw SQLiteException(sqlite3_errmsg(conn.get()));
    }
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt_ptr(
        stmt, &sqlite3_finalize);

    bind_parameters(stmt, 1, std::forward<Args>(params)...);

    ResultSet results;
    int col_count = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RowData row;
        row.reserve(col_count);
        for (int i = 0; i < col_count; ++i) {
            const unsigned char* text = sqlite3_column_text(stmt, i);
            row.emplace_back(text ? reinterpret_cast<const char*>(text) : "");
        }
        results.push_back(std::move(row));
    }
    return results;
}

std::optional<int> SqliteDB::get_int_value(std::string_view query) {
    auto results = select_data(query);
    if (results.empty() || results[0].empty()) {
        return std::nullopt;
    }
    return std::stoi(std::string(results[0][0]));
}

std::optional<double> SqliteDB::get_double_value(std::string_view query) {
    auto results = select_data(query);
    if (results.empty() || results[0].empty()) {
        return std::nullopt;
    }
    return std::stod(std::string(results[0][0]));
}

std::optional<String> SqliteDB::get_text_value(std::string_view query) {
    auto results = select_data(query);
    if (results.empty() || results[0].empty()) {
        return std::nullopt;
    }
    return results[0][0];
}

void SqliteDB::with_transaction(
    const std::function<void(TransactionContext&)>& operations) {
    auto conn_ptr = p_impl_->pool->acquire();
    sqlite3* conn = conn_ptr.get();

    if (sqlite3_exec(conn, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) !=
        SQLITE_OK) {
        throw SQLiteException(std::string("Failed to begin transaction: ") +
                              sqlite3_errmsg(conn));
    }

    try {
        TransactionContext ctx(conn);
        operations(ctx);
        if (sqlite3_exec(conn, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw SQLiteException(std::string("Failed to commit transaction: ") +
                                  sqlite3_errmsg(conn));
        }
    } catch (...) {
        sqlite3_exec(conn, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;  // Re-throw the exception
    }
}

bool SqliteDB::is_connected() const noexcept {
    return p_impl_ && p_impl_->is_connected.load();
}

int64_t SqliteDB::get_last_insert_rowid() const {
    auto conn = p_impl_->pool->acquire();
    return sqlite3_last_insert_rowid(conn.get());
}

bool SqliteDB::table_exists(std::string_view table_name) {
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
    auto result = select_parameterized_data(query, table_name);
    return !result.empty();
}

bool SqliteDB::vacuum() { return execute_query("VACUUM;"), true; }

// Explicit template instantiations
template void SqliteDB::execute_parameterized_query<int>(std::string_view, int&&);
template void SqliteDB::execute_parameterized_query<double>(std::string_view,
                                                         double&&);
template void SqliteDB::execute_parameterized_query<const char*>(
    std::string_view, const char*&&);
template void SqliteDB::execute_parameterized_query<String>(std::string_view,
                                                         String&&);

template SqliteDB::ResultSet SqliteDB::select_parameterized_data<int>(
    std::string_view, int&&);

}  // namespace atom::search
