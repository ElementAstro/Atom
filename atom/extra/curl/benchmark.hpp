#ifndef ATOM_EXTRA_CURL_BENCHMARK_HPP
#define ATOM_EXTRA_CURL_BENCHMARK_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>

#include "connection_pool.hpp"
#include "session_pool.hpp"
#include "cache.hpp"
#include "rate_limiter.hpp"
#include "thread_pool.hpp"
#include "memory_pool.hpp"

namespace atom::extra::curl::benchmark {

/**
 * @brief Performance measurement utilities
 */
class PerformanceMeter {
public:
    struct Metrics {
        std::chrono::nanoseconds total_time{0};
        std::chrono::nanoseconds min_time{std::chrono::nanoseconds::max()};
        std::chrono::nanoseconds max_time{0};
        std::chrono::nanoseconds avg_time{0};
        uint64_t operations = 0;
        double throughput = 0.0;  // operations per second

        void calculate() {
            if (operations > 0) {
                avg_time = total_time / operations;
                throughput = static_cast<double>(operations) * 1e9 / total_time.count();
            }
        }
    };

    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = end_time - start_time_;

        metrics_.total_time += duration;
        metrics_.min_time = std::min(metrics_.min_time, duration);
        metrics_.max_time = std::max(metrics_.max_time, duration);
        metrics_.operations++;
    }

    const Metrics& getMetrics() {
        metrics_.calculate();
        return metrics_;
    }

    void reset() {
        metrics_ = Metrics{};
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    Metrics metrics_;
};

/**
 * @brief Benchmark suite for curl components
 */
class BenchmarkSuite {
public:
    struct Config {
        size_t thread_count = std::thread::hardware_concurrency();
        size_t operations_per_thread = 10000;
        size_t warmup_operations = 1000;
        bool enable_detailed_logging = false;

        static Config createDefault() {
            return Config{};
        }

        static Config createStressTest() {
            Config config;
            config.thread_count = std::thread::hardware_concurrency() * 2;
            config.operations_per_thread = 100000;
            config.warmup_operations = 10000;
            return config;
        }
    };

    explicit BenchmarkSuite(const Config& config = Config::createDefault());

    /**
     * @brief Run all benchmarks
     */
    void runAll();

    /**
     * @brief Benchmark connection pool performance
     */
    void benchmarkConnectionPool();

    /**
     * @brief Benchmark session pool performance
     */
    void benchmarkSessionPool();

    /**
     * @brief Benchmark cache performance
     */
    void benchmarkCache();

    /**
     * @brief Benchmark rate limiter performance
     */
    void benchmarkRateLimiter();

    /**
     * @brief Benchmark thread pool performance
     */
    void benchmarkThreadPool();

    /**
     * @brief Benchmark memory pool performance
     */
    void benchmarkMemoryPool();

    /**
     * @brief Thread safety validation tests
     */
    void validateThreadSafety();

    /**
     * @brief Scalability tests across different core counts
     */
    void testScalability();

    /**
     * @brief Print comprehensive results
     */
    void printResults() const;

private:
    const Config config_;
    std::map<std::string, PerformanceMeter::Metrics> results_;

    /**
     * @brief Run benchmark with multiple threads
     */
    template<typename F>
    PerformanceMeter::Metrics runMultiThreadedBenchmark(
        const std::string& name, F&& benchmark_func);

    /**
     * @brief Warmup phase to stabilize performance
     */
    template<typename F>
    void warmup(F&& func, size_t iterations);

    /**
     * @brief Validate that operations are thread-safe
     */
    template<typename F>
    bool validateConcurrentOperations(F&& func, size_t iterations);
};

/**
 * @brief Specific benchmark implementations
 */
namespace benchmarks {

/**
 * @brief Connection pool acquire/release benchmark
 */
class ConnectionPoolBenchmark {
public:
    explicit ConnectionPoolBenchmark(size_t pool_size = 100);
    void run(size_t iterations);
    PerformanceMeter::Metrics getMetrics() const { return meter_.getMetrics(); }

private:
    std::unique_ptr<ConnectionPool> pool_;
    mutable PerformanceMeter meter_;
};

/**
 * @brief Session pool acquire/release benchmark
 */
class SessionPoolBenchmark {
public:
    explicit SessionPoolBenchmark(const SessionPool::Config& config = SessionPool::Config::createDefault());
    void run(size_t iterations);
    PerformanceMeter::Metrics getMetrics() const { return meter_.getMetrics(); }

private:
    std::unique_ptr<SessionPool> pool_;
    mutable PerformanceMeter meter_;
};

/**
 * @brief Cache get/set benchmark
 */
class CacheBenchmark {
public:
    explicit CacheBenchmark(const Cache::Config& config = Cache::Config::createDefault());
    void run(size_t iterations);
    PerformanceMeter::Metrics getMetrics() const { return meter_.getMetrics(); }

private:
    std::unique_ptr<Cache> cache_;
    mutable PerformanceMeter meter_;
    std::vector<std::string> test_urls_;
    std::vector<Response> test_responses_;

    void generateTestData();
};

/**
 * @brief Rate limiter acquire benchmark
 */
class RateLimiterBenchmark {
public:
    explicit RateLimiterBenchmark(const RateLimiter::Config& config = RateLimiter::Config::createDefault());
    void run(size_t iterations);
    PerformanceMeter::Metrics getMetrics() const { return meter_.getMetrics(); }

private:
    std::unique_ptr<RateLimiter> limiter_;
    mutable PerformanceMeter meter_;
};

/**
 * @brief Thread pool task submission benchmark
 */
class ThreadPoolBenchmark {
public:
    explicit ThreadPoolBenchmark(const ThreadPool::Config& config = ThreadPool::Config::createDefault());
    void run(size_t iterations);
    PerformanceMeter::Metrics getMetrics() const { return meter_.getMetrics(); }

private:
    std::unique_ptr<ThreadPool> pool_;
    mutable PerformanceMeter meter_;
};

/**
 * @brief Memory pool allocation benchmark
 */
class MemoryPoolBenchmark {
public:
    explicit MemoryPoolBenchmark(const MemoryPool<std::vector<char>>::Config& config =
                                MemoryPool<std::vector<char>>::Config::createDefault());
    void run(size_t iterations);
    PerformanceMeter::Metrics getMetrics() const { return meter_.getMetrics(); }

private:
    std::unique_ptr<MemoryPool<std::vector<char>>> pool_;
    mutable PerformanceMeter meter_;
};

}  // namespace benchmarks

/**
 * @brief Utility functions for benchmark execution
 */
namespace utils {

/**
 * @brief Generate random test data
 */
std::vector<std::string> generateRandomUrls(size_t count);
std::vector<Response> generateRandomResponses(size_t count);

/**
 * @brief CPU and memory usage monitoring
 */
class ResourceMonitor {
public:
    struct Usage {
        double cpu_percent = 0.0;
        size_t memory_mb = 0;
        size_t peak_memory_mb = 0;
    };

    void start();
    void stop();
    Usage getUsage() const { return usage_; }

private:
    Usage usage_;
    std::atomic<bool> monitoring_{false};
    std::thread monitor_thread_;

    void monitorLoop();
};

/**
 * @brief Statistical analysis utilities
 */
class Statistics {
public:
    static double calculatePercentile(const std::vector<double>& values, double percentile);
    static double calculateStandardDeviation(const std::vector<double>& values);
    static void printDistribution(const std::vector<double>& values, const std::string& name);
};

}  // namespace utils
}  // namespace atom::extra::curl::benchmark

#endif  // ATOM_EXTRA_CURL_BENCHMARK_HPP
