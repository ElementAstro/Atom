#ifndef ATOM_EXTRA_BEAST_PERFORMANCE_MONITOR_HPP
#define ATOM_EXTRA_BEAST_PERFORMANCE_MONITOR_HPP

#include "concurrency_primitives.hpp"
#include <atomic>
#include <chrono>
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace atom::beast::monitoring {

/**
 * @brief Lock-free performance counter with minimal overhead
 */
template<typename T>
class LockFreeCounter {
private:
    concurrency::CacheAligned<std::atomic<T>> value_{T{}};
    concurrency::CacheAligned<std::atomic<T>> peak_{T{}};
    concurrency::CacheAligned<std::atomic<std::chrono::steady_clock::time_point>> peak_time_;
    
public:
    LockFreeCounter() : peak_time_(std::chrono::steady_clock::now()) {}
    
    /**
     * @brief Increments the counter atomically
     */
    T increment(T delta = T{1}) noexcept {
        auto new_value = value_.value.fetch_add(delta, std::memory_order_acq_rel) + delta;
        update_peak(new_value);
        return new_value;
    }
    
    /**
     * @brief Decrements the counter atomically
     */
    T decrement(T delta = T{1}) noexcept {
        return value_.value.fetch_sub(delta, std::memory_order_acq_rel) - delta;
    }
    
    /**
     * @brief Sets the counter to a specific value
     */
    void set(T new_value) noexcept {
        value_.value.store(new_value, std::memory_order_release);
        update_peak(new_value);
    }
    
    /**
     * @brief Gets the current value
     */
    [[nodiscard]] T get() const noexcept {
        return value_.value.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Gets the peak value
     */
    [[nodiscard]] T get_peak() const noexcept {
        return peak_.value.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Resets the counter and peak
     */
    void reset() noexcept {
        value_.value.store(T{}, std::memory_order_release);
        peak_.value.store(T{}, std::memory_order_release);
        peak_time_.value.store(std::chrono::steady_clock::now(), std::memory_order_release);
    }
    
private:
    void update_peak(T new_value) noexcept {
        T current_peak = peak_.value.load(std::memory_order_relaxed);
        while (new_value > current_peak) {
            if (peak_.value.compare_exchange_weak(current_peak, new_value,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed)) {
                peak_time_.value.store(std::chrono::steady_clock::now(), std::memory_order_release);
                break;
            }
        }
    }
};

/**
 * @brief High-resolution latency histogram with lock-free updates
 */
class LockFreeLatencyHistogram {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    static constexpr std::size_t MAX_LATENCY_US = 1000000; // 1 second
    
    std::array<LockFreeCounter<std::uint64_t>, BUCKET_COUNT> buckets_;
    LockFreeCounter<std::uint64_t> total_samples_;
    LockFreeCounter<std::uint64_t> total_latency_us_;
    std::atomic<std::uint64_t> min_latency_us_{UINT64_MAX};
    std::atomic<std::uint64_t> max_latency_us_{0};
    
    [[nodiscard]] std::size_t get_bucket_index(std::uint64_t latency_us) const noexcept {
        if (latency_us == 0) return 0;
        if (latency_us >= MAX_LATENCY_US) return BUCKET_COUNT - 1;
        
        // Logarithmic bucketing for better resolution at lower latencies
        auto log_latency = static_cast<std::size_t>(std::log2(latency_us));
        return std::min(log_latency, BUCKET_COUNT - 1);
    }
    
public:
    /**
     * @brief Records a latency sample
     */
    void record_latency(std::chrono::microseconds latency) noexcept {
        auto latency_us = static_cast<std::uint64_t>(latency.count());
        
        // Update histogram
        auto bucket_index = get_bucket_index(latency_us);
        buckets_[bucket_index].increment();
        
        // Update aggregates
        total_samples_.increment();
        total_latency_us_.increment(latency_us);
        
        // Update min/max
        update_min_max(latency_us);
    }
    
    /**
     * @brief Records latency for a timed operation
     */
    template<typename TimePoint>
    void record_latency_since(TimePoint start_time) noexcept {
        auto end_time = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        record_latency(latency);
    }
    
    /**
     * @brief Latency statistics
     */
    struct Statistics {
        std::uint64_t sample_count;
        std::uint64_t min_latency_us;
        std::uint64_t max_latency_us;
        double avg_latency_us;
        std::array<std::uint64_t, BUCKET_COUNT> bucket_counts;
    };
    
    [[nodiscard]] Statistics get_statistics() const noexcept {
        Statistics stats{};
        stats.sample_count = total_samples_.get();
        stats.min_latency_us = min_latency_us_.load(std::memory_order_acquire);
        stats.max_latency_us = max_latency_us_.load(std::memory_order_acquire);
        
        auto total_latency = total_latency_us_.get();
        stats.avg_latency_us = stats.sample_count > 0 ? 
            static_cast<double>(total_latency) / stats.sample_count : 0.0;
        
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            stats.bucket_counts[i] = buckets_[i].get();
        }
        
        return stats;
    }
    
    /**
     * @brief Calculates percentile latency
     */
    [[nodiscard]] std::uint64_t get_percentile(double percentile) const noexcept {
        auto stats = get_statistics();
        if (stats.sample_count == 0) return 0;
        
        auto target_count = static_cast<std::uint64_t>(stats.sample_count * percentile / 100.0);
        std::uint64_t cumulative_count = 0;
        
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            cumulative_count += stats.bucket_counts[i];
            if (cumulative_count >= target_count) {
                // Return the upper bound of this bucket
                return i == 0 ? 1 : (1ULL << i);
            }
        }
        
        return MAX_LATENCY_US;
    }
    
    /**
     * @brief Resets all statistics
     */
    void reset() noexcept {
        for (auto& bucket : buckets_) {
            bucket.reset();
        }
        total_samples_.reset();
        total_latency_us_.reset();
        min_latency_us_.store(UINT64_MAX, std::memory_order_release);
        max_latency_us_.store(0, std::memory_order_release);
    }
    
private:
    void update_min_max(std::uint64_t latency_us) noexcept {
        // Update minimum
        std::uint64_t current_min = min_latency_us_.load(std::memory_order_relaxed);
        while (latency_us < current_min) {
            if (min_latency_us_.compare_exchange_weak(current_min, latency_us,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed)) {
                break;
            }
        }
        
        // Update maximum
        std::uint64_t current_max = max_latency_us_.load(std::memory_order_relaxed);
        while (latency_us > current_max) {
            if (max_latency_us_.compare_exchange_weak(current_max, latency_us,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed)) {
                break;
            }
        }
    }
};

/**
 * @brief Comprehensive performance monitor for HTTP/WebSocket operations
 */
class PerformanceMonitor {
private:
    // HTTP metrics
    LockFreeCounter<std::uint64_t> http_requests_total_;
    LockFreeCounter<std::uint64_t> http_requests_success_;
    LockFreeCounter<std::uint64_t> http_requests_error_;
    LockFreeCounter<std::uint64_t> http_bytes_sent_;
    LockFreeCounter<std::uint64_t> http_bytes_received_;
    LockFreeLatencyHistogram http_latency_;
    
    // WebSocket metrics
    LockFreeCounter<std::uint64_t> ws_connections_total_;
    LockFreeCounter<std::uint64_t> ws_connections_active_;
    LockFreeCounter<std::uint64_t> ws_messages_sent_;
    LockFreeCounter<std::uint64_t> ws_messages_received_;
    LockFreeCounter<std::uint64_t> ws_bytes_sent_;
    LockFreeCounter<std::uint64_t> ws_bytes_received_;
    LockFreeLatencyHistogram ws_latency_;
    
    // Connection pool metrics
    LockFreeCounter<std::uint64_t> pool_connections_created_;
    LockFreeCounter<std::uint64_t> pool_connections_reused_;
    LockFreeCounter<std::uint64_t> pool_connections_active_;
    
    // System metrics
    std::atomic<std::chrono::steady_clock::time_point> start_time_;
    
public:
    PerformanceMonitor() : start_time_(std::chrono::steady_clock::now()) {
        spdlog::info("Performance monitor initialized");
    }
    
    // HTTP metrics
    void record_http_request_start() noexcept {
        http_requests_total_.increment();
    }
    
    void record_http_request_success(std::chrono::steady_clock::time_point start_time,
                                   std::size_t bytes_sent, std::size_t bytes_received) noexcept {
        http_requests_success_.increment();
        http_bytes_sent_.increment(bytes_sent);
        http_bytes_received_.increment(bytes_received);
        http_latency_.record_latency_since(start_time);
    }
    
    void record_http_request_error() noexcept {
        http_requests_error_.increment();
    }
    
    // WebSocket metrics
    void record_ws_connection_opened() noexcept {
        ws_connections_total_.increment();
        ws_connections_active_.increment();
    }
    
    void record_ws_connection_closed() noexcept {
        ws_connections_active_.decrement();
    }
    
    void record_ws_message_sent(std::size_t bytes) noexcept {
        ws_messages_sent_.increment();
        ws_bytes_sent_.increment(bytes);
    }
    
    void record_ws_message_received(std::size_t bytes, 
                                   std::chrono::steady_clock::time_point send_time) noexcept {
        ws_messages_received_.increment();
        ws_bytes_received_.increment(bytes);
        ws_latency_.record_latency_since(send_time);
    }
    
    // Connection pool metrics
    void record_pool_connection_created() noexcept {
        pool_connections_created_.increment();
        pool_connections_active_.increment();
    }
    
    void record_pool_connection_reused() noexcept {
        pool_connections_reused_.increment();
    }
    
    void record_pool_connection_released() noexcept {
        pool_connections_active_.decrement();
    }
    
    /**
     * @brief Comprehensive performance statistics
     */
    struct PerformanceStats {
        // HTTP stats
        std::uint64_t http_requests_total;
        std::uint64_t http_requests_success;
        std::uint64_t http_requests_error;
        double http_success_rate;
        std::uint64_t http_bytes_sent;
        std::uint64_t http_bytes_received;
        LockFreeLatencyHistogram::Statistics http_latency;
        
        // WebSocket stats
        std::uint64_t ws_connections_total;
        std::uint64_t ws_connections_active;
        std::uint64_t ws_messages_sent;
        std::uint64_t ws_messages_received;
        std::uint64_t ws_bytes_sent;
        std::uint64_t ws_bytes_received;
        LockFreeLatencyHistogram::Statistics ws_latency;
        
        // Pool stats
        std::uint64_t pool_connections_created;
        std::uint64_t pool_connections_reused;
        std::uint64_t pool_connections_active;
        double pool_reuse_rate;
        
        // System stats
        std::chrono::seconds uptime;
    };
    
    [[nodiscard]] PerformanceStats get_statistics() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto start = start_time_.load(std::memory_order_acquire);
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        
        auto http_total = http_requests_total_.get();
        auto http_success = http_requests_success_.get();
        auto pool_created = pool_connections_created_.get();
        auto pool_reused = pool_connections_reused_.get();
        
        return PerformanceStats{
            // HTTP
            http_total,
            http_success,
            http_requests_error_.get(),
            http_total > 0 ? static_cast<double>(http_success) / http_total * 100.0 : 0.0,
            http_bytes_sent_.get(),
            http_bytes_received_.get(),
            http_latency_.get_statistics(),
            
            // WebSocket
            ws_connections_total_.get(),
            ws_connections_active_.get(),
            ws_messages_sent_.get(),
            ws_messages_received_.get(),
            ws_bytes_sent_.get(),
            ws_bytes_received_.get(),
            ws_latency_.get_statistics(),
            
            // Pool
            pool_created,
            pool_reused,
            pool_connections_active_.get(),
            (pool_created + pool_reused) > 0 ? 
                static_cast<double>(pool_reused) / (pool_created + pool_reused) * 100.0 : 0.0,
            
            // System
            uptime
        };
    }
    
    /**
     * @brief Logs performance summary
     */
    void log_performance_summary() const {
        auto stats = get_statistics();
        
        spdlog::info("=== Performance Summary ===");
        spdlog::info("Uptime: {}s", stats.uptime.count());
        spdlog::info("HTTP: {} requests ({:.1f}% success), {:.1f}μs avg latency",
                    stats.http_requests_total, stats.http_success_rate, stats.http_latency.avg_latency_us);
        spdlog::info("WebSocket: {} connections, {} messages, {:.1f}μs avg latency",
                    stats.ws_connections_total, stats.ws_messages_sent, stats.ws_latency.avg_latency_us);
        spdlog::info("Pool: {} created, {} reused ({:.1f}% reuse rate)",
                    stats.pool_connections_created, stats.pool_connections_reused, stats.pool_reuse_rate);
    }
    
    /**
     * @brief Resets all statistics
     */
    void reset_statistics() noexcept {
        http_requests_total_.reset();
        http_requests_success_.reset();
        http_requests_error_.reset();
        http_bytes_sent_.reset();
        http_bytes_received_.reset();
        http_latency_.reset();
        
        ws_connections_total_.reset();
        ws_connections_active_.reset();
        ws_messages_sent_.reset();
        ws_messages_received_.reset();
        ws_bytes_sent_.reset();
        ws_bytes_received_.reset();
        ws_latency_.reset();
        
        pool_connections_created_.reset();
        pool_connections_reused_.reset();
        pool_connections_active_.reset();
        
        start_time_.store(std::chrono::steady_clock::now(), std::memory_order_release);
        
        spdlog::info("Performance statistics reset");
    }
};

/**
 * @brief Global performance monitor instance
 */
extern PerformanceMonitor& get_global_performance_monitor();

/**
 * @brief RAII timer for automatic latency measurement
 */
class ScopedTimer {
private:
    std::chrono::steady_clock::time_point start_time_;
    std::function<void(std::chrono::steady_clock::time_point)> completion_callback_;
    
public:
    template<typename Callback>
    explicit ScopedTimer(Callback&& callback) 
        : start_time_(std::chrono::steady_clock::now())
        , completion_callback_(std::forward<Callback>(callback)) {}
    
    ~ScopedTimer() {
        if (completion_callback_) {
            completion_callback_(start_time_);
        }
    }
    
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = default;
    ScopedTimer& operator=(ScopedTimer&&) = default;
};

} // namespace atom::beast::monitoring

#endif // ATOM_EXTRA_BEAST_PERFORMANCE_MONITOR_HPP
