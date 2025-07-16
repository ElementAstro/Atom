/**
 * @file mysql.cpp
 * @brief Implementation of the high-performance, thread-safe MySQL/MariaDB client.
 * @date 2025-07-16
 */

#include "mysql.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace atom::database {

// Helper to bind parameters to a prepared statement
namespace {
void bind_parameter(MYSQL_STMT* /*stmt*/, unsigned int /*index*/,
                    MYSQL_BIND* /*bind*/) {}

template <typename T, typename... Args>
void bind_parameter(MYSQL_STMT* stmt, unsigned int index, MYSQL_BIND* binds,
                    T&& value, Args&&... args) {
    auto& bind = binds[index];
    memset(&bind, 0, sizeof(MYSQL_BIND));

    using DecayedT = std::decay_t<T>;

    if constexpr (std::is_same_v<DecayedT, int>) {
        bind.buffer_type = MYSQL_TYPE_LONG;
        bind.buffer = const_cast<int*>(&value);
    } else if constexpr (std::is_same_v<DecayedT, int64_t>) {
        bind.buffer_type = MYSQL_TYPE_LONGLONG;
        bind.buffer = const_cast<int64_t*>(&value);
    } else if constexpr (std::is_same_v<DecayedT, double>) {
        bind.buffer_type = MYSQL_TYPE_DOUBLE;
        bind.buffer = const_cast<double*>(&value);
    } else if constexpr (std::is_same_v<DecayedT, std::string> ||
                         std::is_same_v<DecayedT, std::string_view>) {
        bind.buffer_type = MYSQL_TYPE_STRING;
        bind.buffer = const_cast<char*>(value.data());
        bind.buffer_length = value.size();
    } else {
        // This should not happen with a well-defined interface
        throw MySQLException("Unsupported parameter type");
    }

    bind_parameter(stmt, index + 1, binds, std::forward<Args>(args)...);
}
}  // namespace

/**
 * @class ConnectionPool
 * @brief Manages a pool of MySQL connections.
 */
class ConnectionPool {
public:
    ConnectionPool(const ConnectionParams& params, unsigned int pool_size)
        : params_(params) {
        for (unsigned int i = 0; i < pool_size; ++i) {
            pool_.push_back(create_connection());
        }
    }

    ~ConnectionPool() {
        for (MYSQL* conn : pool_) {
            mysql_close(conn);
        }
    }

    std::unique_ptr<MYSQL, std::function<void(MYSQL*)>> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !pool_.empty(); });
        MYSQL* conn = pool_.front();
        pool_.pop_front();
        return {conn, [this](MYSQL* c) { release(c); }};
    }

    void release(MYSQL* conn) {
        std::unique_lock<std::mutex> lock(mutex_);
        pool_.push_back(conn);
        lock.unlock();
        cv_.notify_one();
    }

private:
    MYSQL* create_connection() {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            throw MySQLException("mysql_init failed");
        }

        if (params_.connect_timeout > 0) {
            mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT,
                          &params_.connect_timeout);
        }

        my_bool reconnect = params_.auto_reconnect;
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

        if (!mysql_real_connect(conn, params_.host.c_str(), params_.user.c_str(),
                                params_.password.c_str(),
                                params_.database.c_str(), params_.port,
                                params_.socket.c_str(), params_.client_flag)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw MySQLException(error);
        }
        return conn;
    }

    ConnectionParams params_;
    std::deque<MYSQL*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class MysqlDB::Impl {
public:
    Impl(const ConnectionParams& params, unsigned int pool_size)
        : pool_(params, pool_size > 0 ? pool_size
                                      : std::thread::hardware_concurrency()) {}

    ConnectionPool pool_;
};

MysqlDB::MysqlDB(const ConnectionParams& params, unsigned int pool_size)
    : p_impl_(std::make_unique<Impl>(params, pool_size)) {}

MysqlDB::~MysqlDB() = default;

uint64_t MysqlDB::execute(std::string_view query) {
    auto conn = p_impl_->pool_.acquire();
    if (mysql_real_query(conn.get(), query.data(), query.length()) != 0) {
        throw MySQLException(mysql_error(conn.get()));
    }
    return mysql_affected_rows(conn.get());
}

template <typename... Args>
uint64_t MysqlDB::execute(std::string_view query, Args&&... params) {
    auto conn = p_impl_->pool_.acquire();
    MYSQL_STMT* stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        throw MySQLException("mysql_stmt_init failed");
    }
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt_ptr(
        stmt, &mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.data(), query.length()) != 0) {
        throw MySQLException(mysql_stmt_error(stmt));
    }

    std::vector<MYSQL_BIND> binds(sizeof...(params));
    bind_parameter(stmt, 0, binds.data(), std::forward<Args>(params)...);

    if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
        throw MySQLException(mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt) != 0) {
        throw MySQLException(mysql_stmt_error(stmt));
    }

    return mysql_stmt_affected_rows(stmt);
}

std::unique_ptr<ResultSet> MysqlDB::query(std::string_view query) {
    auto conn = p_impl_->pool_.acquire();
    if (mysql_real_query(conn.get(), query.data(), query.length()) != 0) {
        throw MySQLException(mysql_error(conn.get()));
    }

    MYSQL_RES* result = mysql_store_result(conn.get());
    if (!result) {
        if (mysql_field_count(conn.get()) == 0) {
            return nullptr;  // No result set
        }
        throw MySQLException(mysql_error(conn.get()));
    }
    return std::make_unique<ResultSet>(result);
}

template <typename... Args>
std::unique_ptr<ResultSet> MysqlDB::query(std::string_view query,
                                          Args&&... params) {
    auto conn = p_impl_->pool_.acquire();
    MYSQL_STMT* stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        throw MySQLException("mysql_stmt_init failed");
    }
    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt_ptr(
        stmt, &mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.data(), query.length()) != 0) {
        throw MySQLException(mysql_stmt_error(stmt));
    }

    std::vector<MYSQL_BIND> binds(sizeof...(params));
    bind_parameter(stmt, 0, binds.data(), std::forward<Args>(params)...);

    if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
        throw MySQLException(mysql_stmt_error(stmt));
    }

    if (mysql_stmt_execute(stmt) != 0) {
        throw MySQLException(mysql_stmt_error(stmt));
    }

    MYSQL_RES* result = mysql_stmt_result_metadata(stmt);
    if (!result) {
        throw MySQLException("mysql_stmt_result_metadata failed");
    }

    if (mysql_stmt_store_result(stmt) != 0) {
        mysql_free_result(result);
        throw MySQLException(mysql_stmt_error(stmt));
    }

    return std::make_unique<ResultSet>(result);
}

std::unique_ptr<Transaction> MysqlDB::begin_transaction() {
    return std::make_unique<Transaction>(p_impl_->pool_.acquire());
}

void MysqlDB::with_transaction(const std::function<void(MYSQL&)>& func) {
    auto conn = p_impl_->pool_.acquire();
    if (mysql_query(conn.get(), "START TRANSACTION") != 0) {
        throw MySQLException(mysql_error(conn.get()));
    }
    try {
        func(*conn.get());
        if (mysql_query(conn.get(), "COMMIT") != 0) {
            throw MySQLException(mysql_error(conn.get()));
        }
    } catch (...) {
        mysql_query(conn.get(), "ROLLBACK");
        throw;
    }
}

std::string MysqlDB::escape(std::string_view str) {
    auto conn = p_impl_->pool_.acquire();
    std::string escaped(str.length() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(
        conn.get(), &escaped[0], str.data(), str.length());
    escaped.resize(len);
    return escaped;
}

bool MysqlDB::ping() {
    auto conn = p_impl_->pool_.acquire();
    return mysql_ping(conn.get()) == 0;
}

// ResultSet Implementation
ResultSet::ResultSet(MYSQL_RES* result)
    : result_(result, &mysql_free_result),
      field_count_(mysql_num_fields(result)) {}

ResultSet::~ResultSet() = default;

ResultSet::ResultSet(ResultSet&&) noexcept = default;
ResultSet& ResultSet::operator=(ResultSet&&) noexcept = default;

bool ResultSet::next() {
    current_row_ = mysql_fetch_row(result_.get());
    if (current_row_) {
        current_lengths_ = mysql_fetch_lengths(result_.get());
    }
    return current_row_ != nullptr;
}

std::string_view ResultSet::get_string(unsigned int index) const {
    if (!current_row_ || index >= field_count_ || !current_row_[index]) {
        return "";
    }
    return {current_row_[index], current_lengths_[index]};
}

int ResultSet::get_int(unsigned int index) const {
    auto sv = get_string(index);
    if (sv.empty()) return 0;
    return std::stoi(std::string(sv));
}

int64_t ResultSet::get_int64(unsigned int index) const {
    auto sv = get_string(index);
    if (sv.empty()) return 0;
    return std::stoll(std::string(sv));
}

double ResultSet::get_double(unsigned int index) const {
    auto sv = get_string(index);
    if (sv.empty()) return 0.0;
    return std::stod(std::string(sv));
}

bool ResultSet::is_null(unsigned int index) const {
    return !current_row_ || index >= field_count_ || !current_row_[index];
}

unsigned int ResultSet::get_field_count() const { return field_count_; }

uint64_t ResultSet::get_row_count() const {
    return mysql_num_rows(result_.get());
}

// Transaction Implementation
Transaction::Transaction(
    std::unique_ptr<MYSQL, std::function<void(MYSQL*)>> conn)
    : conn_(std::move(conn)) {
    if (mysql_query(conn_.get(), "START TRANSACTION") != 0) {
        throw MySQLException(mysql_error(conn_.get()));
    }
}

Transaction::~Transaction() {
    if (conn_ && !committed_or_rolled_back_) {
        try {
            rollback();
        } catch (const MySQLException& e) {
            spdlog::error("Failed to rollback transaction in destructor: {}",
                          e.what());
        }
    }
}

Transaction::Transaction(Transaction&& other) noexcept
    : conn_(std::move(other.conn_)),
      committed_or_rolled_back_(other.committed_or_rolled_back_) {
    other.committed_or_rolled_back_ = true;
}

Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        conn_ = std::move(other.conn_);
        committed_or_rolled_back_ = other.committed_or_rolled_back_;
        other.committed_or_rolled_back_ = true;
    }
    return *this;
}

void Transaction::commit() {
    if (!conn_ || committed_or_rolled_back_) {
        return;
    }
    if (mysql_query(conn_.get(), "COMMIT") != 0) {
        throw MySQLException(mysql_error(conn_.get()));
    }
    committed_or_rolled_back_ = true;
}

void Transaction::rollback() {
    if (!conn_ || committed_or_rolled_back_) {
        return;
    }
    if (mysql_query(conn_.get(), "ROLLBACK") != 0) {
        throw MySQLException(mysql_error(conn_.get()));
    }
    committed_or_rolled_back_ = true;
}

MYSQL* Transaction::get_connection() { return conn_.get(); }

// Explicit template instantiations
template uint64_t MysqlDB::execute<int>(std::string_view, int&&);
template uint64_t MysqlDB::execute<double>(std::string_view, double&&);
template uint64_t MysqlDB::execute<std::string_view>(std::string_view,
                                                      std::string_view&&);

template std::unique_ptr<ResultSet> MysqlDB::query<int>(std::string_view, int&&);

}  // namespace database
}  // namespace atom