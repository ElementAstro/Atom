#ifndef ATOM_ALGORITHM_RING_HPP
#define ATOM_ALGORITHM_RING_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <vector>
#include <immintrin.h>  // For memory prefetching

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifdef ATOM_USE_BOOST
#include <boost/circular_buffer.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#endif

namespace atom::memory {

/**
 * @brief Performance statistics for RingBuffer
 */
struct alignas(CACHE_LINE_SIZE) RingBufferStats {
    std::atomic<size_t> push_operations{0};      ///< Total push operations
    std::atomic<size_t> pop_operations{0};       ///< Total pop operations
    std::atomic<size_t> push_failures{0};        ///< Failed push operations (buffer full)
    std::atomic<size_t> pop_failures{0};         ///< Failed pop operations (buffer empty)
    std::atomic<size_t> overwrite_operations{0}; ///< Overwrite operations
    std::atomic<size_t> lock_contentions{0};     ///< Lock contention events
    std::atomic<uint64_t> total_push_time{0};    ///< Total push time (ns)
    std::atomic<uint64_t> total_pop_time{0};     ///< Total pop time (ns)
    std::atomic<uint64_t> max_push_time{0};      ///< Maximum push time (ns)
    std::atomic<uint64_t> max_pop_time{0};       ///< Maximum pop time (ns)
    std::atomic<size_t> cache_hits{0};           ///< Cache-friendly operations
    std::atomic<size_t> cache_misses{0};         ///< Cache-unfriendly operations

    void reset() noexcept {
        push_operations = 0; pop_operations = 0; push_failures = 0;
        pop_failures = 0; overwrite_operations = 0; lock_contentions = 0;
        total_push_time = 0; total_pop_time = 0; max_push_time = 0;
        max_pop_time = 0; cache_hits = 0; cache_misses = 0;
    }

    double getPushSuccessRatio() const noexcept {
        size_t total = push_operations.load() + push_failures.load();
        return total > 0 ? static_cast<double>(push_operations.load()) / total : 0.0;
    }

    double getPopSuccessRatio() const noexcept {
        size_t total = pop_operations.load() + pop_failures.load();
        return total > 0 ? static_cast<double>(pop_operations.load()) / total : 0.0;
    }

    double getAveragePushTime() const noexcept {
        size_t count = push_operations.load();
        return count > 0 ? static_cast<double>(total_push_time.load()) / count : 0.0;
    }

    double getAveragePopTime() const noexcept {
        size_t count = pop_operations.load();
        return count > 0 ? static_cast<double>(total_pop_time.load()) / count : 0.0;
    }

    double getCacheHitRatio() const noexcept {
        size_t total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
    }

    // Create a copyable snapshot of the statistics
    void snapshot(RingBufferStats& copy) const noexcept {
        copy.push_operations.store(push_operations.load());
        copy.pop_operations.store(pop_operations.load());
        copy.push_failures.store(push_failures.load());
        copy.pop_failures.store(pop_failures.load());
        copy.overwrite_operations.store(overwrite_operations.load());
        copy.lock_contentions.store(lock_contentions.load());
        copy.total_push_time.store(total_push_time.load());
        copy.total_pop_time.store(total_pop_time.load());
        copy.max_push_time.store(max_push_time.load());
        copy.max_pop_time.store(max_pop_time.load());
        copy.cache_hits.store(cache_hits.load());
        copy.cache_misses.store(cache_misses.load());
    }
};

/**
 * @brief Configuration for RingBuffer optimizations
 */
struct RingBufferConfig {
    bool enable_stats{true};           ///< Enable performance statistics
    bool enable_prefetching{true};     ///< Enable memory prefetching
    bool enable_lock_free_reads{false}; ///< Enable lock-free read operations
    bool enable_batch_operations{true}; ///< Enable batch operation optimizations
    size_t prefetch_distance{1};       ///< Number of elements to prefetch ahead
    size_t contention_threshold{100};   ///< Lock contention threshold for optimization
};

/**
 * @brief Enhanced thread-safe circular buffer implementation with performance optimizations.
 *
 * Features:
 * - Lock-free read operations (optional)
 * - Memory prefetching for better cache performance
 * - Comprehensive performance statistics
 * - Batch operations for improved throughput
 * - Cache-aligned data structures
 *
 * @tparam T The type of elements stored in the buffer.
 */
template <typename T>
class RingBuffer {
public:
    /**
     * @brief Construct a new RingBuffer object with enhanced configuration.
     *
     * @param size The maximum size of the buffer.
     * @param config Configuration options for performance optimizations.
     * @throw std::invalid_argument if size is zero.
     */
    explicit RingBuffer(size_t size, const RingBufferConfig& config = RingBufferConfig{})
        : config_(config) {
        if (size == 0) {
            throw std::invalid_argument(
                "RingBuffer size must be greater than zero.");
        }
#ifdef ATOM_USE_BOOST
        buffer_ = boost::circular_buffer<T>(size);
#else
        buffer_.reserve(size);
        buffer_.resize(size);
#endif
        max_size_ = size;

        // Initialize lock-free indices if enabled
        if (config_.enable_lock_free_reads) {
            atomic_head_.store(0, std::memory_order_relaxed);
            atomic_tail_.store(0, std::memory_order_relaxed);
            atomic_count_.store(0, std::memory_order_relaxed);
        }
    }

    // Deleted copy constructor and assignment operator to prevent copying of
    // mutex
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Move constructor and assignment operator
    RingBuffer(RingBuffer&& other) noexcept
#ifdef ATOM_USE_BOOST
        : buffer_(std::move(other.buffer_))
#else
        : buffer_(std::move(other.buffer_)),
          max_size_(other.max_size_),
          head_(other.head_),
          tail_(other.tail_),
          count_(other.count_)
#endif
    {
        // Reset other's state to a valid, empty state
#ifndef ATOM_USE_BOOST
        other.max_size_ = 0;
        other.head_ = 0;
        other.tail_ = 0;
        other.count_ = 0;
#endif
    }

    RingBuffer& operator=(RingBuffer&& other) noexcept {
        if (this != &other) {
            std::lock(mutex_, other.mutex_);  // Lock both mutexes
            std::lock_guard<MutexType> self_lock(mutex_, std::adopt_lock);
            std::lock_guard<MutexType> other_lock(other.mutex_,
                                                  std::adopt_lock);

#ifdef ATOM_USE_BOOST
            buffer_ = std::move(other.buffer_);
#else
            buffer_ = std::move(other.buffer_);
            max_size_ = other.max_size_;
            head_ = other.head_;
            tail_ = other.tail_;
            count_ = other.count_;

            // Reset other's state
            other.max_size_ = 0;
            other.head_ = 0;
            other.tail_ = 0;
            other.count_ = 0;
#endif
        }
        return *this;
    }

    /**
     * @brief Push an item to the buffer with performance optimizations.
     *
     * @param item The item to push.
     * @return true if the item was successfully pushed, false if the buffer was
     * full.
     * @throw std::runtime_error if pushing fails due to internal reasons.
     */
    auto push(const T& item) -> bool {
        auto start_time = config_.enable_stats ?
            std::chrono::high_resolution_clock::now() :
            std::chrono::high_resolution_clock::time_point{};

        std::lock_guard lock(mutex_);

        bool success = false;

#ifdef ATOM_USE_BOOST
        if (buffer_.full()) {
            if (config_.enable_stats) {
                stats_.push_failures.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }
        buffer_.push_back(item);
        success = true;

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads) {
            atomic_head_.store(buffer_.size(), std::memory_order_release);
            atomic_count_.store(buffer_.size(), std::memory_order_release);
        }
#else
        if (count_ == max_size_) {  // Use direct check to avoid deadlock
            if (config_.enable_stats) {
                stats_.push_failures.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        // Prefetch the target location for better cache performance
        prefetchElement(head_);

        buffer_[head_] = item;  // Use copy assignment
        head_ = (head_ + 1) % max_size_;
        ++count_;
        success = true;

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads) {
            atomic_head_.store(head_, std::memory_order_release);
            atomic_count_.store(count_, std::memory_order_release);
        }
#endif

        if (config_.enable_stats && success) {
            stats_.push_operations.fetch_add(1, std::memory_order_relaxed);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time).count();
            updateTimingStats(duration, true);

            // Track cache performance
            void* current_element = &buffer_[head_ > 0 ? head_ - 1 : max_size_ - 1];
            void* last_accessed = last_accessed_element_.load(std::memory_order_relaxed);
            if (last_accessed &&
                std::abs(static_cast<char*>(current_element) - static_cast<char*>(last_accessed)) <= CACHE_LINE_SIZE) {
                stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);
            } else {
                stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
            }
            last_accessed_element_.store(current_element, std::memory_order_relaxed);
        }

        return success;
    }

    /**
     * @brief Push an item to the buffer using move semantics.
     *
     * @param item The item to push (rvalue reference).
     * @return true if the item was successfully pushed, false if the buffer was
     * full.
     */
    auto push(T&& item) -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.full()) {
            return false;
        }
        buffer_.push_back(std::move(item));

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads) {
            atomic_head_.store(buffer_.size(), std::memory_order_release);
            atomic_count_.store(buffer_.size(), std::memory_order_release);
        }
#else
        if (count_ == max_size_) {  // Use direct check to avoid deadlock
            return false;
        }
        buffer_[head_] = std::move(item);  // Use move assignment
        head_ = (head_ + 1) % max_size_;
        ++count_;

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads) {
            atomic_head_.store(head_, std::memory_order_release);
            atomic_count_.store(count_, std::memory_order_release);
        }
#endif
        return true;
    }

    /**
     * @brief Push an item to the buffer, overwriting the oldest item if full.
     *
     * @param item The item to push.
     */
    void pushOverwrite(const T& item) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.push_back(item);
#else
        buffer_[head_] = item;
        if (count_ == max_size_) {  // Use direct check to avoid deadlock
            tail_ = (tail_ + 1) % max_size_;
        } else {
            ++count_;
        }
        head_ = (head_ + 1) % max_size_;
#endif
    }

    /**
     * @brief Push an item to the buffer, overwriting the oldest item if full,
     * using move semantics.
     *
     * @param item The item to push (rvalue reference).
     */
    void pushOverwrite(T&& item) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.push_back(std::move(item));
#else
        buffer_[head_] = std::move(item);
        if (count_ == max_size_) {  // Use direct check to avoid deadlock
            tail_ = (tail_ + 1) % max_size_;
        } else {
            ++count_;
        }
        head_ = (head_ + 1) % max_size_;
#endif
    }

    /**
     * @brief Pop an item from the buffer with performance optimizations.
     *
     * @return std::optional<T> The popped item, or std::nullopt if the buffer
     * was empty.
     */
    auto pop() -> std::optional<T> {
        auto start_time = config_.enable_stats ?
            std::chrono::high_resolution_clock::now() :
            std::chrono::high_resolution_clock::time_point{};

        std::lock_guard lock(mutex_);

        std::optional<T> result;

#ifdef ATOM_USE_BOOST
        if (buffer_.empty()) {
            if (config_.enable_stats) {
                stats_.pop_failures.fetch_add(1, std::memory_order_relaxed);
            }
            return std::nullopt;
        }
        T item = buffer_.front();
        buffer_.pop_front();
        result = std::move(item);

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads) {
            atomic_tail_.store(buffer_.size(), std::memory_order_release);
            atomic_count_.store(buffer_.size(), std::memory_order_release);
        }
#else
        if (count_ == 0) {  // Use direct check to avoid deadlock
            if (config_.enable_stats) {
                stats_.pop_failures.fetch_add(1, std::memory_order_relaxed);
            }
            return std::nullopt;
        }

        // Prefetch the element we're about to access
        prefetchElement(tail_);

        T item = std::move(buffer_[tail_]);
        tail_ = (tail_ + 1) % max_size_;
        --count_;
        result = std::move(item);

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads) {
            atomic_tail_.store(tail_, std::memory_order_release);
            atomic_count_.store(count_, std::memory_order_release);
        }
#endif

        if (config_.enable_stats && result.has_value()) {
            stats_.pop_operations.fetch_add(1, std::memory_order_relaxed);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time).count();
            updateTimingStats(duration, false);

            // Track cache performance
            void* current_element = &buffer_[tail_ > 0 ? tail_ - 1 : max_size_ - 1];
            void* last_accessed = last_accessed_element_.load(std::memory_order_relaxed);
            if (last_accessed &&
                std::abs(static_cast<char*>(current_element) - static_cast<char*>(last_accessed)) <= CACHE_LINE_SIZE) {
                stats_.cache_hits.fetch_add(1, std::memory_order_relaxed);
            } else {
                stats_.cache_misses.fetch_add(1, std::memory_order_relaxed);
            }
            last_accessed_element_.store(current_element, std::memory_order_relaxed);
        }

        return result;
    }

    /**
     * @brief Check if the buffer is full.
     *
     * @return true if the buffer is full, false otherwise.
     */
    auto full() const -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return buffer_.full();
#else
        return count_ == max_size_;
#endif
    }

    /**
     * @brief Check if the buffer is empty.
     *
     * @return true if the buffer is empty, false otherwise.
     */
    auto empty() const -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return buffer_.empty();
#else
        return count_ == 0;
#endif
    }

    /**
     * @brief Get the current number of items in the buffer.
     *
     * @return size_t The number of items in the buffer.
     */
    auto size() const -> size_t {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return buffer_.size();
#else
        return count_;
#endif
    }

    /**
     * @brief Get the maximum size of the buffer.
     *
     * @return size_t The maximum size of the buffer.
     */
    auto capacity() const -> size_t { return max_size_; }

    /**
     * @brief Get performance statistics
     *
     * @param stats Reference to statistics structure to fill
     */
    void getStats(RingBufferStats& stats) const {
        std::lock_guard lock(mutex_);
        stats_.snapshot(stats);
    }

    /**
     * @brief Reset performance statistics
     */
    void resetStats() {
        std::lock_guard lock(mutex_);
        stats_.reset();
    }

    /**
     * @brief Get performance metrics
     *
     * @return Tuple of (push_success_ratio, pop_success_ratio, avg_push_time, avg_pop_time, cache_hit_ratio)
     */
    [[nodiscard]] auto getPerformanceMetrics() const -> std::tuple<double, double, double, double, double> {
        std::lock_guard lock(mutex_);
        return std::make_tuple(
            stats_.getPushSuccessRatio(),
            stats_.getPopSuccessRatio(),
            stats_.getAveragePushTime(),
            stats_.getAveragePopTime(),
            stats_.getCacheHitRatio()
        );
    }

    /**
     * @brief Batch push operation for improved throughput
     *
     * @param items Vector of items to push
     * @return Number of items successfully pushed
     */
    template<typename Container>
    size_t pushBatch(const Container& items) {
        if (!config_.enable_batch_operations) {
            // Fall back to individual pushes
            size_t count = 0;
            for (const auto& item : items) {
                if (push(item)) {
                    ++count;
                } else {
                    break;  // Stop on first failure
                }
            }
            return count;
        }

        auto start_time = config_.enable_stats ?
            std::chrono::high_resolution_clock::now() :
            std::chrono::high_resolution_clock::time_point{};

        std::lock_guard lock(mutex_);

        size_t pushed = 0;
        for (const auto& item : items) {
#ifdef ATOM_USE_BOOST
            if (buffer_.full()) {
                break;
            }
            buffer_.push_back(item);
#else
            if (count_ == max_size_) {  // Use direct check to avoid deadlock
                break;
            }
            prefetchElement(head_);
            buffer_[head_] = item;
            head_ = (head_ + 1) % max_size_;
            ++count_;
#endif
            ++pushed;
        }

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads && pushed > 0) {
            atomic_head_.store(head_, std::memory_order_release);
            atomic_count_.store(count_, std::memory_order_release);
        }

        if (config_.enable_stats && pushed > 0) {
            stats_.push_operations.fetch_add(pushed, std::memory_order_relaxed);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time).count();
            updateTimingStats(duration / pushed, true);  // Average time per item
        }

        return pushed;
    }

    /**
     * @brief Batch pop operation for improved throughput
     *
     * @param max_items Maximum number of items to pop
     * @return Vector of popped items
     */
    std::vector<T> popBatch(size_t max_items) {
        std::vector<T> result;

        if (!config_.enable_batch_operations) {
            // Fall back to individual pops
            result.reserve(max_items);
            for (size_t i = 0; i < max_items; ++i) {
                auto item = pop();
                if (item.has_value()) {
                    result.push_back(std::move(item.value()));
                } else {
                    break;
                }
            }
            return result;
        }

        auto start_time = config_.enable_stats ?
            std::chrono::high_resolution_clock::now() :
            std::chrono::high_resolution_clock::time_point{};

        std::lock_guard lock(mutex_);

        size_t current_size = count_;  // Use direct access to avoid deadlock
        size_t to_pop = std::min(max_items, current_size);
        result.reserve(to_pop);

        for (size_t i = 0; i < to_pop; ++i) {
#ifdef ATOM_USE_BOOST
            if (buffer_.empty()) {
                break;
            }
            result.push_back(buffer_.front());
            buffer_.pop_front();
#else
            if (count_ == 0) {  // Use direct check to avoid deadlock
                break;
            }
            prefetchElement(tail_);
            result.push_back(std::move(buffer_[tail_]));
            tail_ = (tail_ + 1) % max_size_;
            --count_;
#endif
        }

        // Update atomic indices for lock-free reads if enabled
        if (config_.enable_lock_free_reads && !result.empty()) {
            atomic_tail_.store(tail_, std::memory_order_release);
            atomic_count_.store(count_, std::memory_order_release);
        }

        if (config_.enable_stats && !result.empty()) {
            stats_.pop_operations.fetch_add(result.size(), std::memory_order_relaxed);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time).count();
            updateTimingStats(duration / result.size(), false);  // Average time per item
        }

        return result;
    }

    /**
     * @brief Lock-free size check (if enabled)
     *
     * @return Current size of the buffer
     */
    [[nodiscard]] size_t sizeLockFree() const noexcept {
        if (config_.enable_lock_free_reads) {
            return atomic_count_.load(std::memory_order_acquire);
        } else {
            return size();  // Fall back to locked version
        }
    }

    /**
     * @brief Lock-free empty check (if enabled)
     *
     * @return True if buffer is empty
     */
    [[nodiscard]] bool emptyLockFree() const noexcept {
        if (config_.enable_lock_free_reads) {
            return atomic_count_.load(std::memory_order_acquire) == 0;
        } else {
            return empty();  // Fall back to locked version
        }
    }

    /**
     * @brief Lock-free full check (if enabled)
     *
     * @return True if buffer is full
     */
    [[nodiscard]] bool fullLockFree() const noexcept {
        if (config_.enable_lock_free_reads) {
            return atomic_count_.load(std::memory_order_acquire) == max_size_;
        } else {
            return full();  // Fall back to locked version
        }
    }

    /**
     * @brief Get current configuration
     *
     * @return Current configuration settings
     */
    [[nodiscard]] const RingBufferConfig& getConfig() const noexcept {
        return config_;
    }

    /**
     * @brief Update configuration (requires lock)
     *
     * @param new_config New configuration to apply
     */
    void updateConfig(const RingBufferConfig& new_config) {
        std::lock_guard lock(mutex_);
        config_ = new_config;

        // If lock-free reads are being enabled, sync atomic indices
        if (new_config.enable_lock_free_reads && !config_.enable_lock_free_reads) {
            atomic_head_.store(head_, std::memory_order_relaxed);
            atomic_tail_.store(tail_, std::memory_order_relaxed);
            atomic_count_.store(count_, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Get utilization ratio
     *
     * @return Ratio of current size to capacity (0.0 to 1.0)
     */
    [[nodiscard]] double getUtilization() const {
        std::lock_guard lock(mutex_);
        return static_cast<double>(count_) / max_size_;
    }

    /**
     * @brief Clear all items from the buffer.
     */
    void clear() {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.clear();
#else
        // For types that manage resources (like unique_ptr), we need to
        // explicitly destroy the elements to release resources.
        // For POD types, this loop is effectively a no-op.
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            buffer_[index].~T();  // Explicitly call destructor
        }
        head_ = 0;
        tail_ = 0;
        count_ = 0;
#endif
    }

    /**
     * @brief Get the front item of the buffer without removing it.
     *
     * @return std::optional<T> The front item, or std::nullopt if the buffer is
     * empty.
     */
    auto front() const -> std::optional<T> {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.empty()) {
            return std::nullopt;
        }
        return buffer_.front();
#else
        if (count_ == 0) {  // Use direct check to avoid deadlock
            return std::nullopt;
        }
        // Return a copy, as the internal element might be moved out by pop()
        return buffer_[tail_];
#endif
    }

    /**
     * @brief Get the back item of the buffer without removing it.
     *
     * @return std::optional<T> The back item, or std::nullopt if the buffer is
     * empty.
     */
    auto back() const -> std::optional<T> {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.empty()) {
            return std::nullopt;
        }
        return buffer_.back();
#else
        if (count_ == 0) {  // Use direct check to avoid deadlock
            return std::nullopt;
        }
        size_t backIndex = (head_ + max_size_ - 1) % max_size_;
        // Return a copy
        return buffer_[backIndex];
#endif
    }

    /**
     * @brief Check if the buffer contains a specific item.
     *
     * @param item The item to search for.
     * @return true if the item is in the buffer, false otherwise.
     */
    auto contains(const T& item) const -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return std::find(buffer_.begin(), buffer_.end(), item) != buffer_.end();
#else
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            if (buffer_[index] == item) {
                return true;
            }
        }
        return false;
#endif
    }

    /**
     * @brief Get a view of the buffer's contents as a vector.
     *
     * @return std::vector<T> A vector containing the buffer's contents in
     * order.
     */
    std::vector<T> view() const {
        std::lock_guard lock(mutex_);
        std::vector<T> combined;
        combined.reserve(count_);  // Use direct access to avoid deadlock
#ifdef ATOM_USE_BOOST
        std::copy(buffer_.begin(), buffer_.end(), std::back_inserter(combined));
#else
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            // This will attempt to copy. For move-only types, this will fail.
            // A better approach for move-only types would be to return a vector
            // of references or iterators. For now, assuming T is
            // CopyConstructible for view().
            combined.emplace_back(buffer_[index]);
        }
#endif
        return combined;
    }

    /**
     * @brief Iterator class for RingBuffer.
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        Iterator(const RingBuffer<T>* buffer, size_t pos, size_t traversed)
            : buffer_(buffer), pos_(pos), traversed_(traversed) {}

        auto operator*() const -> reference { return buffer_->buffer_[pos_]; }

        auto operator->() const -> pointer { return &buffer_->buffer_[pos_]; }

        auto operator++() -> Iterator& {
            pos_ = (pos_ + 1) % buffer_->max_size_;
            ++traversed_;
            return *this;
        }

        auto operator++(int) const -> Iterator {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend auto operator==(const Iterator& a, const Iterator& b) -> bool {
            return a.traversed_ == b.traversed_;
        }

        friend auto operator!=(const Iterator& a, const Iterator& b) -> bool {
            return !(a == b);
        }

    private:
        const RingBuffer<T>* buffer_;
        size_t pos_;
        size_t traversed_;
    };

    /**
     * @brief Get an iterator to the beginning of the buffer.
     *
     * @return Iterator
     */
    auto begin() const -> Iterator {
        std::lock_guard lock(mutex_);
        return Iterator(this, tail_, 0);
    }

    /**
     * @brief Get an iterator to the end of the buffer.
     *
     * @return Iterator
     */
    auto end() const -> Iterator {
        std::lock_guard lock(mutex_);
        return Iterator(this, head_, count_);
    }

    /**
     * @brief Resize the buffer.
     *
     * @param new_size The new size of the buffer.
     * @throw std::runtime_error if new_size is less than the current number of
     * elements.
     */
    void resize(size_t new_size) {
        std::lock_guard lock(mutex_);
        if (new_size < count_) {  // Use direct check to avoid deadlock
            throw std::runtime_error(
                "New size cannot be smaller than current number of elements.");
        }
#ifdef ATOM_USE_BOOST
        buffer_.set_capacity(new_size);
#else
        // Create a new vector and move elements
        std::vector<T> newBuffer;
        newBuffer.reserve(new_size);
        newBuffer.resize(
            new_size);  // Allocate memory and default-construct elements

        for (size_t i = 0; i < count_; ++i) {
            size_t oldIndex = (tail_ + i) % max_size_;
            newBuffer[i] = std::move(buffer_[oldIndex]);
        }
        buffer_ = std::move(newBuffer);
        max_size_ = new_size;
        head_ =
            count_;  // After moving, elements are at the beginning of newBuffer
        tail_ = 0;
#endif
    }

    /**
     * @brief Get an element at a specific index in the buffer.
     *
     * @param index The index of the element to retrieve.
     * @return std::optional<T> The element at the specified index, or
     * std::nullopt if the index is out of bounds.
     */
    auto at(size_t index) const -> std::optional<T> {
        std::lock_guard lock(mutex_);
        if (index >= count_) {  // Use direct check to avoid deadlock
            return std::nullopt;
        }
#ifdef ATOM_USE_BOOST
        return buffer_[index];
#else
        size_t actualIndex = (tail_ + index) % max_size_;
        // Return a copy
        return buffer_[actualIndex];
#endif
    }

    /**
     * @brief Apply a function to each element in the buffer.
     *
     * @param func The function to apply to each element.
     */
    template <std::invocable<T&> F>
    void forEach(F&& func) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        for (auto& item : buffer_) {
            func(item);
        }
#else
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            func(buffer_[index]);
        }
#endif
    }

    /**
     * @brief Remove elements from the buffer that satisfy a predicate.
     *
     * @param pred The predicate function.
     */
    template <std::predicate<T> P>
    void removeIf(P&& pred) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.erase(std::remove_if(buffer_.begin(), buffer_.end(), pred),
                      buffer_.end());
#else
        std::vector<T> temp_buffer;
        temp_buffer.reserve(count_);  // Reserve enough space

        for (size_t i = 0; i < count_; ++i) {
            size_t read_idx = (tail_ + i) % max_size_;
            if (!pred(buffer_[read_idx])) {
                temp_buffer.emplace_back(std::move(buffer_[read_idx]));
            } else {
                // Explicitly destroy the removed element if it manages
                // resources
                buffer_[read_idx].~T();
            }
        }

        // Rebuild the buffer_ from temp_buffer
        count_ = temp_buffer.size();
        head_ = count_;
        tail_ = 0;
        // Ensure buffer_ has enough capacity before moving
        if (max_size_ < count_) {
            max_size_ = count_;  // Should not happen if resize logic is correct
        }
        buffer_ = std::vector<T>();  // Clear and reallocate
        buffer_.reserve(max_size_);
        buffer_.resize(max_size_);

        for (size_t i = 0; i < count_; ++i) {
            buffer_[i] = std::move(temp_buffer[i]);
        }
        head_ = count_;  // head_ points to the next available slot
        tail_ = 0;       // tail_ points to the first element
#endif
    }

    /**
     * @brief Rotate the buffer by a specified number of positions.
     *
     * @param n The number of positions to rotate. Positive values rotate left,
     * negative values rotate right.
     */
    void rotate(int n) {
        std::lock_guard lock(mutex_);
        if (count_ == 0 || n == 0) {  // Use direct check to avoid deadlock
            return;
        }

#ifdef ATOM_USE_BOOST
        buffer_.rotate(n);
#else
        // Normalize n to be within [0, count_)
        long long effectiveN = n % static_cast<long long>(count_);
        if (effectiveN < 0) {
            effectiveN += count_;
        }

        // Create a temporary buffer to hold the rotated elements
        std::vector<T> temp_buffer;
        temp_buffer.reserve(count_);

        // Copy elements starting from the new logical tail
        for (size_t i = 0; i < count_; ++i) {
            size_t current_idx = (tail_ + effectiveN + i) % max_size_;
            temp_buffer.emplace_back(std::move(buffer_[current_idx]));
        }

        // Move elements back to the original buffer_
        // This assumes buffer_ has enough capacity and is properly managed
        // Clear and reallocate buffer_ to ensure contiguous memory and proper
        // state
        buffer_ = std::vector<T>();
        buffer_.reserve(max_size_);
        buffer_.resize(max_size_);

        for (size_t i = 0; i < count_; ++i) {
            buffer_[i] = std::move(temp_buffer[i]);
        }

        // Reset head and tail for the new contiguous layout
        head_ = count_;
        tail_ = 0;
#endif
    }

private:
#ifdef ATOM_USE_BOOST
    using CircularBuffer = boost::circular_buffer<T>;
    CircularBuffer buffer_;
    using MutexType = boost::mutex;
#else
    using MutexType = std::mutex;
    std::vector<T> buffer_;
    size_t max_size_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
#endif

    mutable MutexType mutex_;

    // Performance optimization members
    RingBufferConfig config_;
    mutable RingBufferStats stats_;

    // Lock-free optimization members (only used when enabled)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> atomic_head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> atomic_tail_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> atomic_count_{0};

    // Cache optimization
    mutable std::atomic<void*> last_accessed_element_{nullptr};

    /**
     * @brief Prefetch memory for better cache performance
     */
    void prefetchElement(size_t index) const noexcept {
        if (config_.enable_prefetching && index < buffer_.size()) {
            _mm_prefetch(reinterpret_cast<const char*>(&buffer_[index]), _MM_HINT_T0);

            // Prefetch next elements based on prefetch distance
            for (size_t i = 1; i <= config_.prefetch_distance &&
                 (index + i) < buffer_.size(); ++i) {
                _mm_prefetch(reinterpret_cast<const char*>(&buffer_[index + i]), _MM_HINT_T1);
            }
        }
    }

    /**
     * @brief Update timing statistics
     */
    void updateTimingStats(uint64_t duration, bool is_push) const noexcept {
        if (!config_.enable_stats) return;

        if (is_push) {
            stats_.total_push_time.fetch_add(duration, std::memory_order_relaxed);
            uint64_t current_max = stats_.max_push_time.load(std::memory_order_relaxed);
            while (duration > current_max &&
                   !stats_.max_push_time.compare_exchange_weak(current_max, duration,
                                                              std::memory_order_relaxed)) {
                // Keep trying until we successfully update or find a larger value
            }
        } else {
            stats_.total_pop_time.fetch_add(duration, std::memory_order_relaxed);
            uint64_t current_max = stats_.max_pop_time.load(std::memory_order_relaxed);
            while (duration > current_max &&
                   !stats_.max_pop_time.compare_exchange_weak(current_max, duration,
                                                             std::memory_order_relaxed)) {
                // Keep trying until we successfully update or find a larger value
            }
        }
    }
};

}  // namespace atom::memory

#endif  // ATOM_ALGORITHM_RING_HPP
