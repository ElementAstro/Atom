/*
 * object_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-04-05

Description: An enhanced implementation of object pool with
automatic object release, better exception handling, and additional
functionalities. Optional Boost support can be enabled with ATOM_USE_BOOST.

**************************************************/

#ifndef ATOM_MEMORY_OBJECT_POOL_HPP
#define ATOM_MEMORY_OBJECT_POOL_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <immintrin.h>  // For memory prefetching

#include "atom/error/exception.hpp"

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifdef ATOM_USE_BOOST
#include <boost/pool/object_pool.hpp>
#endif

namespace atom::memory {

/**
 * @brief Concept for objects that can be reset to a clean state
 */
template <typename T>
concept Resettable = requires(T& obj) { obj.reset(); };

/**
 * @brief A thread-safe, high-performance object pool for managing reusable
 * objects.
 *
 * This implementation provides:
 * - Automatic object release with custom deleters
 * - Configurable timeout and validation mechanisms
 * - Statistical tracking and monitoring
 * - Enhanced memory management and concurrency
 * - Optional batch operations and priority-based allocation
 *
 * @tparam T The type of objects managed by the pool. Must satisfy the
 * Resettable concept.
 */
template <Resettable T>
class ObjectPool {
public:
    using CreateFunc = std::function<std::shared_ptr<T>()>;

    /**
     * @brief Statistics about the object pool's performance and usage
     */
    struct alignas(CACHE_LINE_SIZE) PoolStats {
        // Basic statistics (atomic for thread safety)
        std::atomic<size_t> hits{0};  ///< Number of times an object was reused from the pool
        std::atomic<size_t> misses{0};    ///< Number of times a new object had to be created
        std::atomic<size_t> cleanups{0};  ///< Number of objects removed during cleanup
        std::atomic<size_t> peak_usage{0};  ///< Maximum number of objects in use at once
        std::atomic<size_t> wait_count{0};  ///< Number of times clients had to wait for an object
        std::atomic<size_t> timeout_count{0};  ///< Number of times acquire operations timed out

        // Advanced performance metrics
        std::atomic<size_t> total_acquisitions{0};  ///< Total acquisition attempts
        std::atomic<size_t> total_releases{0};      ///< Total object releases
        std::atomic<size_t> validation_failures{0}; ///< Objects failed validation
        std::atomic<size_t> cleanup_operations{0};  ///< Number of cleanup operations
        std::atomic<size_t> batch_acquisitions{0};  ///< Number of batch acquisitions
        std::atomic<size_t> memory_reuses{0};       ///< Objects reused from pool
        std::atomic<size_t> memory_allocations{0};  ///< New objects created
        std::atomic<size_t> lock_contentions{0};    ///< Number of lock contentions

        // Timing statistics (in nanoseconds for precision)
        std::atomic<uint64_t> total_wait_time{0};      ///< Total time spent waiting for objects
        std::atomic<uint64_t> max_wait_time{0};        ///< Maximum time spent waiting for an object
        std::atomic<uint64_t> total_acquisition_time{0}; ///< Total acquisition time
        std::atomic<uint64_t> max_acquisition_time{0};   ///< Maximum acquisition time
        std::atomic<uint64_t> total_validation_time{0};  ///< Total validation time
        std::atomic<uint64_t> total_lock_wait_time{0};   ///< Total lock wait time

        // Performance calculation helpers
        double getHitRatio() const noexcept {
            size_t total_requests = hits.load() + misses.load();
            return total_requests > 0 ? static_cast<double>(hits.load()) / total_requests : 0.0;
        }

        double getAverageWaitTime() const noexcept {
            size_t count = wait_count.load();
            return count > 0 ? static_cast<double>(total_wait_time.load()) / count : 0.0;
        }

        double getAverageAcquisitionTime() const noexcept {
            size_t count = total_acquisitions.load();
            return count > 0 ? static_cast<double>(total_acquisition_time.load()) / count : 0.0;
        }

        double getMemoryReuseRatio() const noexcept {
            size_t total_objects = memory_reuses.load() + memory_allocations.load();
            return total_objects > 0 ? static_cast<double>(memory_reuses.load()) / total_objects : 0.0;
        }

        void reset() noexcept {
            hits = 0; misses = 0; cleanups = 0; peak_usage = 0;
            wait_count = 0; timeout_count = 0; total_acquisitions = 0;
            total_releases = 0; validation_failures = 0; cleanup_operations = 0;
            batch_acquisitions = 0; memory_reuses = 0; memory_allocations = 0;
            lock_contentions = 0; total_wait_time = 0; max_wait_time = 0;
            total_acquisition_time = 0; max_acquisition_time = 0;
            total_validation_time = 0; total_lock_wait_time = 0;
        }
    };

    /**
     * @brief Enhanced configuration options for the object pool
     */
    struct PoolConfig {
        // Basic configuration
        bool enable_stats{true};  ///< Whether to collect usage statistics
        bool enable_auto_cleanup{true};  ///< Whether to automatically clean idle objects
        bool validate_on_acquire{false};  ///< Whether to validate objects on acquisition
        bool validate_on_release{true};  ///< Whether to validate objects on release

        // Performance optimization settings
        bool enable_prefetching{true};   ///< Enable memory prefetching for better cache performance
        bool enable_batch_optimization{true}; ///< Enable batch operation optimizations
        bool enable_priority_queue{true}; ///< Enable priority-based acquisition
        bool enable_lock_free_stats{true}; ///< Use lock-free statistics updates

        // Timing and cleanup configuration
        std::chrono::minutes cleanup_interval{10};  ///< How often to run cleanup
        std::chrono::minutes max_idle_time{30};     ///< Maximum time an object can remain idle
        std::chrono::milliseconds acquisition_timeout{5000}; ///< Default acquisition timeout
        std::chrono::milliseconds validation_timeout{100};   ///< Validation operation timeout

        // Pool sizing and growth
        size_t initial_pool_size{0};     ///< Initial number of objects to create
        size_t max_pool_growth{100};     ///< Maximum objects to create in one growth operation
        double growth_factor{1.5};       ///< Factor by which to grow the pool
        size_t shrink_threshold{50};     ///< Percentage of unused objects before shrinking

        // Validation and monitoring
        std::function<bool(const T&)> validator{nullptr}; ///< Optional custom validator function
        std::function<void(const T&)> object_initializer{nullptr}; ///< Optional object initializer
        std::function<void(const PoolStats&)> stats_callback{nullptr}; ///< Optional stats callback

        // Advanced features
        bool enable_object_warming{false}; ///< Pre-warm objects during idle time
        bool enable_adaptive_sizing{false}; ///< Automatically adjust pool size based on usage
        bool enable_memory_pressure_handling{false}; ///< Handle memory pressure events
        size_t memory_pressure_threshold{80}; ///< Memory usage percentage to trigger pressure handling
    };

    /**
     * @brief Priority levels for object acquisition
     */
    enum class Priority { Low, Normal, High, Critical };

    /**
     * @brief Constructs an ObjectPool with a specified maximum size and an
     * optional custom object creator.
     *
     * @param max_size The maximum number of objects the pool can hold.
     * @param initial_size The initial number of objects to prefill the pool
     * with.
     * @param creator A function to create new objects. Defaults to
     * std::make_shared<T>().
     * @param config Configuration options for the pool.
     */
    explicit ObjectPool(
        size_t max_size, size_t initial_size = 0,
        CreateFunc creator = []() { return std::make_shared<T>(); },
        const PoolConfig& config = PoolConfig{})
        : max_size_(max_size),
          available_(max_size),
          creator_(std::move(creator)),
          config_(config),
          last_cleanup_(std::chrono::steady_clock::now())
#ifdef ATOM_USE_BOOST
          ,
          boost_pool_(max_size)
#endif
    {
        assert(max_size_ > 0 && "ObjectPool size must be greater than zero.");

        // Reserve capacity to avoid reallocations
        pool_.reserve(max_size_);
        if (config_.enable_auto_cleanup) {
            idle_objects_.reserve(max_size_);
        }

        prefill(initial_size);
    }

    // Disable copy and assignment
    ObjectPool(const ObjectPool&) = default;
    ObjectPool& operator=(const ObjectPool&) = default;

    // Allow move operations
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    /**
     * @brief Destructor - ensures all objects are properly cleaned up
     */
    ~ObjectPool() {
        std::unique_lock lock(mutex_);
        pool_.clear();
        idle_objects_.clear();
    }

    /**
     * @brief Acquires an object from the pool. Blocks if no objects are
     * available.
     *
     * @param priority The priority level for this acquisition request.
     * @return A shared pointer to the acquired object with a custom deleter.
     * @throw std::runtime_error If the pool is full and no object is available.
     */
    [[nodiscard]] std::shared_ptr<T> acquire(
        Priority priority = Priority::Normal) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Try fast path first - check for pre-warmed objects without full locking
        if (config_.enable_object_warming) {
            std::shared_lock<std::shared_mutex> read_lock(mutex_);
            if (auto warmed_obj = tryGetWarmedObject()) {
                fast_path_acquisitions_.fetch_add(1, std::memory_order_relaxed);
                prefetchObject(warmed_obj);

                if (config_.enable_stats) {
                    stats_.total_acquisitions.fetch_add(1, std::memory_order_relaxed);
                    stats_.memory_reuses.fetch_add(1, std::memory_order_relaxed);
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now() - start_time).count();
                    updateTimingStats(duration, stats_.total_acquisition_time, stats_.max_acquisition_time);
                }

                return wrapWithDeleter(std::move(warmed_obj));
            }
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (available_ == 0 && pool_.empty()) {
            THROW_RUNTIME_ERROR("ObjectPool is full");
        }

        bool waited = false;
        auto lock_acquired_time = std::chrono::high_resolution_clock::now();

        if (pool_.empty() && available_ == 0) {
            if (config_.enable_stats) {
                stats_.wait_count.fetch_add(1, std::memory_order_relaxed);
                stats_.lock_contentions.fetch_add(1, std::memory_order_relaxed);
            }
            waited = true;
            waiting_priorities_.push_back(priority);
            cv_.wait(lock, [this, priority] {
                return (!pool_.empty() || available_ > 0) &&
                       (waiting_priorities_.empty() ||
                        waiting_priorities_.front() <= priority);
            });
            waiting_priorities_.erase(
                std::remove(waiting_priorities_.begin(),
                            waiting_priorities_.end(), priority),
                waiting_priorities_.end());
        }

        if (config_.enable_stats) {
            stats_.total_acquisitions.fetch_add(1, std::memory_order_relaxed);

            if (waited) {
                auto wait_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::high_resolution_clock::now() - start_time).count();
                updateTimingStats(wait_duration, stats_.total_wait_time, stats_.max_wait_time);
            }

            auto lock_wait_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                lock_acquired_time - start_time).count();
            updateTimingStats(lock_wait_duration, stats_.total_lock_wait_time, stats_.max_acquisition_time);
        }

        // Track recent acquisition patterns for adaptive sizing
        ++recent_acquisition_count_;

        if (config_.enable_auto_cleanup) {
            tryCleanupLocked();
        }

        auto result = acquireImpl(lock);

        // Prefetch the acquired object and track it
        prefetchObject(result);
        last_acquired_object_.store(result.get(), std::memory_order_relaxed);

        return result;
    }

    /**
     * @brief Acquires an object from the pool with a timeout.
     *
     * @param timeout_duration The maximum duration to wait for an available
     * object.
     * @param priority The priority level for this acquisition request.
     * @return A shared pointer to the acquired object or nullptr if the timeout
     * expires.
     * @throw std::runtime_error If the pool is full and no object is available.
     */
    template <typename Rep, typename Period>
    [[nodiscard]] std::optional<std::shared_ptr<T>> tryAcquireFor(
        const std::chrono::duration<Rep, Period>& timeout_duration,
        Priority priority = Priority::Normal) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (available_ == 0 && pool_.empty()) {
            THROW_RUNTIME_ERROR("ObjectPool is full");
        }

        auto start_time = std::chrono::steady_clock::now();
        bool waited = false;

        if (pool_.empty() && available_ == 0) {
            if (config_.enable_stats) {
                stats_.wait_count++;
            }
            waited = true;
            waiting_priorities_.push_back(priority);
            bool success =
                cv_.wait_for(lock, timeout_duration, [this, priority] {
                    return (!pool_.empty() || available_ > 0) &&
                           (waiting_priorities_.empty() ||
                            waiting_priorities_.front() <= priority);
                });
            waiting_priorities_.erase(
                std::remove(waiting_priorities_.begin(),
                            waiting_priorities_.end(), priority),
                waiting_priorities_.end());
            if (!success) {
                if (config_.enable_stats) {
                    stats_.timeout_count++;
                }
                return std::nullopt;
            }
        }

        if (config_.enable_stats && waited) {
            auto wait_duration = std::chrono::steady_clock::now() - start_time;
            stats_.total_wait_time += wait_duration;
            stats_.max_wait_time =
                std::max(stats_.max_wait_time, wait_duration);
        }

        if (config_.enable_auto_cleanup) {
            tryCleanupLocked();
        }

        return acquireImpl(lock);
    }

    /**
     * @brief Acquires an object that passes a validation check.
     *
     * @param validator Function that returns true if an object is valid.
     * @param priority The priority level for this acquisition request.
     * @return A shared pointer to a validated object.
     */
    [[nodiscard]] std::shared_ptr<T> acquireValidated(
        const std::function<bool(const T&)>& validator,
        Priority priority = Priority::Normal) {
        std::unique_lock lock(mutex_);

        auto start_time = std::chrono::steady_clock::now();
        bool waited = false;

        while (true) {
            // If we need to wait for objects to become available
            if (pool_.empty() && available_ == 0) {
                if (config_.enable_stats && !waited) {
                    stats_.wait_count++;
                    waited = true;
                }

                waiting_priorities_.push_back(priority);

                cv_.wait(lock, [this, priority] {
                    return (!pool_.empty() || available_ > 0) &&
                           (waiting_priorities_.empty() ||
                            waiting_priorities_.front() <= priority);
                });

                waiting_priorities_.erase(
                    std::remove(waiting_priorities_.begin(),
                                waiting_priorities_.end(), priority),
                    waiting_priorities_.end());
            }

            // Try to find a valid object in the pool
            if (!pool_.empty()) {
                auto it = std::find_if(
                    pool_.begin(), pool_.end(),
                    [&validator](const auto& obj) { return validator(*obj); });

                if (it != pool_.end()) {
                    // Found a valid object
                    auto obj = std::move(*it);
                    pool_.erase(it);

                    if (config_.enable_stats) {
                        stats_.hits++;
                        if (waited) {
                            auto wait_duration =
                                std::chrono::steady_clock::now() - start_time;
                            stats_.total_wait_time += wait_duration;
                            stats_.max_wait_time =
                                std::max(stats_.max_wait_time, wait_duration);
                        }
                    }

                    return wrapWithDeleter(std::move(obj));
                }
            }

            // If no valid objects in pool, create a new one if possible
            if (available_ > 0) {
                --available_;

                if (config_.enable_stats) {
                    stats_.misses++;
                    if (waited) {
                        auto wait_duration =
                            std::chrono::steady_clock::now() - start_time;
                        stats_.total_wait_time += wait_duration;
                        stats_.max_wait_time =
                            std::max(stats_.max_wait_time, wait_duration);
                    }
                }

                auto obj = creator_();
                return wrapWithDeleter(std::move(obj));
            }

            // If we get here, we need to keep waiting for objects
        }
    }

    /**
     * @brief Acquires multiple objects from the pool at once.
     *
     * @param count Number of objects to acquire.
     * @param priority The priority level for this acquisition request.
     * @return Vector of shared pointers to acquired objects.
     * @throw std::runtime_error If requesting more objects than the pool can
     * provide.
     */
    [[nodiscard]] std::vector<std::shared_ptr<T>> acquireBatch(
        size_t count, Priority priority = Priority::Normal) {
        if (count == 0) {
            return {};
        }

        if (count > max_size_) {
            THROW_RUNTIME_ERROR(
                "Requested batch size exceeds pool maximum size");
        }

        std::vector<std::shared_ptr<T>> result;
        result.reserve(count);

        std::unique_lock lock(mutex_);

        auto start_time = std::chrono::steady_clock::now();
        bool waited = false;

        // Wait until we can satisfy the entire batch request
        if (pool_.size() + available_ < count) {
            if (config_.enable_stats) {
                stats_.wait_count++;
            }
            waited = true;

            waiting_priorities_.push_back(priority);

            cv_.wait(lock, [this, count, priority] {
                return (pool_.size() + available_ >= count) &&
                       (waiting_priorities_.empty() ||
                        waiting_priorities_.front() <= priority);
            });

            waiting_priorities_.erase(
                std::remove(waiting_priorities_.begin(),
                            waiting_priorities_.end(), priority),
                waiting_priorities_.end());
        }

        // Calculate wait time if tracking stats
        if (config_.enable_stats && waited) {
            auto wait_duration = std::chrono::steady_clock::now() - start_time;
            stats_.total_wait_time += wait_duration;
            stats_.max_wait_time =
                std::max(stats_.max_wait_time, wait_duration);
        }

        // First take objects from the pool
        size_t from_pool = std::min(pool_.size(), count);
        for (size_t i = 0; i < from_pool; ++i) {
            result.push_back(wrapWithDeleter(std::move(pool_.back())));
            pool_.pop_back();

            if (config_.enable_stats) {
                stats_.hits++;
            }
        }

        // Create new objects as needed
        size_t to_create = count - from_pool;
        for (size_t i = 0; i < to_create; ++i) {
            --available_;
            result.push_back(wrapWithDeleter(creator_()));

            if (config_.enable_stats) {
                stats_.misses++;
            }
        }

        // Update peak usage statistic
        if (config_.enable_stats) {
            size_t current_usage = max_size_ - available_;
            if (current_usage > stats_.peak_usage) {
                stats_.peak_usage = current_usage;
            }
        }

        return result;
    }

    /**
     * @brief Returns the number of available objects in the pool.
     *
     * @return The number of available objects.
     */
    [[nodiscard]] size_t available() const {
        std::shared_lock lock(mutex_);
        return available_ + pool_.size();
    }

    /**
     * @brief Returns the current size of the pool.
     *
     * @return The current number of objects in the pool.
     */
    [[nodiscard]] size_t size() const {
        std::shared_lock lock(mutex_);
        return max_size_ - available_ + pool_.size();
    }

    /**
     * @brief Gets the current number of in-use objects.
     *
     * @return The number of in-use objects.
     */
    [[nodiscard]] size_t inUseCount() const {
        std::shared_lock lock(mutex_);
        return max_size_ - available_;
    }

    /**
     * @brief Prefills the pool with a specified number of objects.
     *
     * @param count The number of objects to prefill the pool with.
     * @throw std::runtime_error If prefill exceeds the maximum pool size.
     */
    void prefill(size_t count) {
        std::unique_lock lock(mutex_);
        if (count > max_size_) {
            THROW_RUNTIME_ERROR("Prefill count exceeds maximum pool size.");
        }

        // Calculate how many new objects we need to create
        size_t to_create = count - pool_.size();
        if (to_create > available_) {
            THROW_RUNTIME_ERROR(
                "Not enough available slots to prefill the requested count.");
        }

        for (size_t i = 0; i < to_create; ++i) {
            pool_.emplace_back(creator_());
            --available_;
        }
    }

    /**
     * @brief Clears all objects from the pool.
     */
    void clear() {
        std::unique_lock lock(mutex_);
        pool_.clear();
        idle_objects_.clear();
        available_ = max_size_;
    }

    /**
     * @brief Resizes the pool to a new maximum size.
     *
     * @param new_max_size The new maximum size for the pool.
     * @throw std::runtime_error If the new size is smaller than the number of
     * prefilled objects.
     */
    void resize(size_t new_max_size) {
        std::unique_lock lock(mutex_);
        if (new_max_size < (max_size_ - available_)) {
            THROW_RUNTIME_ERROR(
                "New maximum size is smaller than the number of in-use "
                "objects.");
        }

        // Update max size and available count
        size_t additional_capacity = new_max_size - max_size_;
        max_size_ = new_max_size;
        available_ += additional_capacity;

        // Reserve more space if growing
        if (additional_capacity > 0) {
            pool_.reserve(new_max_size);
            if (config_.enable_auto_cleanup) {
                idle_objects_.reserve(new_max_size);
            }
        }

        cv_.notify_all();
    }

    /**
     * @brief Applies a function to all objects in the pool.
     *
     * @param func The function to apply to each object.
     */
    void applyToAll(const std::function<void(T&)>& func) {
        std::unique_lock lock(mutex_);
        for (auto& objPtr : pool_) {
            func(*objPtr);
        }
    }

    /**
     * @brief Runs cleanup of idle objects manually.
     *
     * @param force If true, runs cleanup regardless of the elapsed time since
     * last cleanup.
     * @return Number of objects cleaned up.
     */
    size_t runCleanup(bool force = false) {
        std::unique_lock lock(mutex_);
        return runCleanupLocked(force);
    }

    /**
     * @brief Gets the current statistics for the object pool.
     *
     * @return A copy of the current statistics structure.
     */
    [[nodiscard]] PoolStats getStats() const {
        if (!config_.enable_stats) {
            return PoolStats{};
        }

        std::shared_lock lock(mutex_);
        return stats_;
    }

    /**
     * @brief Resets the statistics counters.
     */
    void resetStats() {
        if (!config_.enable_stats) {
            return;
        }

        std::unique_lock lock(mutex_);
        stats_ = PoolStats{};
    }

    /**
     * @brief Updates the pool configuration.
     *
     * @param config The new configuration to apply.
     */
    void reconfigure(const PoolConfig& config) {
        std::unique_lock lock(mutex_);
        config_ = config;
    }

    /**
     * @brief Get detailed performance metrics
     *
     * @return Tuple containing (hit_ratio, avg_wait_time, avg_acquisition_time, memory_reuse_ratio)
     */
    [[nodiscard]] auto getPerformanceMetrics() const -> std::tuple<double, double, double, double> {
        std::shared_lock lock(mutex_);
        return std::make_tuple(
            stats_.getHitRatio(),
            stats_.getAverageWaitTime(),
            stats_.getAverageAcquisitionTime(),
            stats_.getMemoryReuseRatio()
        );
    }

    /**
     * @brief Get lock contention statistics
     *
     * @return Tuple containing (contentions, total_lock_wait_time, avg_lock_wait_time)
     */
    [[nodiscard]] auto getLockContentionStats() const -> std::tuple<size_t, uint64_t, double> {
        std::shared_lock lock(mutex_);
        size_t contentions = stats_.lock_contentions.load();
        uint64_t total_wait = stats_.total_lock_wait_time.load();
        double avg_wait = contentions > 0 ? static_cast<double>(total_wait) / contentions : 0.0;
        return std::make_tuple(contentions, total_wait, avg_wait);
    }

    /**
     * @brief Get memory efficiency statistics
     *
     * @return Tuple containing (memory_reuses, memory_allocations, reuse_ratio)
     */
    [[nodiscard]] auto getMemoryEfficiencyStats() const -> std::tuple<size_t, size_t, double> {
        std::shared_lock lock(mutex_);
        size_t reuses = stats_.memory_reuses.load();
        size_t allocations = stats_.memory_allocations.load();
        double ratio = stats_.getMemoryReuseRatio();
        return std::make_tuple(reuses, allocations, ratio);
    }

    /**
     * @brief Get fast path statistics
     *
     * @return Number of fast path acquisitions
     */
    [[nodiscard]] size_t getFastPathAcquisitions() const noexcept {
        return fast_path_acquisitions_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Manually trigger object warming
     *
     * @param count Number of objects to pre-warm
     */
    void triggerObjectWarming(size_t count) {
        std::unique_lock lock(mutex_);
        warmObjects(count);
    }

    /**
     * @brief Manually trigger adaptive sizing
     */
    void triggerAdaptiveSizing() {
        std::unique_lock lock(mutex_);
        performAdaptiveSizing();
    }

    /**
     * @brief Get current pool utilization
     *
     * @return Tuple containing (current_usage, max_size, utilization_ratio)
     */
    [[nodiscard]] auto getUtilization() const -> std::tuple<size_t, size_t, double> {
        std::shared_lock lock(mutex_);
        size_t current_usage = max_size_ - available_;
        double utilization = static_cast<double>(current_usage) / max_size_;
        return std::make_tuple(current_usage, max_size_, utilization);
    }

private:
    /**
     * @brief Acquires an object from the pool without waiting (assumes lock is
     * held)
     * @param lock The unique lock that is already held
     * @return A shared pointer to the acquired object
     */
    std::shared_ptr<T> acquireImpl(std::unique_lock<std::shared_mutex>& lock) {
        std::shared_ptr<T> obj;

#ifdef ATOM_USE_BOOST
        T* raw_ptr = boost_pool_.construct();
        if (!raw_ptr) {
            THROW_RUNTIME_ERROR("Boost pool allocation failed");
        }
        obj = std::shared_ptr<T>(raw_ptr, [this](T* ptr) {
            boost_pool_.destroy(ptr);
            std::unique_lock<std::shared_mutex> lock(mutex_);
            ++available_;
            cv_.notify_one();
        });
#else
        if (!pool_.empty()) {
            obj = std::move(pool_.back());
            pool_.pop_back();
            if (config_.enable_stats) {
                stats_.hits.fetch_add(1, std::memory_order_relaxed);
                stats_.memory_reuses.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            --available_;
            obj = creator_();
            ++recent_miss_count_;  // Track for adaptive sizing
            if (config_.enable_stats) {
                stats_.misses.fetch_add(1, std::memory_order_relaxed);
                stats_.memory_allocations.fetch_add(1, std::memory_order_relaxed);
                size_t current_usage = max_size_ - available_;
                size_t current_peak = stats_.peak_usage.load();
                while (current_usage > current_peak &&
                       !stats_.peak_usage.compare_exchange_weak(current_peak, current_usage)) {
                    // Keep trying until we successfully update or find a larger value
                }
            }
        }
        obj = wrapWithDeleter(std::move(obj));
#endif

        return obj;
    }

    /**
     * @brief Wraps an object with a custom deleter that returns it to the pool.
     *
     * @param obj The object to wrap.
     * @return A shared pointer with a custom deleter.
     */
    std::shared_ptr<T> wrapWithDeleter(std::shared_ptr<T> obj) {
        // Create a custom deleter to return the object to the pool
        auto deleter = [this, creation_time =
                                  std::chrono::steady_clock::now()](T* ptr) {
            // Create a new shared_ptr that owns the object but won't delete it
            std::shared_ptr<T> sharedObj(ptr, [](T*) {});

            // Validate the object if configured
            bool is_valid = !config_.validate_on_release ||
                            !config_.validator || config_.validator(*ptr);

            std::unique_lock lock(mutex_);

            if (is_valid && pool_.size() < max_size_) {
                // Reset the object to a clean state
                sharedObj->reset();

                // Track idle time if auto-cleanup is enabled
                if (config_.enable_auto_cleanup) {
                    idle_objects_.emplace_back(
                        sharedObj, std::chrono::steady_clock::now());
                }

                // Return to the pool
                pool_.push_back(std::move(sharedObj));
            } else {
                // If invalid or pool is full, just discard and increment
                // available count
                ++available_;
            }

            // Notify waiters that an object is available
            cv_.notify_one();
        };

        // Return a shared_ptr with the custom deleter
        return std::shared_ptr<T>(obj.get(), deleter);
    }

    /**
     * @brief Runs cleanup of idle objects (assumes lock is held).
     *
     * @param force If true, runs cleanup regardless of the elapsed time since
     * last cleanup.
     * @return Number of objects cleaned up.
     */
    size_t runCleanupLocked(bool force = false) {
        if (!config_.enable_auto_cleanup) {
            return 0;
        }

        auto now = std::chrono::steady_clock::now();
        if (!force && (now - last_cleanup_ < config_.cleanup_interval)) {
            return 0;
        }

        last_cleanup_ = now;

        // Find objects that have been idle too long
        auto it =
            std::remove_if(idle_objects_.begin(), idle_objects_.end(),
                           [this, now](const auto& item) {
                               return now - item.second > config_.max_idle_time;
                           });

        // Calculate how many objects will be removed
        size_t removed = std::distance(it, idle_objects_.end());

        // Remove references from both idle tracking and the main pool
        if (removed > 0) {
            // First create a set of pointers to remove
            std::unordered_map<T*, bool> to_remove;
            for (auto iter = it; iter != idle_objects_.end(); ++iter) {
                to_remove[iter->first.get()] = true;
            }

            // Remove from the main pool
            auto pool_it = std::remove_if(
                pool_.begin(), pool_.end(), [&to_remove](const auto& obj) {
                    return to_remove.count(obj.get()) > 0;
                });
            pool_.erase(pool_it, pool_.end());

            // Remove from idle tracking
            idle_objects_.erase(it, idle_objects_.end());

            // Update available count
            available_ += removed;

            if (config_.enable_stats) {
                stats_.cleanups += removed;
            }
        }

        return removed;
    }

    /**
     * @brief Checks if auto-cleanup should run and does so if needed.
     */
    void tryCleanupLocked() {
        if (config_.enable_auto_cleanup) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_cleanup_ >= config_.cleanup_interval) {
                runCleanupLocked();
            }
        }
    }

    // Core pool data
    size_t max_size_;
    size_t available_;
    mutable std::shared_mutex mutex_;  // Shared mutex for better read concurrency
    std::condition_variable_any cv_;
    std::vector<std::shared_ptr<T>> pool_;
    std::vector<std::pair<std::shared_ptr<T>, std::chrono::steady_clock::time_point>> idle_objects_;
    CreateFunc creator_;

    // Performance optimization data
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> fast_path_acquisitions_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<void*> last_acquired_object_{nullptr};
    std::vector<std::shared_ptr<T>> warm_objects_;  ///< Pre-warmed objects for fast allocation

    // Adaptive sizing data
    std::chrono::steady_clock::time_point last_resize_time_;
    size_t recent_acquisition_count_{0};
    size_t recent_miss_count_{0};

    // Priority handling
    std::vector<Priority> waiting_priorities_;

    // Configuration
    PoolConfig config_;

    // Statistics and cleanup tracking
    PoolStats stats_;
    std::chrono::steady_clock::time_point last_cleanup_;

#ifdef ATOM_USE_BOOST
    boost::object_pool<T> boost_pool_;
#endif

    /**
     * @brief Prefetch memory for better cache performance
     */
    void prefetchObject(const std::shared_ptr<T>& obj) const noexcept {
        if (config_.enable_prefetching && obj) {
            _mm_prefetch(reinterpret_cast<const char*>(obj.get()), _MM_HINT_T0);
        }
    }

    /**
     * @brief Update timing statistics with lock-free optimization
     */
    void updateTimingStats(uint64_t duration, std::atomic<uint64_t>& total,
                          std::atomic<uint64_t>& max_time) noexcept {
        if (config_.enable_lock_free_stats) {
            total.fetch_add(duration, std::memory_order_relaxed);
            uint64_t current_max = max_time.load(std::memory_order_relaxed);
            while (duration > current_max &&
                   !max_time.compare_exchange_weak(current_max, duration,
                                                  std::memory_order_relaxed)) {
                // Keep trying until we successfully update or find a larger value
            }
        }
    }

    /**
     * @brief Try to get a pre-warmed object for faster allocation
     */
    std::shared_ptr<T> tryGetWarmedObject() {
        if (!warm_objects_.empty()) {
            auto obj = std::move(warm_objects_.back());
            warm_objects_.pop_back();
            return obj;
        }
        return nullptr;
    }

    /**
     * @brief Pre-warm objects for faster allocation
     */
    void warmObjects(size_t count) {
        if (!config_.enable_object_warming || count == 0) return;

        warm_objects_.reserve(warm_objects_.size() + count);
        for (size_t i = 0; i < count && available_ > 0; ++i) {
            try {
                auto obj = creator_();
                if (config_.object_initializer) {
                    config_.object_initializer(*obj);
                }
                warm_objects_.push_back(std::move(obj));
                --available_;
            } catch (...) {
                // Ignore warming failures
                break;
            }
        }
    }

    /**
     * @brief Perform adaptive pool sizing based on recent usage patterns
     */
    void performAdaptiveSizing() {
        if (!config_.enable_adaptive_sizing) return;

        auto now = std::chrono::steady_clock::now();
        auto time_since_last_resize = now - last_resize_time_;

        // Only resize every few minutes to avoid thrashing
        if (time_since_last_resize < std::chrono::minutes(5)) return;

        double miss_ratio = recent_acquisition_count_ > 0 ?
            static_cast<double>(recent_miss_count_) / recent_acquisition_count_ : 0.0;

        // If miss ratio is high, consider growing the pool
        if (miss_ratio > 0.3 && available_ < max_size_ / 4) {
            size_t growth_amount = std::min(config_.max_pool_growth,
                                          static_cast<size_t>(available_ * config_.growth_factor));
            available_ += growth_amount;

            // Pre-warm some objects if enabled
            if (config_.enable_object_warming) {
                warmObjects(growth_amount / 2);
            }
        }
        // If miss ratio is very low, consider shrinking
        else if (miss_ratio < 0.05 && pool_.size() > max_size_ * config_.shrink_threshold / 100) {
            size_t shrink_amount = pool_.size() / 4;
            for (size_t i = 0; i < shrink_amount && !pool_.empty(); ++i) {
                pool_.pop_back();
                ++available_;
            }
        }

        last_resize_time_ = now;
        recent_acquisition_count_ = 0;
        recent_miss_count_ = 0;
    }
};

}  // namespace atom::memory

#endif  // ATOM_MEMORY_OBJECT_POOL_HPP
