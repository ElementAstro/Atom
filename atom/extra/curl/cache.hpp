#ifndef ATOM_EXTRA_CURL_CACHE_HPP
#define ATOM_EXTRA_CURL_CACHE_HPP

#include "response.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <array>
#include <map>
#include <spdlog/spdlog.h>

namespace atom::extra::curl {

/**
 * @brief Lock-free cache with epoch-based memory management
 *
 * This implementation provides a high-performance concurrent hash map
 * using atomic operations, compare-and-swap, and epoch-based memory
 * reclamation for safe lock-free operations.
 */
class Cache {
private:
    /**
     * @brief Cache entry with atomic operations support
     */
    struct CacheEntry {
        Response response;
        std::chrono::system_clock::time_point expires;
        std::string etag;
        std::string last_modified;
        std::atomic<uint64_t> version{0};  // For ABA protection
        std::atomic<bool> marked_for_deletion{false};

        CacheEntry() = default;
        CacheEntry(Response resp, std::chrono::system_clock::time_point exp,
                   std::string et, std::string lm)
            : response(std::move(resp)), expires(exp),
              etag(std::move(et)), last_modified(std::move(lm)) {}
    };

    /**
     * @brief Hash table bucket with atomic pointer
     */
    struct Bucket {
        struct Node {
            std::string key;
            std::atomic<std::shared_ptr<CacheEntry>> entry;
            std::atomic<Node*> next;
            std::atomic<uint64_t> version{0};

            Node(std::string k) : key(std::move(k)), next(nullptr) {}
        };

        alignas(64) std::atomic<Node*> head{nullptr};  // Cache line aligned
    };

    /**
     * @brief Epoch-based memory management
     */
    class EpochManager {
    private:
        static constexpr size_t MAX_THREADS = 64;
        static constexpr size_t EPOCHS = 3;

        struct alignas(64) ThreadEpoch {
            std::atomic<uint64_t> epoch{0};
            std::atomic<std::thread::id> thread_id{};
            std::atomic<bool> active{false};
        };

        alignas(64) std::atomic<uint64_t> global_epoch_{0};
        std::array<ThreadEpoch, MAX_THREADS> thread_epochs_;

        // Retired objects per epoch
        struct RetiredList {
            std::atomic<Bucket::Node*> head{nullptr};
            std::atomic<size_t> count{0};
        };
        std::array<RetiredList, EPOCHS> retired_lists_;

        thread_local static size_t thread_index_;

    public:
        EpochManager() = default;

        /**
         * @brief Enter epoch (called before accessing shared data)
         */
        void enter() noexcept;

        /**
         * @brief Exit epoch (called after accessing shared data)
         */
        void exit() noexcept;

        /**
         * @brief Retire a node for safe deletion
         */
        void retire(Bucket::Node* node) noexcept;

        /**
         * @brief Try to advance global epoch and reclaim memory
         */
        void tryAdvanceEpoch() noexcept;

    private:
        size_t getThreadIndex() noexcept;
        uint64_t getMinEpoch() const noexcept;
        void reclaimEpoch(size_t epoch_index) noexcept;
    };

public:
    /**
     * @brief Configuration for cache behavior
     */
    struct Config {
        std::chrono::seconds default_ttl = std::chrono::minutes(5);
        size_t initial_bucket_count = 1024;
        double load_factor_threshold = 0.75;
        bool enable_statistics = true;
        size_t max_entries = 10000;

        static Config createDefault() {
            return Config{};
        }

        static Config createHighPerformance() {
            Config config;
            config.initial_bucket_count = 4096;
            config.max_entries = 50000;
            return config;
        }
    };

    /**
     * @brief Cache statistics
     */
    struct Statistics {
        std::atomic<uint64_t> get_count{0};
        std::atomic<uint64_t> set_count{0};
        std::atomic<uint64_t> hit_count{0};
        std::atomic<uint64_t> miss_count{0};
        std::atomic<uint64_t> eviction_count{0};
        std::atomic<uint64_t> collision_count{0};

        double getHitRatio() const noexcept {
            uint64_t total = get_count.load(std::memory_order_relaxed);
            return total > 0 ? static_cast<double>(hit_count.load(std::memory_order_relaxed)) / total : 0.0;
        }
    };

    /**
     * @brief Constructor with configuration
     */
    explicit Cache(const Config& config = Config::createDefault());

    /**
     * @brief Legacy constructor for compatibility
     */
    Cache(std::chrono::seconds default_ttl);


    /**
     * @brief Destructor
     */
    ~Cache();

    /**
     * @brief Set a cache entry (lock-free)
     */
    void set(const std::string& url, const Response& response,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * @brief Get a cached response (lock-free)
     */
    std::optional<Response> get(const std::string& url);

    /**
     * @brief Invalidate a cache entry (lock-free)
     */
    void invalidate(const std::string& url);

    /**
     * @brief Clear entire cache (lock-free)
     */
    void clear();

    /**
     * @brief Get validation headers for conditional requests
     */
    std::map<std::string, std::string> get_validation_headers(const std::string& url);

    /**
     * @brief Handle 304 Not Modified response
     */
    void handle_not_modified(const std::string& url);

    /**
     * @brief Get cache statistics
     */
    const Statistics& getStatistics() const noexcept { return stats_; }

    /**
     * @brief Get approximate cache size
     */
    size_t size() const noexcept;

private:
    const Config config_;
    mutable Statistics stats_;

    // Hash table with lock-free buckets
    std::unique_ptr<Bucket[]> buckets_;
    std::atomic<size_t> bucket_count_;
    std::atomic<size_t> entry_count_{0};

    // Epoch-based memory management
    std::unique_ptr<EpochManager> epoch_manager_;

    // Stale entries for validation (using atomic shared_ptr)
    struct StaleEntry {
        std::string etag;
        std::string last_modified;
        std::chrono::system_clock::time_point original_expires;
    };
    std::unique_ptr<Bucket[]> stale_buckets_;

    /**
     * @brief Hash function for URLs
     */
    size_t hash(const std::string& url) const noexcept;

    /**
     * @brief Find bucket for given URL
     */
    Bucket& getBucket(const std::string& url) const noexcept;

    /**
     * @brief Find stale bucket for given URL
     */
    Bucket& getStaleBucket(const std::string& url) const noexcept;

    /**
     * @brief Find node in bucket (with epoch protection)
     */
    Bucket::Node* findNode(Bucket& bucket, const std::string& url) const noexcept;

    /**
     * @brief Insert or update node in bucket
     */
    bool insertOrUpdate(Bucket& bucket, const std::string& url,
                       std::shared_ptr<CacheEntry> entry) noexcept;

    /**
     * @brief Remove node from bucket
     */
    bool removeNode(Bucket& bucket, const std::string& url) noexcept;

    /**
     * @brief Check if entry is expired
     */
    bool isExpired(const CacheEntry& entry) const noexcept;

    /**
     * @brief Try to resize hash table if needed
     */
    void tryResize() noexcept;
};

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_CACHE_HPP
