#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <source_location>
#include <format>

namespace atom::extra::pugixml::performance {

/**
 * @brief High-resolution timer for performance measurements
 */
class HighResolutionTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;

public:
    HighResolutionTimer() : start_time_(std::chrono::high_resolution_clock::now()) {}

    void reset() noexcept {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept {
        return std::chrono::high_resolution_clock::now() - start_time_;
    }

    [[nodiscard]] double elapsed_seconds() const noexcept {
        return std::chrono::duration<double>(elapsed()).count();
    }

    [[nodiscard]] double elapsed_milliseconds() const noexcept {
        return std::chrono::duration<double, std::milli>(elapsed()).count();
    }

    [[nodiscard]] double elapsed_microseconds() const noexcept {
        return std::chrono::duration<double, std::micro>(elapsed()).count();
    }
};

/**
 * @brief RAII-based scoped timer for automatic performance measurement
 */
class ScopedTimer {
private:
    HighResolutionTimer timer_;
    std::string operation_name_;
    std::shared_ptr<spdlog::logger> logger_;
    std::source_location location_;

public:
    explicit ScopedTimer(std::string operation_name,
                        std::shared_ptr<spdlog::logger> logger = nullptr,
                        const std::source_location& location = std::source_location::current())
        : operation_name_(std::move(operation_name)), logger_(logger), location_(location) {

        if (logger_) {
            logger_->trace("Starting operation '{}' at {}:{}",
                          operation_name_, location_.file_name(), location_.line());
        }
    }

    ~ScopedTimer() {
        auto duration = timer_.elapsed_microseconds();
        if (logger_) {
            logger_->debug("Operation '{}' completed in {:.3f}μs at {}:{}",
                          operation_name_, duration, location_.file_name(), location_.line());
        }
    }

    [[nodiscard]] double elapsed_microseconds() const noexcept {
        return timer_.elapsed_microseconds();
    }
};

/**
 * @brief Thread-safe performance metrics collector
 */
class MetricsCollector {
private:
    struct MetricData {
        std::atomic<uint64_t> count{0};
        std::atomic<double> total_time{0.0};
        std::atomic<double> min_time{std::numeric_limits<double>::max()};
        std::atomic<double> max_time{0.0};
        std::atomic<uint64_t> error_count{0};

        void update(double time_microseconds) noexcept {
            count.fetch_add(1, std::memory_order_relaxed);
            total_time.fetch_add(time_microseconds, std::memory_order_relaxed);

            // Update min time
            double current_min = min_time.load(std::memory_order_relaxed);
            while (time_microseconds < current_min &&
                   !min_time.compare_exchange_weak(current_min, time_microseconds,
                                                  std::memory_order_relaxed)) {
                // Retry
            }

            // Update max time
            double current_max = max_time.load(std::memory_order_relaxed);
            while (time_microseconds > current_max &&
                   !max_time.compare_exchange_weak(current_max, time_microseconds,
                                                  std::memory_order_relaxed)) {
                // Retry
            }
        }

        void increment_error() noexcept {
            error_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::unordered_map<std::string, std::unique_ptr<MetricData>> metrics_;
    mutable std::shared_mutex metrics_mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<bool> collection_enabled_{true};

    // Background reporting
    std::thread reporter_thread_;
    std::atomic<bool> stop_reporter_{false};
    std::condition_variable reporter_cv_;
    std::mutex reporter_mutex_;
    std::chrono::seconds report_interval_{30};

    void reporter_loop() {
        while (!stop_reporter_.load(std::memory_order_acquire)) {
            std::unique_lock lock(reporter_mutex_);
            if (reporter_cv_.wait_for(lock, report_interval_,
                                    [this] { return stop_reporter_.load(); })) {
                break; // Stop requested
            }

            generate_report();
        }
    }

    MetricData& get_or_create_metric(const std::string& name) {
        std::shared_lock read_lock(metrics_mutex_);
        auto it = metrics_.find(name);
        if (it != metrics_.end()) {
            return *it->second;
        }
        read_lock.unlock();

        std::unique_lock write_lock(metrics_mutex_);
        // Double-check after acquiring write lock
        it = metrics_.find(name);
        if (it != metrics_.end()) {
            return *it->second;
        }

        auto [inserted_it, success] = metrics_.emplace(name, std::make_unique<MetricData>());
        return *inserted_it->second;
    }

public:
    explicit MetricsCollector(std::shared_ptr<spdlog::logger> logger = nullptr,
                             std::chrono::seconds report_interval = std::chrono::seconds{30})
        : logger_(logger), report_interval_(report_interval) {

        if (!logger_) {
            // Create default logger with rotating file sink
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "xml_performance.log", 1024 * 1024 * 10, 3); // 10MB, 3 files
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            logger_ = std::make_shared<spdlog::logger>("xml_metrics",
                                                      spdlog::sinks_init_list{file_sink, console_sink});
            logger_->set_level(spdlog::level::debug);
            spdlog::register_logger(logger_);
        }

        // Start background reporter
        reporter_thread_ = std::thread(&MetricsCollector::reporter_loop, this);

        if (logger_) {
            logger_->info("MetricsCollector initialized with {}s report interval",
                         report_interval_.count());
        }
    }

    ~MetricsCollector() {
        stop_reporter_.store(true, std::memory_order_release);
        reporter_cv_.notify_all();

        if (reporter_thread_.joinable()) {
            reporter_thread_.join();
        }

        // Generate final report
        generate_report();

        if (logger_) {
            logger_->info("MetricsCollector destroyed");
        }
    }

    /**
     * @brief Record operation timing
     */
    void record_timing(const std::string& operation_name, double time_microseconds) {
        if (!collection_enabled_.load(std::memory_order_relaxed)) {
            return;
        }

        auto& metric = get_or_create_metric(operation_name);
        metric.update(time_microseconds);

        if (logger_) {
            logger_->trace("Recorded timing for '{}': {:.3f}μs", operation_name, time_microseconds);
        }
    }

    /**
     * @brief Record operation error
     */
    void record_error(const std::string& operation_name) {
        if (!collection_enabled_.load(std::memory_order_relaxed)) {
            return;
        }

        auto& metric = get_or_create_metric(operation_name);
        metric.increment_error();

        if (logger_) {
            logger_->warn("Recorded error for operation '{}'", operation_name);
        }
    }

    /**
     * @brief Create scoped timer for automatic measurement
     */
    [[nodiscard]] ScopedTimer create_scoped_timer(const std::string& operation_name,
                                                  const std::source_location& location =
                                                      std::source_location::current()) {
        return ScopedTimer{operation_name, logger_, location};
    }

    /**
     * @brief Performance statistics for an operation
     */
    struct OperationStats {
        std::string name;
        uint64_t count;
        double total_time_ms;
        double avg_time_us;
        double min_time_us;
        double max_time_us;
        uint64_t error_count;
        double error_rate;
        double throughput_ops_per_sec;
    };

    /**
     * @brief Get statistics for a specific operation
     */
    [[nodiscard]] std::optional<OperationStats> get_stats(const std::string& operation_name) const {
        std::shared_lock lock(metrics_mutex_);
        auto it = metrics_.find(operation_name);
        if (it == metrics_.end()) {
            return std::nullopt;
        }

        const auto& metric = *it->second;
        auto count = metric.count.load(std::memory_order_relaxed);
        if (count == 0) {
            return std::nullopt;
        }

        auto total_time = metric.total_time.load(std::memory_order_relaxed);
        auto min_time = metric.min_time.load(std::memory_order_relaxed);
        auto max_time = metric.max_time.load(std::memory_order_relaxed);
        auto error_count = metric.error_count.load(std::memory_order_relaxed);

        return OperationStats{
            .name = operation_name,
            .count = count,
            .total_time_ms = total_time / 1000.0,
            .avg_time_us = total_time / count,
            .min_time_us = min_time,
            .max_time_us = max_time,
            .error_count = error_count,
            .error_rate = static_cast<double>(error_count) / count,
            .throughput_ops_per_sec = count / (total_time / 1'000'000.0)
        };
    }

    /**
     * @brief Get all operation statistics
     */
    [[nodiscard]] std::vector<OperationStats> get_all_stats() const {
        std::vector<OperationStats> results;
        std::shared_lock lock(metrics_mutex_);

        results.reserve(metrics_.size());
        for (const auto& [name, metric] : metrics_) {
            if (auto stats = get_stats(name)) {
                results.push_back(*stats);
            }
        }

        return results;
    }

    /**
     * @brief Generate comprehensive performance report
     */
    void generate_report() const {
        if (!logger_) return;

        auto all_stats = get_all_stats();
        if (all_stats.empty()) {
            logger_->info("No performance metrics to report");
            return;
        }

        logger_->info("=== XML Performance Report ===");
        logger_->info("{:<25} {:>10} {:>12} {:>12} {:>12} {:>12} {:>8} {:>12}",
                     "Operation", "Count", "Avg(μs)", "Min(μs)", "Max(μs)",
                     "Total(ms)", "Errors", "Ops/sec");
        logger_->info(std::string(120, '-'));

        for (const auto& stats : all_stats) {
            logger_->info("{:<25} {:>10} {:>12.3f} {:>12.3f} {:>12.3f} {:>12.3f} {:>8} {:>12.1f}",
                         stats.name, stats.count, stats.avg_time_us, stats.min_time_us,
                         stats.max_time_us, stats.total_time_ms, stats.error_count,
                         stats.throughput_ops_per_sec);
        }
        logger_->info(std::string(120, '='));
    }

    /**
     * @brief Enable/disable metrics collection
     */
    void set_collection_enabled(bool enabled) noexcept {
        collection_enabled_.store(enabled, std::memory_order_relaxed);
        if (logger_) {
            logger_->info("Metrics collection {}", enabled ? "enabled" : "disabled");
        }
    }

    /**
     * @brief Clear all collected metrics
     */
    void clear_metrics() {
        std::unique_lock lock(metrics_mutex_);
        metrics_.clear();
        if (logger_) {
            logger_->info("All metrics cleared");
        }
    }

    /**
     * @brief Set report interval
     */
    void set_report_interval(std::chrono::seconds interval) {
        report_interval_ = interval;
        if (logger_) {
            logger_->info("Report interval set to {}s", interval.count());
        }
    }
};

/**
 * @brief RAII wrapper for automatic timing with metrics collection
 */
class AutoTimer {
private:
    HighResolutionTimer timer_;
    std::string operation_name_;
    MetricsCollector* collector_;

public:
    AutoTimer(std::string operation_name, MetricsCollector* collector)
        : operation_name_(std::move(operation_name)), collector_(collector) {}

    ~AutoTimer() {
        if (collector_) {
            collector_->record_timing(operation_name_, timer_.elapsed_microseconds());
        }
    }

    AutoTimer(const AutoTimer&) = delete;
    AutoTimer& operator=(const AutoTimer&) = delete;
    AutoTimer(AutoTimer&&) = delete;
    AutoTimer& operator=(AutoTimer&&) = delete;
};

// Convenience macro for automatic timing
#define XML_AUTO_TIMER(collector, operation) \
    auto CONCAT(_timer_, __LINE__) = ::atom::extra::pugixml::performance::AutoTimer{operation, collector}

}  // namespace atom::extra::pugixml::performance
