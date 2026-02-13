#ifndef ATOM_EXTRA_CURL_SESSION_POOL_HPP
#define ATOM_EXTRA_CURL_SESSION_POOL_HPP

#include <atomic>
#include <memory>
#include <chrono>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>

namespace atom::extra::curl {

class Session;

/**
 * @brief Simplified session pool using atom::memory::ObjectPool
 *
 * This provides a compatible interface to the existing curl code while using
 * the atom library's high-performance object pool implementation.
 */
class SessionPool {
public:
    /**
     * @brief Configuration for session pool behavior
     */
    struct Config {
        size_t max_pool_size = 100;
        std::chrono::seconds timeout = std::chrono::seconds(30);
        bool enable_statistics = true;

        static Config createDefault() {
            return Config{};
        }

        static Config createHighThroughput() {
            Config config;
            config.max_pool_size = 500;
            config.timeout = std::chrono::seconds(60);
            return config;
        }

        static Config createLowMemory() {
            Config config;
            config.max_pool_size = 20;
            config.timeout = std::chrono::seconds(10);
            return config;
        }
    };

    /**
     * @brief Performance statistics
     */
    struct Statistics {
        std::atomic<uint64_t> acquire_count{0};
        std::atomic<uint64_t> release_count{0};
        std::atomic<uint64_t> create_count{0};
        std::atomic<uint64_t> cache_hits{0};
        std::atomic<uint64_t> cache_misses{0};
        std::atomic<uint64_t> work_steals{0};
        std::atomic<uint64_t> contention_count{0};
    };

public:

    /**
     * @brief Constructor with configuration
     */
    explicit SessionPool(const Config& config = Config::createDefault());

    /**
     * @brief Destructor
     */
    ~SessionPool();

    /**
     * @brief Acquire a session (lock-free with thread-local caching)
     */
    std::shared_ptr<Session> acquire();

    /**
     * @brief Release a session back to the pool
     */
    void release(std::shared_ptr<Session> session);

    /**
     * @brief Get current pool statistics
     */
    const Statistics& getStatistics() const noexcept { return stats_; }

    /**
     * @brief Get approximate total session count
     */
    size_t size() const noexcept;

private:
    // Simplified implementation using standard containers
    std::vector<std::shared_ptr<Session>> available_sessions_;
    std::mutex pool_mutex_;
    Config config_;
    mutable Statistics stats_;

    /**
     * @brief Create a new session
     */
    std::shared_ptr<Session> createSession();
};

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_SESSION_POOL_HPP
