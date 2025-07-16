/**
 * @file redis.hpp
 * @brief A high-performance, thread-safe Redis client for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_REDIS_HPP
#define ATOM_SEARCH_REDIS_HPP

#include <hiredis/hiredis.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atom::database {

/**
 * @brief Custom exception for Redis-related errors.
 */
class RedisException : public std::runtime_error {
public:
    explicit RedisException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Encapsulates parameters for a Redis database connection.
 */
struct RedisConnectionParams {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password;
    int db = 0;
    struct timeval timeout = {1, 500000};  // 1.5 seconds
};

/**
 * @class RedisReply
 * @brief A RAII wrapper for redisReply, providing convenient access to reply data.
 */
class RedisReply {
public:
    explicit RedisReply(redisReply* r);
    ~RedisReply() = default;

    RedisReply(const RedisReply&) = delete;
    RedisReply& operator=(const RedisReply&) = delete;
    RedisReply(RedisReply&&) noexcept;
    RedisReply& operator=(RedisReply&&) noexcept;

    [[nodiscard]] int type() const { return reply_->type; }
    [[nodiscard]] long long integer() const;
    [[nodiscard]] std::optional<std::string> str() const;
    [[nodiscard]] std::vector<RedisReply> elements() const;
    [[nodiscard]] bool is_nil() const { return type() == REDIS_REPLY_NIL; }

private:
    std::unique_ptr<redisReply, decltype(&freeReplyObject)> reply_;
};

class RedisPipeline;

/**
 * @class RedisDB
 * @brief A high-performance, thread-safe Redis client using a connection pool.
 *
 * This class provides a modern C++ interface for Redis commands, managing a
 * pool of connections to handle concurrent requests efficiently.
 */
class RedisDB {
public:
    /**
     * @brief Constructs a RedisDB object and initializes the connection pool.
     * @param params The connection parameters.
     * @param pool_size The number of connections in the pool. Defaults to
     * hardware concurrency.
     * @throws RedisException if the connection pool cannot be initialized.
     */
    explicit RedisDB(const RedisConnectionParams& params, unsigned int pool_size = 0);

    ~RedisDB();

    RedisDB(const RedisDB&) = delete;
    RedisDB& operator=(const RedisDB&) = delete;
    RedisDB(RedisDB&&) = delete;
    RedisDB& operator=(RedisDB&&) = delete;

    /**
     * @brief Executes a Redis command with variadic arguments.
     * @tparam Args The types of the command arguments.
     * @param cmd The command string.
     * @param args The arguments for the command.
     * @return A RedisReply object wrapping the response.
     * @throws RedisException on failure.
     */
    template <typename... Args>
    [[nodiscard]] RedisReply command(std::string_view cmd, Args&&... args);

    // Key commands
    int64_t del(const std::vector<std::string_view>& keys);
    bool expire(std::string_view key, int seconds);
    int64_t ttl(std::string_view key);
    bool exists(const std::vector<std::string_view>& keys);

    // String commands
    void set(std::string_view key, std::string_view value);
    void setex(std::string_view key, int seconds, std::string_view value);
    [[nodiscard]] std::optional<std::string> get(std::string_view key);
    int64_t incr(std::string_view key);

    // Hash commands
    void hset(std::string_view key, std::string_view field,
              std::string_view value);
    [[nodiscard]] std::optional<std::string> hget(std::string_view key,
                                                  std::string_view field);
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> hgetall(
        std::string_view key);
    int64_t hdel(std::string_view key, const std::vector<std::string_view>& fields);

    // List commands
    int64_t lpush(std::string_view key, const std::vector<std::string_view>& values);
    std::optional<std::string> lpop(std::string_view key);
    int64_t rpush(std::string_view key, const std::vector<std::string_view>& values);
    std::optional<std::string> rpop(std::string_view key);
    std::vector<std::string> lrange(std::string_view key, int64_t start, int64_t stop);

    // Set commands
    int64_t sadd(std::string_view key, const std::vector<std::string_view>& members);
    int64_t srem(std::string_view key, const std::vector<std::string_view>& members);
    std::vector<std::string> smembers(std::string_view key);
    bool sismember(std::string_view key, std::string_view member);

    // Pub/Sub
    int64_t publish(std::string_view channel, std::string_view message);

    /**
     * @brief Pings the Redis server.
     * @return The server's response, typically "PONG".
     */
    std::string ping();

    /**
     * @brief Creates a pipeline for batching commands.
     * @return A unique_ptr to a RedisPipeline object.
     */
    [[nodiscard]] std::unique_ptr<RedisPipeline> pipeline();

    /**
     * @brief Executes a series of commands in a pipeline.
     * @param func A function that takes a RedisPipeline reference and appends
     * commands to it.
     * @return A vector of RedisReply objects for each command in the pipeline.
     */
    std::vector<RedisReply> with_pipeline(
        const std::function<void(RedisPipeline&)>& func);

private:
    class Impl;
    std::unique_ptr<Impl> p_impl_;
};

/**
 * @class RedisPipeline
 * @brief A class for batching Redis commands to improve performance.
 *
 * Commands are appended and then executed in a single network round-trip.
 */
class RedisPipeline {
public:
    ~RedisPipeline();

    RedisPipeline(const RedisPipeline&) = delete;
    RedisPipeline& operator=(const RedisPipeline&) = delete;
    RedisPipeline(RedisPipeline&&) noexcept;
    RedisPipeline& operator=(RedisPipeline&&) noexcept;

    /**
     * @brief Appends a command to the pipeline.
     * @tparam Args The types of the command arguments.
     * @param cmd The command string.
     * @param args The arguments for the command.
     */
    template <typename... Args>
    void append_command(std::string_view cmd, Args&&... args);

    /**
     * @brief Executes all appended commands.
     * @return A vector of RedisReply objects.
     * @throws RedisException on failure.
     */
    [[nodiscard]] std::vector<RedisReply> execute();

private:
    friend class RedisDB;
    explicit RedisPipeline(
        std::unique_ptr<redisContext, std::function<void(redisContext*)>> conn);

    std::unique_ptr<redisContext, std::function<void(redisContext*)>> conn_;
    int command_count_ = 0;
};

}  // namespace atom::database

#endif  // ATOM_SEARCH_REDIS_HPP
