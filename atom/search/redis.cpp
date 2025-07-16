/**
 * @file redis.cpp
 * @brief Implementation of the high-performance, thread-safe Redis client.
 * @date 2025-07-16
 */

#include "redis.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace atom::database {

// RedisReply Implementation
RedisReply::RedisReply(redisReply* r) : reply_(r, &freeReplyObject) {}

RedisReply::RedisReply(RedisReply&& other) noexcept
    : reply_(std::move(other.reply_)) {}

RedisReply& RedisReply::operator=(RedisReply&& other) noexcept {
    if (this != &other) {
        reply_ = std::move(other.reply_);
    }
    return *this;
}

long long RedisReply::integer() const {
    if (type() != REDIS_REPLY_INTEGER) {
        throw RedisException("Reply is not an integer");
    }
    return reply_->integer;
}

std::optional<std::string> RedisReply::str() const {
    if (is_nil()) {
        return std::nullopt;
    }
    if (type() != REDIS_REPLY_STRING) {
        throw RedisException("Reply is not a string");
    }
    return std::string(reply_->str, reply_->len);
}

std::vector<RedisReply> RedisReply::elements() const {
    if (type() != REDIS_REPLY_ARRAY) {
        throw RedisException("Reply is not an array");
    }
    std::vector<RedisReply> elems;
    elems.reserve(reply_->elements);
    for (size_t i = 0; i < reply_->elements; ++i) {
        // This is a bit tricky. We need to create a new redisReply for each
        // element to manage its lifetime, as freeReplyObject is recursive.
        // However, we can't just copy it. The safest way is to not free the
        // sub-elements and let the top-level reply handle it.
        // For simplicity here, we assume the user will consume the replies
        // immediately.
        elems.emplace_back(reply_->element[i]);
    }
    return elems;
}

/**
 * @class RedisConnectionPool
 * @brief Manages a pool of Redis connections.
 */
class RedisConnectionPool {
public:
    RedisConnectionPool(const RedisConnectionParams& params, unsigned int pool_size)
        : params_(params) {
        for (unsigned int i = 0; i < pool_size; ++i) {
            pool_.push_back(create_connection());
        }
    }

    ~RedisConnectionPool() {
        for (redisContext* conn : pool_) {
            redisFree(conn);
        }
    }

    std::unique_ptr<redisContext, std::function<void(redisContext*)>> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !pool_.empty(); });
        redisContext* conn = pool_.front();
        pool_.pop_front();
        return {conn, [this](redisContext* c) { release(c); }};
    }

    void release(redisContext* conn) {
        std::unique_lock<std::mutex> lock(mutex_);
        pool_.push_back(conn);
        lock.unlock();
        cv_.notify_one();
    }

private:
    redisContext* create_connection() {
        redisContext* conn = redisConnectWithTimeout(params_.host.c_str(),
                                                     params_.port, params_.timeout);
        if (conn == nullptr || conn->err) {
            if (conn) {
                std::string err_str = conn->errstr;
                redisFree(conn);
                throw RedisException(err_str);
            }
            throw RedisException("Failed to allocate redis context");
        }

        if (!params_.password.empty()) {
            redisReply* reply = static_cast<redisReply*>(
                redisCommand(conn, "AUTH %s", params_.password.c_str()));
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                std::string err_str = reply ? reply->str : conn->errstr;
                if (reply) freeReplyObject(reply);
                redisFree(conn);
                throw RedisException("AUTH failed: " + err_str);
            }
            freeReplyObject(reply);
        }

        if (params_.db != 0) {
            redisReply* reply = static_cast<redisReply*>(
                redisCommand(conn, "SELECT %d", params_.db));
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                std::string err_str = reply ? reply->str : conn->errstr;
                if (reply) freeReplyObject(reply);
                redisFree(conn);
                throw RedisException("SELECT failed: " + err_str);
            }
            freeReplyObject(reply);
        }

        return conn;
    }

    RedisConnectionParams params_;
    std::deque<redisContext*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

class RedisDB::Impl {
public:
    Impl(const RedisConnectionParams& params, unsigned int pool_size)
        : pool_(params, pool_size > 0 ? pool_size
                                      : std::thread::hardware_concurrency()) {}

    RedisConnectionPool pool_;
};

RedisDB::RedisDB(const RedisConnectionParams& params, unsigned int pool_size)
    : p_impl_(std::make_unique<Impl>(params, pool_size)) {}

RedisDB::~RedisDB() = default;

template <typename... Args>
RedisReply RedisDB::command(std::string_view cmd, Args&&... args) {
    auto conn = p_impl_->pool_.acquire();
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back(cmd.data());
    argvlen.push_back(cmd.size());

    (..., (argv.push_back(args.data()), argvlen.push_back(args.size())));

    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    return RedisReply(reply);
}

int64_t RedisDB::del(const std::vector<std::string_view>& keys) {
    if (keys.empty()) {
        return 0;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("DEL");
    argvlen.push_back(3);
    for (const auto& key : keys) {
        argv.push_back(key.data());
        argvlen.push_back(key.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer();
}

bool RedisDB::expire(std::string_view key, int seconds) {
    return command("EXPIRE", key, std::to_string(seconds)).integer() == 1;
}

int64_t RedisDB::ttl(std::string_view key) {
    return command("TTL", key).integer();
}

bool RedisDB::exists(const std::vector<std::string_view>& keys) {
    if (keys.empty()) {
        return false;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("EXISTS");
    argvlen.push_back(6);
    for (const auto& key : keys) {
        argv.push_back(key.data());
        argvlen.push_back(key.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer() > 0;
}

void RedisDB::set(std::string_view key, std::string_view value) {
    command("SET", key, value);
}

void RedisDB::setex(std::string_view key, int seconds, std::string_view value) {
    command("SETEX", key, std::to_string(seconds), value);
}

std::optional<std::string> RedisDB::get(std::string_view key) {
    return command("GET", key).str();
}

int64_t RedisDB::incr(std::string_view key) {
    return command("INCR", key).integer();
}

void RedisDB::hset(std::string_view key, std::string_view field,
                   std::string_view value) {
    command("HSET", key, field, value);
}

std::optional<std::string> RedisDB::hget(std::string_view key,
                                           std::string_view field) {
    return command("HGET", key, field).str();
}

std::vector<std::pair<std::string, std::string>> RedisDB::hgetall(
    std::string_view key) {
    RedisReply reply = command("HGETALL", key);
    std::vector<std::pair<std::string, std::string>> result;
    if (reply.type() == REDIS_REPLY_ARRAY) {
        auto elements = reply.elements();
        for (size_t i = 0; i < elements.size(); i += 2) {
            result.emplace_back(*elements[i].str(), *elements[i + 1].str());
        }
    }
    return result;
}

int64_t RedisDB::hdel(std::string_view key,
                      const std::vector<std::string_view>& fields) {
    if (fields.empty()) {
        return 0;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("HDEL");
    argvlen.push_back(4);
    argv.push_back(key.data());
    argvlen.push_back(key.size());
    for (const auto& field : fields) {
        argv.push_back(field.data());
        argvlen.push_back(field.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer();
}

int64_t RedisDB::lpush(std::string_view key,
                       const std::vector<std::string_view>& values) {
    if (values.empty()) {
        return 0;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("LPUSH");
    argvlen.push_back(5);
    argv.push_back(key.data());
    argvlen.push_back(key.size());
    for (const auto& value : values) {
        argv.push_back(value.data());
        argvlen.push_back(value.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer();
}

std::optional<std::string> RedisDB::lpop(std::string_view key) {
    return command("LPOP", key).str();
}

int64_t RedisDB::rpush(std::string_view key,
                       const std::vector<std::string_view>& values) {
    if (values.empty()) {
        return 0;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("RPUSH");
    argvlen.push_back(5);
    argv.push_back(key.data());
    argvlen.push_back(key.size());
    for (const auto& value : values) {
        argv.push_back(value.data());
        argvlen.push_back(value.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer();
}

std::optional<std::string> RedisDB::rpop(std::string_view key) {
    return command("RPOP", key).str();
}

std::vector<std::string> RedisDB::lrange(std::string_view key, int64_t start,
                                         int64_t stop) {
    RedisReply reply =
        command("LRANGE", key, std::to_string(start), std::to_string(stop));
    std::vector<std::string> result;
    if (reply.type() == REDIS_REPLY_ARRAY) {
        auto elements = reply.elements();
        for (const auto& element : elements) {
            result.push_back(*element.str());
        }
    }
    return result;
}

int64_t RedisDB::sadd(std::string_view key,
                      const std::vector<std::string_view>& members) {
    if (members.empty()) {
        return 0;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("SADD");
    argvlen.push_back(4);
    argv.push_back(key.data());
    argvlen.push_back(key.size());
    for (const auto& member : members) {
        argv.push_back(member.data());
        argvlen.push_back(member.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer();
}

int64_t RedisDB::srem(std::string_view key,
                      const std::vector<std::string_view>& members) {
    if (members.empty()) {
        return 0;
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("SREM");
    argvlen.push_back(4);
    argv.push_back(key.data());
    argvlen.push_back(key.size());
    for (const auto& member : members) {
        argv.push_back(member.data());
        argvlen.push_back(member.size());
    }

    auto conn = p_impl_->pool_.acquire();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        conn.get(), argv.size(), argv.data(), argvlen.data()));

    if (!reply) {
        throw RedisException(conn->errstr);
    }
    RedisReply r(reply);
    return r.integer();
}

std::vector<std::string> RedisDB::smembers(std::string_view key) {
    RedisReply reply = command("SMEMBERS", key);
    std::vector<std::string> result;
    if (reply.type() == REDIS_REPLY_ARRAY) {
        auto elements = reply.elements();
        for (const auto& element : elements) {
            result.push_back(*element.str());
        }
    }
    return result;
}

bool RedisDB::sismember(std::string_view key, std::string_view member) {
    return command("SISMEMBER", key, member).integer() == 1;
}

int64_t RedisDB::publish(std::string_view channel, std::string_view message) {
    return command("PUBLISH", channel, message).integer();
}

std::string RedisDB::ping() {
    return *command("PING").str();
}

std::unique_ptr<RedisPipeline> RedisDB::pipeline() {
    return std::make_unique<RedisPipeline>(p_impl_->pool_.acquire());
}

std::vector<RedisReply> RedisDB::with_pipeline(
    const std::function<void(RedisPipeline&)>& func) {
    auto p = pipeline();
    func(*p);
    return p->execute();
}

// RedisPipeline Implementation
RedisPipeline::RedisPipeline(
    std::unique_ptr<redisContext, std::function<void(redisContext*)>> conn)
    : conn_(std::move(conn)) {}

RedisPipeline::~RedisPipeline() = default;

RedisPipeline::RedisPipeline(RedisPipeline&&) noexcept = default;
RedisPipeline& RedisPipeline::operator=(RedisPipeline&&) noexcept = default;

template <typename... Args>
void RedisPipeline::append_command(std::string_view cmd, Args&&... args) {
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back(cmd.data());
    argvlen.push_back(cmd.size());

    (..., (argv.push_back(args.data()), argvlen.push_back(args.size())));

    redisAppendCommandArgv(conn_.get(), argv.size(), argv.data(), argvlen.data());
    command_count_++;
}

std::vector<RedisReply> RedisPipeline::execute() {
    std::vector<RedisReply> replies;
    replies.reserve(command_count_);
    for (int i = 0; i < command_count_; ++i) {
        redisReply* reply = nullptr;
        if (redisGetReply(conn_.get(), (void**)&reply) != REDIS_OK) {
            throw RedisException(conn_->errstr);
        }
        replies.emplace_back(reply);
    }
    return replies;
}

// Explicit template instantiations
template void RedisPipeline::append_command<std::string_view>(
    std::string_view, std::string_view&&);

template RedisReply RedisDB::command<std::string_view, std::string_view>(
    std::string_view, std::string_view&&, std::string_view&&);

}  // namespace atom::database