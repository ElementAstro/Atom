#include "benchmark.hpp"
#include "response.hpp"
#include <random>
#include <algorithm>
#include <future>
#include <iomanip>

namespace atom::extra::curl::benchmark {

BenchmarkSuite::BenchmarkSuite(const Config& config) : config_(config) {
    spdlog::info("Initializing benchmark suite: {} threads, {} ops/thread, warmup: {}", 
                 config_.thread_count, config_.operations_per_thread, config_.warmup_operations);
}

void BenchmarkSuite::runAll() {
    spdlog::info("Starting comprehensive benchmark suite...");
    
    benchmarkConnectionPool();
    benchmarkSessionPool();
    benchmarkCache();
    benchmarkRateLimiter();
    benchmarkThreadPool();
    benchmarkMemoryPool();
    
    validateThreadSafety();
    testScalability();
    
    printResults();
}

void BenchmarkSuite::benchmarkConnectionPool() {
    spdlog::info("Benchmarking connection pool...");
    
    auto metrics = runMultiThreadedBenchmark("ConnectionPool", [this](size_t thread_id) {
        benchmarks::ConnectionPoolBenchmark benchmark(100);
        warmup([&]() { benchmark.run(1); }, config_.warmup_operations);
        benchmark.run(config_.operations_per_thread);
        return benchmark.getMetrics();
    });
    
    results_["ConnectionPool"] = metrics;
    spdlog::info("Connection pool benchmark completed: {:.2f} ops/sec", metrics.throughput);
}

void BenchmarkSuite::benchmarkSessionPool() {
    spdlog::info("Benchmarking session pool...");
    
    auto metrics = runMultiThreadedBenchmark("SessionPool", [this](size_t thread_id) {
        benchmarks::SessionPoolBenchmark benchmark;
        warmup([&]() { benchmark.run(1); }, config_.warmup_operations);
        benchmark.run(config_.operations_per_thread);
        return benchmark.getMetrics();
    });
    
    results_["SessionPool"] = metrics;
    spdlog::info("Session pool benchmark completed: {:.2f} ops/sec", metrics.throughput);
}

void BenchmarkSuite::benchmarkCache() {
    spdlog::info("Benchmarking cache...");
    
    auto metrics = runMultiThreadedBenchmark("Cache", [this](size_t thread_id) {
        benchmarks::CacheBenchmark benchmark;
        warmup([&]() { benchmark.run(1); }, config_.warmup_operations);
        benchmark.run(config_.operations_per_thread);
        return benchmark.getMetrics();
    });
    
    results_["Cache"] = metrics;
    spdlog::info("Cache benchmark completed: {:.2f} ops/sec", metrics.throughput);
}

void BenchmarkSuite::benchmarkRateLimiter() {
    spdlog::info("Benchmarking rate limiter...");
    
    auto metrics = runMultiThreadedBenchmark("RateLimiter", [this](size_t thread_id) {
        benchmarks::RateLimiterBenchmark benchmark;
        warmup([&]() { benchmark.run(1); }, config_.warmup_operations);
        benchmark.run(config_.operations_per_thread);
        return benchmark.getMetrics();
    });
    
    results_["RateLimiter"] = metrics;
    spdlog::info("Rate limiter benchmark completed: {:.2f} ops/sec", metrics.throughput);
}

void BenchmarkSuite::benchmarkThreadPool() {
    spdlog::info("Benchmarking thread pool...");
    
    auto metrics = runMultiThreadedBenchmark("ThreadPool", [this](size_t thread_id) {
        benchmarks::ThreadPoolBenchmark benchmark;
        warmup([&]() { benchmark.run(1); }, config_.warmup_operations);
        benchmark.run(config_.operations_per_thread);
        return benchmark.getMetrics();
    });
    
    results_["ThreadPool"] = metrics;
    spdlog::info("Thread pool benchmark completed: {:.2f} ops/sec", metrics.throughput);
}

void BenchmarkSuite::benchmarkMemoryPool() {
    spdlog::info("Benchmarking memory pool...");
    
    auto metrics = runMultiThreadedBenchmark("MemoryPool", [this](size_t thread_id) {
        benchmarks::MemoryPoolBenchmark benchmark;
        warmup([&]() { benchmark.run(1); }, config_.warmup_operations);
        benchmark.run(config_.operations_per_thread);
        return benchmark.getMetrics();
    });
    
    results_["MemoryPool"] = metrics;
    spdlog::info("Memory pool benchmark completed: {:.2f} ops/sec", metrics.throughput);
}

void BenchmarkSuite::validateThreadSafety() {
    spdlog::info("Validating thread safety...");
    
    // Test connection pool thread safety
    bool connection_pool_safe = validateConcurrentOperations([](size_t iterations) {
        ConnectionPool pool(50);
        for (size_t i = 0; i < iterations; ++i) {
            CURL* handle = pool.acquire();
            if (handle) {
                pool.release(handle);
            }
        }
    }, 1000);
    
    // Test cache thread safety
    bool cache_safe = validateConcurrentOperations([](size_t iterations) {
        Cache cache;
        Response response;
        response.set_status_code(200);
        response.set_body("test");
        
        for (size_t i = 0; i < iterations; ++i) {
            std::string url = "http://test" + std::to_string(i % 100) + ".com";
            cache.set(url, response);
            cache.get(url);
        }
    }, 1000);
    
    spdlog::info("Thread safety validation - ConnectionPool: {}, Cache: {}", 
                 connection_pool_safe ? "PASS" : "FAIL", 
                 cache_safe ? "PASS" : "FAIL");
}

void BenchmarkSuite::testScalability() {
    spdlog::info("Testing scalability across different core counts...");
    
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 16, std::thread::hardware_concurrency()};
    
    for (size_t threads : thread_counts) {
        if (threads > std::thread::hardware_concurrency() * 2) continue;
        
        spdlog::info("Testing with {} threads", threads);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Test connection pool scalability
        std::vector<std::future<void>> futures;
        ConnectionPool pool(threads * 10);
        
        for (size_t i = 0; i < threads; ++i) {
            futures.emplace_back(std::async(std::launch::async, [&pool]() {
                for (size_t j = 0; j < 1000; ++j) {
                    CURL* handle = pool.acquire();
                    if (handle) {
                        pool.release(handle);
                    }
                }
            }));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double throughput = (threads * 1000.0) / (duration.count() / 1000.0);
        spdlog::info("Scalability test with {} threads: {:.2f} ops/sec", threads, throughput);
    }
}

void BenchmarkSuite::printResults() const {
    spdlog::info("\n=== BENCHMARK RESULTS ===");
    
    std::cout << std::left << std::setw(20) << "Component" 
              << std::setw(15) << "Throughput" 
              << std::setw(15) << "Avg Time" 
              << std::setw(15) << "Min Time" 
              << std::setw(15) << "Max Time" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& [name, metrics] : results_) {
        std::cout << std::left << std::setw(20) << name
                  << std::setw(15) << std::fixed << std::setprecision(2) << metrics.throughput
                  << std::setw(15) << metrics.avg_time.count() / 1000.0 << "μs"
                  << std::setw(15) << metrics.min_time.count() / 1000.0 << "μs"
                  << std::setw(15) << metrics.max_time.count() / 1000.0 << "μs" << std::endl;
    }
    
    std::cout << std::string(80, '-') << std::endl;
}

template<typename F>
PerformanceMeter::Metrics BenchmarkSuite::runMultiThreadedBenchmark(
    const std::string& name, F&& benchmark_func) {
    
    std::vector<std::future<PerformanceMeter::Metrics>> futures;
    
    for (size_t i = 0; i < config_.thread_count; ++i) {
        futures.emplace_back(std::async(std::launch::async, benchmark_func, i));
    }
    
    PerformanceMeter::Metrics combined_metrics;
    
    for (auto& future : futures) {
        auto metrics = future.get();
        combined_metrics.total_time += metrics.total_time;
        combined_metrics.operations += metrics.operations;
        combined_metrics.min_time = std::min(combined_metrics.min_time, metrics.min_time);
        combined_metrics.max_time = std::max(combined_metrics.max_time, metrics.max_time);
    }
    
    combined_metrics.calculate();
    return combined_metrics;
}

template<typename F>
void BenchmarkSuite::warmup(F&& func, size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
        func();
    }
}

template<typename F>
bool BenchmarkSuite::validateConcurrentOperations(F&& func, size_t iterations) {
    try {
        std::vector<std::future<void>> futures;
        
        for (size_t i = 0; i < config_.thread_count; ++i) {
            futures.emplace_back(std::async(std::launch::async, func, iterations));
        }
        
        for (auto& future : futures) {
            future.wait();
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Thread safety validation failed: {}", e.what());
        return false;
    }
}

// Benchmark implementations
namespace benchmarks {

ConnectionPoolBenchmark::ConnectionPoolBenchmark(size_t pool_size)
    : pool_(std::make_unique<ConnectionPool>(pool_size)) {}

void ConnectionPoolBenchmark::run(size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
        meter_.start();
        CURL* handle = pool_->acquire();
        if (handle) {
            pool_->release(handle);
        }
        meter_.stop();
    }
}

SessionPoolBenchmark::SessionPoolBenchmark(const SessionPool::Config& config)
    : pool_(std::make_unique<SessionPool>(config)) {}

void SessionPoolBenchmark::run(size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
        meter_.start();
        auto session = pool_->acquire();
        if (session) {
            pool_->release(session);
        }
        meter_.stop();
    }
}

CacheBenchmark::CacheBenchmark(const Cache::Config& config)
    : cache_(std::make_unique<Cache>(config)) {
    generateTestData();
}

void CacheBenchmark::run(size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
        size_t index = i % test_urls_.size();

        meter_.start();
        if (i % 3 == 0) {
            // Set operation
            cache_->set(test_urls_[index], test_responses_[index]);
        } else {
            // Get operation
            cache_->get(test_urls_[index]);
        }
        meter_.stop();
    }
}

void CacheBenchmark::generateTestData() {
    test_urls_ = utils::generateRandomUrls(1000);
    test_responses_ = utils::generateRandomResponses(1000);
}

RateLimiterBenchmark::RateLimiterBenchmark(const RateLimiter::Config& config)
    : limiter_(std::make_unique<RateLimiter>(config)) {}

void RateLimiterBenchmark::run(size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
        meter_.start();
        bool acquired = limiter_->try_acquire();
        meter_.stop();

        if (!acquired) {
            // Brief delay if rate limited
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}

ThreadPoolBenchmark::ThreadPoolBenchmark(const ThreadPool::Config& config)
    : pool_(std::make_unique<ThreadPool>(config)) {}

void ThreadPoolBenchmark::run(size_t iterations) {
    std::vector<std::future<void>> futures;

    meter_.start();
    for (size_t i = 0; i < iterations; ++i) {
        futures.emplace_back(pool_->submit([]() {
            // Simple computation task
            volatile int sum = 0;
            for (int j = 0; j < 100; ++j) {
                sum += j;
            }
        }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
    meter_.stop();
}

MemoryPoolBenchmark::MemoryPoolBenchmark(const MemoryPool<std::vector<char>>::Config& config)
    : pool_(std::make_unique<MemoryPool<std::vector<char>>>(config)) {}

void MemoryPoolBenchmark::run(size_t iterations) {
    std::vector<std::vector<char>*> allocated;
    allocated.reserve(iterations);

    // Allocation phase
    for (size_t i = 0; i < iterations; ++i) {
        meter_.start();
        auto* buffer = pool_->allocate(1024);  // 1KB buffer
        meter_.stop();
        allocated.push_back(buffer);
    }

    // Deallocation phase
    for (auto* buffer : allocated) {
        meter_.start();
        pool_->deallocate(buffer);
        meter_.stop();
    }
}

}  // namespace benchmarks

// Utility implementations
namespace utils {

std::vector<std::string> generateRandomUrls(size_t count) {
    std::vector<std::string> urls;
    urls.reserve(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    for (size_t i = 0; i < count; ++i) {
        urls.emplace_back("http://test" + std::to_string(dis(gen)) + ".com/path" + std::to_string(i));
    }

    return urls;
}

std::vector<Response> generateRandomResponses(size_t count) {
    std::vector<Response> responses;
    responses.reserve(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> status_dis(200, 299);
    std::uniform_int_distribution<> size_dis(100, 10000);

    for (size_t i = 0; i < count; ++i) {
        int status_code = status_dis(gen);
        std::string body_str(size_dis(gen), 'x');
        std::vector<char> body(body_str.begin(), body_str.end());
        std::map<std::string, std::string> headers{
            {"Content-Type", "text/plain"},
            {"Content-Length", std::to_string(body.size())}
        };

        responses.emplace_back(status_code, std::move(body), std::move(headers));
    }

    return responses;
}

}  // namespace utils
}  // namespace atom::extra::curl::benchmark
