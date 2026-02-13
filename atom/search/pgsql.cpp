/**
 * @file pgsql.cpp
 * @brief Implementation of the high-performance, thread-safe PostgreSQL client.
 * @date 2025-07-16
 */

#include "pgsql.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace atom::database {

namespace {
// Helper to convert C++ types to string for libpq
template <typename T>
std::string to_string(T&& value) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                  std::is_same_v<std::decay_t<T>, std::string_view>) {
        return std::string(value);
    } else {
        return std::to_string(value);
    }
}
}  // namespace

/**
 * @class PgSqlConnectionPool
 * @brief Manages a pool of PostgreSQL connections.
 */
class PgSqlConnectionPool {
public:
    PgSqlConnectionPool(const PgSqlConnectionParams& params, unsigned int pool_size)
        : params_(params) {
        for (unsigned int i = 0; i < pool_size; ++i) {
            pool_.push_back(create_connection());
        }
    }

    ~PgSqlConnectionPool() {
        for (PGconn* conn : pool_) {
            PQfinish(conn);
        }
    }

    std::unique_ptr<PGconn, std::function<void(PGconn*)>> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !pool_.empty(); });
        PGconn* conn = pool_.front();
        pool_.pop_front();
        return {conn, [this](PGconn* c) { release(c); }};
    }

    void release(PGconn* conn) {
        std::unique_lock<std::mutex> lock(mutex_);
        pool_.push_back(conn);
        lock.unlock();
        cv_.notify_one();
    }

private:
    PGconn* create_connection() {
        std::string conn_info = "host=" + params_.host + " port=" +
                              std::to_string(params_.port) + " dbname=" +
                              params_.dbname + " user=" + params_.user +
                              " password=" + params_.password +
                              " connect_timeout=" +
                              std::to_string(params_.connect_timeout);

        PGconn* conn = PQconnectdb(conn_info.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::string error = PQerrorMessage(conn);
            PQfinish(conn);
            throw PgSqlException(error);
        }
        return conn;
    }

    PgSqlConnectionParams params_;
    std::deque<PGconn*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class PgSqlDB::Impl {
public:
    Impl(const PgSqlConnectionParams& params, unsigned int pool_size)
        : pool_(params, pool_size > 0 ? pool_size
                                      : std::thread::hardware_concurrency()) {}

    PgSqlConnectionPool pool_;
};

PgSqlDB::PgSqlDB(const PgSqlConnectionParams& params, unsigned int pool_size)
    : p_impl_(std::make_unique<Impl>(params, pool_size)) {}

PgSqlDB::~PgSqlDB() = default;

uint64_t PgSqlDB::execute(std::string_view query) {
    auto conn = p_impl_->pool_.acquire();
    PGresult* res = PQexec(conn.get(), query.data());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string error = PQresultErrorMessage(res);
        PQclear(res);
        throw PgSqlException(error);
    }
    uint64_t affected_rows = std::stoull(PQcmdTuples(res));
    PQclear(res);
    return affected_rows;
}

template <typename... Args>
uint64_t PgSqlDB::execute(std::string_view query, Args&&... args) {
    auto conn = p_impl_->pool_.acquire();
    std::vector<std::string> params_str = {to_string(std::forward<Args>(args))...};
    std::vector<const char*> param_values;
    param_values.reserve(params_str.size());
    for (const auto& p : params_str) {
        param_values.push_back(p.c_str());
    }

    PGresult* res = PQexecParams(conn.get(), query.data(), params_str.size(),
                                 nullptr, param_values.data(), nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string error = PQresultErrorMessage(res);
        PQclear(res);
        throw PgSqlException(error);
    }
    uint64_t affected_rows = std::stoull(PQcmdTuples(res));
    PQclear(res);
    return affected_rows;
}

std::unique_ptr<PgSqlResultSet> PgSqlDB::query(std::string_view query) {
    auto conn = p_impl_->pool_.acquire();
    PGresult* res = PQexec(conn.get(), query.data());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string error = PQresultErrorMessage(res);
        PQclear(res);
        throw PgSqlException(error);
    }
    return std::make_unique<PgSqlResultSet>(res);
}

template <typename... Args>
std::unique_ptr<PgSqlResultSet> PgSqlDB::query(std::string_view query,
                                               Args&&... args) {
    auto conn = p_impl_->pool_.acquire();
    std::vector<std::string> params_str = {to_string(std::forward<Args>(args))...};
    std::vector<const char*> param_values;
    param_values.reserve(params_str.size());
    for (const auto& p : params_str) {
        param_values.push_back(p.c_str());
    }

    PGresult* res = PQexecParams(conn.get(), query.data(), params_str.size(),
                                 nullptr, param_values.data(), nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string error = PQresultErrorMessage(res);
        PQclear(res);
        throw PgSqlException(error);
    }
    return std::make_unique<PgSqlResultSet>(res);
}

std::future<uint64_t> PgSqlDB::async_execute(std::string_view query) {
    return std::async(std::launch::async, [this, q = std::string(query)] {
        return execute(q);
    });
}

std::future<std::unique_ptr<PgSqlResultSet>> PgSqlDB::async_query(
    std::string_view query) {
    return std::async(std::launch::async, [this, q = std::string(query)] {
        return query(q);
    });
}

std::unique_ptr<PgSqlTransaction> PgSqlDB::begin_transaction() {
    return std::make_unique<PgSqlTransaction>(p_impl_->pool_.acquire());
}

void PgSqlDB::with_transaction(const std::function<void(PgSqlDB&)>& func) {
    auto tx = begin_transaction();
    try {
        func(*this);
        tx->commit();
    } catch (...) {
        tx->rollback();
        throw;
    }
}

std::unique_ptr<PgSqlPipeline> PgSqlDB::pipeline() {
    return std::make_unique<PgSqlPipeline>(p_impl_->pool_.acquire());
}

std::vector<std::unique_ptr<PgSqlResultSet>> PgSqlDB::with_pipeline(
    const std::function<void(PgSqlPipeline&)>& func) {
    auto p = pipeline();
    func(*p);
    return p->execute();
}

std::string PgSqlDB::escape_literal(std::string_view str) {
    auto conn = p_impl_->pool_.acquire();
    std::unique_ptr<char, decltype(&PQfreemem)> escaped(
        PQescapeLiteral(conn.get(), str.data(), str.length()), &PQfreemem);
    return escaped.get();
}

bool PgSqlDB::table_exists(std::string_view table_name) {
    auto result = query(
        "SELECT EXISTS (SELECT FROM pg_tables WHERE schemaname = 'public' AND "
        "tablename = $1)",
        std::string(table_name));
    return result && result->next() && result->get_string(0) == "t";
}

bool PgSqlDB::ping() {
    auto conn = p_impl_->pool_.acquire();
    return PQstatus(conn.get()) == CONNECTION_OK;
}

// PgSqlResultSet Implementation
PgSqlResultSet::PgSqlResultSet(PGresult* result)
    : result_(result, &PQclear),
      row_count_(PQntuples(result)),
      field_count_(PQnfields(result)) {}

PgSqlResultSet::~PgSqlResultSet() = default;

PgSqlResultSet::PgSqlResultSet(PgSqlResultSet&&) noexcept = default;
PgSqlResultSet& PgSqlResultSet::operator=(PgSqlResultSet&&) noexcept = default;

bool PgSqlResultSet::next() { return ++current_row_ < row_count_; }

std::string_view PgSqlResultSet::get_string(unsigned int col) const {
    if (is_null(col)) return "";
    return PQgetvalue(result_.get(), current_row_, col);
}

int PgSqlResultSet::get_int(unsigned int col) const {
    auto sv = get_string(col);
    if (sv.empty()) return 0;
    return std::stoi(std::string(sv));
}

int64_t PgSqlResultSet::get_int64(unsigned int col) const {
    auto sv = get_string(col);
    if (sv.empty()) return 0;
    return std::stoll(std::string(sv));
}

double PgSqlResultSet::get_double(unsigned int col) const {
    auto sv = get_string(col);
    if (sv.empty()) return 0.0;
    return std::stod(std::string(sv));
}

bool PgSqlResultSet::is_null(unsigned int col) const {
    return PQgetisnull(result_.get(), current_row_, col);
}

unsigned int PgSqlResultSet::get_field_count() const { return field_count_; }

uint64_t PgSqlResultSet::get_row_count() const { return row_count_; }

// PgSqlTransaction Implementation
PgSqlTransaction::PgSqlTransaction(
    std::unique_ptr<PGconn, std::function<void(PGconn*)>> conn)
    : conn_(std::move(conn)) {
    PGresult* res = PQexec(conn_.get(), "BEGIN");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        throw PgSqlException(PQerrorMessage(conn_.get()));
    }
    PQclear(res);
}

PgSqlTransaction::~PgSqlTransaction() {
    if (conn_ && !committed_or_rolled_back_) {
        try {
            rollback();
        } catch (const PgSqlException& e) {
            spdlog::error("Failed to rollback transaction in destructor: {}",
                          e.what());
        }
    }
}

PgSqlTransaction::PgSqlTransaction(PgSqlTransaction&& other) noexcept
    : conn_(std::move(other.conn_)),
      committed_or_rolled_back_(other.committed_or_rolled_back_) {
    other.committed_or_rolled_back_ = true;
}

PgSqlTransaction& PgSqlTransaction::operator=(PgSqlTransaction&& other) noexcept {
    if (this != &other) {
        conn_ = std::move(other.conn_);
        committed_or_rolled_back_ = other.committed_or_rolled_back_;
        other.committed_or_rolled_back_ = true;
    }
    return *this;
}

void PgSqlTransaction::commit() {
    if (!conn_ || committed_or_rolled_back_) {
        return;
    }
    PGresult* res = PQexec(conn_.get(), "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        throw PgSqlException(PQerrorMessage(conn_.get()));
    }
    PQclear(res);
    committed_or_rolled_back_ = true;
}

void PgSqlTransaction::rollback() {
    if (!conn_ || committed_or_rolled_back_) {
        return;
    }
    PGresult* res = PQexec(conn_.get(), "ROLLBACK");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        throw PgSqlException(PQerrorMessage(conn_.get()));
    }
    PQclear(res);
    committed_or_rolled_back_ = true;
}

PGconn* PgSqlTransaction::get_connection() { return conn_.get(); }

// PgSqlPipeline Implementation
PgSqlPipeline::PgSqlPipeline(
    std::unique_ptr<PGconn, std::function<void(PGconn*)>> conn)
    : conn_(std::move(conn)) {
    if (PQenterPipelineMode(conn_.get()) != 1) {
        throw PgSqlException("Failed to enter pipeline mode");
    }
}

PgSqlPipeline::~PgSqlPipeline() = default;

PgSqlPipeline::PgSqlPipeline(PgSqlPipeline&&) noexcept = default;
PgSqlPipeline& PgSqlPipeline::operator=(PgSqlPipeline&&) noexcept = default;

template <typename... Args>
void PgSqlPipeline::append(std::string_view query, Args&&... args) {
    std::vector<std::string> params_str = {to_string(std::forward<Args>(args))...};
    std::vector<const char*> param_values;
    param_values.reserve(params_str.size());
    for (const auto& p : params_str) {
        param_values.push_back(p.c_str());
    }

    if (PQsendQueryParams(conn_.get(), query.data(), params_str.size(), nullptr,
                          param_values.data(), nullptr, nullptr, 0) != 1) {
        throw PgSqlException(PQerrorMessage(conn_.get()));
    }
}

std::vector<std::unique_ptr<PgSqlResultSet>> PgSqlPipeline::execute() {
    if (PQpipelineSync(conn_.get()) != 1) {
        throw PgSqlException(PQerrorMessage(conn_.get()));
    }

    std::vector<std::unique_ptr<PgSqlResultSet>> results;
    PGresult* res;
    while ((res = PQgetResult(conn_.get())) != nullptr) {
        results.emplace_back(std::make_unique<PgSqlResultSet>(res));
    }

    if (PQexitPipelineMode(conn_.get()) != 1) {
        throw PgSqlException("Failed to exit pipeline mode");
    }

    return results;
}

// Explicit template instantiations
template uint64_t PgSqlDB::execute<int>(std::string_view, int&&);
template std::unique_ptr<PgSqlResultSet> PgSqlDB::query<std::string_view>(
    std::string_view, std::string_view&&);

template void PgSqlPipeline::append<std::string_view>(std::string_view,
                                                      std::string_view&&);

}  // namespace atom::database
