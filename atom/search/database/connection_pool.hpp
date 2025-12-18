/*
 * connection_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file connection_pool.hpp
 * @brief Generic connection pool for database connections.
 */

#ifndef ATOM_SEARCH_DATABASE_CONNECTION_POOL_HPP
#define ATOM_SEARCH_DATABASE_CONNECTION_POOL_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <vector>

#include <spdlog/spdlog.h>

namespace atom::search::database {

/**
 * @brief Configuration for connection pool.
 */
struct PoolConfig {
    size_t minConnections{1};
    size_t maxConnections{10};
    std::chrono::seconds connectionTimeout{30};
    std::chrono::seconds idleTimeout{300};
    std::chrono::seconds healthCheckInterval{60};
    size_t maxRetries{3};
    std::chrono::milliseconds retryDelay{100};
};

/**
 * @brief Exception for connection pool errors.
 */
class PoolException : public std::runtime_error {
public:
    explicit PoolException(const std::string& msg)
        : std::runtime_error("Connection pool error: " + msg) {}
};

/**
 * @brief RAII wrapper for pooled connections.
 * @tparam Connection The connection type.
 */
template <typename Connection>
class PooledConnection {
public:
    using ReleaseFunc = std::function<void(std::unique_ptr<Connection>)>;

    PooledConnection() = default;

    PooledConnection(std::unique_ptr<Connection> conn, ReleaseFunc releaseFunc)
        : connection_(std::move(conn)), releaseFunc_(std::move(releaseFunc)) {}

    ~PooledConnection() {
        if (connection_ && releaseFunc_) {
            try {
                releaseFunc_(std::move(connection_));
            } catch (...) {
                spdlog::error("Exception during connection release");
            }
        }
    }

    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;

    PooledConnection(PooledConnection&& other) noexcept
        : connection_(std::move(other.connection_)),
          releaseFunc_(std::move(other.releaseFunc_)) {
        other.releaseFunc_ = nullptr;
    }

    PooledConnection& operator=(PooledConnection&& other) noexcept {
        if (this != &other) {
            if (connection_ && releaseFunc_) {
                releaseFunc_(std::move(connection_));
            }
            connection_ = std::move(other.connection_);
            releaseFunc_ = std::move(other.releaseFunc_);
            other.releaseFunc_ = nullptr;
        }
        return *this;
    }

    Connection* operator->() { return connection_.get(); }
    const Connection* operator->() const { return connection_.get(); }

    Connection& operator*() { return *connection_; }
    const Connection& operator*() const { return *connection_; }

    explicit operator bool() const noexcept { return connection_ != nullptr; }

    Connection* get() { return connection_.get(); }
    const Connection* get() const { return connection_.get(); }

private:
    std::unique_ptr<Connection> connection_;
    ReleaseFunc releaseFunc_;
};

/**
 * @brief Generic connection pool implementation.
 * @tparam Connection The connection type.
 * @tparam Factory Factory function type for creating connections.
 */
template <typename Connection>
class ConnectionPool {
public:
    using ConnectionPtr = std::unique_ptr<Connection>;
    using Factory = std::function<ConnectionPtr()>;
    using Validator = std::function<bool(Connection&)>;

    /**
     * @brief Construct a connection pool.
     * @param factory Function to create new connections.
     * @param config Pool configuration.
     * @param validator Optional function to validate connections.
     */
    explicit ConnectionPool(Factory factory, PoolConfig config = {},
                            Validator validator = nullptr)
        : factory_(std::move(factory)),
          config_(std::move(config)),
          validator_(std::move(validator)),
          shutdown_(false),
          activeConnections_(0) {
        initializePool();
    }

    ~ConnectionPool() { shutdown(); }

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    /**
     * @brief Acquire a connection from the pool.
     * @param timeout Maximum time to wait for a connection.
     * @return A pooled connection wrapper.
     * @throws PoolException if no connection available within timeout.
     */
    PooledConnection<Connection> acquire(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (pool_.empty()) {
            if (shutdown_) {
                throw PoolException("Pool is shutting down");
            }

            // Try to create a new connection if under max limit
            if (activeConnections_ < config_.maxConnections) {
                lock.unlock();
                auto conn = createConnection();
                if (conn) {
                    ++activeConnections_;
                    return wrapConnection(std::move(conn));
                }
                lock.lock();
            }

            // Wait for a connection to be released
            if (condition_.wait_until(lock, deadline) ==
                std::cv_status::timeout) {
                throw PoolException("Timeout waiting for connection");
            }
        }

        auto conn = std::move(pool_.front());
        pool_.pop();

        // Validate connection before returning
        if (validator_ && !validator_(*conn)) {
            spdlog::warn("Connection validation failed, creating new one");
            --activeConnections_;
            lock.unlock();
            conn = createConnection();
            if (conn) {
                ++activeConnections_;
            } else {
                throw PoolException("Failed to create replacement connection");
            }
        }

        return wrapConnection(std::move(conn));
    }

    /**
     * @brief Get current pool statistics.
     */
    struct Stats {
        size_t availableConnections;
        size_t activeConnections;
        size_t maxConnections;
    };

    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {pool_.size(), activeConnections_.load(),
                config_.maxConnections};
    }

    /**
     * @brief Shutdown the pool and close all connections.
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        while (!pool_.empty()) {
            pool_.pop();
        }
        activeConnections_ = 0;
        condition_.notify_all();
        spdlog::debug("Connection pool shutdown complete");
    }

    /**
     * @brief Check if the pool is running.
     */
    [[nodiscard]] bool isRunning() const noexcept { return !shutdown_; }

private:
    void initializePool() {
        for (size_t i = 0; i < config_.minConnections; ++i) {
            auto conn = createConnection();
            if (conn) {
                pool_.push(std::move(conn));
                ++activeConnections_;
            }
        }
        spdlog::debug("Connection pool initialized with {} connections",
                      pool_.size());
    }

    ConnectionPtr createConnection() {
        for (size_t attempt = 0; attempt < config_.maxRetries; ++attempt) {
            try {
                auto conn = factory_();
                if (conn) {
                    return conn;
                }
            } catch (const std::exception& e) {
                spdlog::warn("Connection creation attempt {} failed: {}",
                             attempt + 1, e.what());
                if (attempt + 1 < config_.maxRetries) {
                    std::this_thread::sleep_for(config_.retryDelay);
                }
            }
        }
        spdlog::error("Failed to create connection after {} attempts",
                      config_.maxRetries);
        return nullptr;
    }

    void releaseConnection(ConnectionPtr conn) {
        if (!conn || shutdown_) {
            if (conn) {
                --activeConnections_;
            }
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.size() < config_.maxConnections) {
            pool_.push(std::move(conn));
            condition_.notify_one();
        } else {
            --activeConnections_;
        }
    }

    PooledConnection<Connection> wrapConnection(ConnectionPtr conn) {
        return PooledConnection<Connection>(
            std::move(conn),
            [this](ConnectionPtr c) { releaseConnection(std::move(c)); });
    }

    Factory factory_;
    PoolConfig config_;
    Validator validator_;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<ConnectionPtr> pool_;

    std::atomic<bool> shutdown_;
    std::atomic<size_t> activeConnections_;
};

}  // namespace atom::search::database

#endif  // ATOM_SEARCH_DATABASE_CONNECTION_POOL_HPP
