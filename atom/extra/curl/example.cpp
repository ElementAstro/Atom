/**
 * @file example.cpp
 * @brief Example demonstrating high-performance curl components
 *
 * This example showcases the lock-free, high-performance implementations
 * of connection pools, session pools, caches, rate limiters, thread pools,
 * and memory pools optimized for multicore architectures.
 */

#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include <spdlog/spdlog.h>

#include "connection_pool.hpp"
#include "session_pool.hpp"
#include "cache.hpp"
#include "rate_limiter.hpp"
#include "thread_pool.hpp"
#include "memory_pool.hpp"
#include "benchmark.hpp"

using namespace atom::extra::curl;

/**
 * @brief Demonstrate connection pool performance
 */
void demonstrateConnectionPool() {
    spdlog::info("=== Connection Pool Demo ===");

    // Create high-performance connection pool
    ConnectionPool pool(100);

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate concurrent access
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&pool]() {
            for (int j = 0; j < 1000; ++j) {
                CURL* handle = pool.acquire();
                if (handle) {
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
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

    const auto& stats = pool.getStatistics();
    spdlog::info("Connection pool completed 10,000 operations in {}ms", duration.count());
    spdlog::info("Stats - Acquired: {}, Released: {}, Created: {}, Destroyed: {}",
                 stats.acquire_count.load(), stats.release_count.load(),
                 stats.create_count.load(), stats.destroy_count.load());
}

/**
 * @brief Demonstrate session pool with work stealing
 */
void demonstrateSessionPool() {
    spdlog::info("=== Session Pool Demo ===");

    // Create high-throughput session pool
    SessionPool pool(SessionPool::Config::createHighThroughput());

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate concurrent session usage
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&pool]() {
            for (int j = 0; j < 500; ++j) {
                auto session = pool.acquire();
                if (session) {
                    // Simulate session work
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    pool.release(session);
                }
            }
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto& stats = pool.getStatistics();
    spdlog::info("Session pool completed 4,000 operations in {}ms", duration.count());
    spdlog::info("Stats - Cache hits: {}, Work steals: {}, Contention: {}",
                 stats.cache_hits.load(), stats.work_steals.load(), stats.contention_count.load());
}

/**
 * @brief Demonstrate lock-free cache performance
 */
void demonstrateCache() {
    spdlog::info("=== Lock-Free Cache Demo ===");

    // Create high-performance cache
    Cache cache(Cache::Config::createHighPerformance());

    // Create test response
    std::vector<char> body{'H', 'e', 'l', 'l', 'o'};
    std::map<std::string, std::string> headers{{"Content-Type", "text/plain"}};
    Response response(200, body, headers);

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate concurrent cache operations
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 6; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&cache, &response, i]() {
            for (int j = 0; j < 1000; ++j) {
                std::string url = "http://test" + std::to_string((i * 1000 + j) % 100) + ".com";

                if (j % 3 == 0) {
                    // Set operation
                    cache.set(url, response);
                } else {
                    // Get operation
                    auto cached = cache.get(url);
                }
            }
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto& stats = cache.getStatistics();
    spdlog::info("Cache completed 6,000 operations in {}ms", duration.count());
    spdlog::info("Stats - Hit ratio: {:.2f}%, Collisions: {}, Size: {}",
                 stats.getHitRatio() * 100.0, stats.collision_count.load(), cache.size());
}

/**
 * @brief Demonstrate atomic rate limiter
 */
void demonstrateRateLimiter() {
    spdlog::info("=== Atomic Rate Limiter Demo ===");

    // Create high-throughput rate limiter
    RateLimiter limiter(RateLimiter::Config::createHighThroughput());

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate concurrent rate limiting
    std::atomic<int> successful_requests{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 4; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&limiter, &successful_requests]() {
            for (int j = 0; j < 2000; ++j) {
                if (limiter.try_acquire()) {
                    successful_requests.fetch_add(1);
                }
            }
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto& stats = limiter.getStatistics();
    spdlog::info("Rate limiter processed 8,000 requests in {}ms", duration.count());
    spdlog::info("Stats - Allowed: {}, Denied: {}, Allow ratio: {:.2f}%",
                 stats.requests_allowed.load(), stats.requests_denied.load(),
                 stats.getAllowedRatio() * 100.0);
}

/**
 * @brief Demonstrate memory pool allocation
 */
void demonstrateMemoryPool() {
    spdlog::info("=== Memory Pool Demo ===");

    // Create high-throughput memory pool
    MemoryPool<std::vector<char>> pool(MemoryPool<std::vector<char>>::Config::createHighThroughput());

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate concurrent allocations
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 4; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&pool]() {
            std::vector<std::vector<char>*> allocated;

            // Allocation phase
            for (int j = 0; j < 1000; ++j) {
                auto* buffer = pool.allocate(1024);  // 1KB buffers
                allocated.push_back(buffer);
            }

            // Deallocation phase
            for (auto* buffer : allocated) {
                pool.deallocate(buffer);
            }
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    const auto& stats = pool.getStatistics();
    spdlog::info("Memory pool completed 4,000 alloc/dealloc cycles in {}ms", duration.count());
    spdlog::info("Stats - Cache hit ratio: {:.2f}%, Memory usage: {} bytes",
                 stats.getCacheHitRatio() * 100.0, pool.getMemoryUsage());
}

/**
 * @brief Run comprehensive benchmarks
 */
void runBenchmarks() {
    spdlog::info("=== Running Comprehensive Benchmarks ===");

    benchmark::BenchmarkSuite suite(benchmark::BenchmarkSuite::Config::createDefault());
    suite.runAll();
}

int main() {
    // Configure logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Starting high-performance curl components demonstration");

    try {
        demonstrateConnectionPool();
        std::cout << std::endl;

        demonstrateSessionPool();
        std::cout << std::endl;

        demonstrateCache();
        std::cout << std::endl;

        demonstrateRateLimiter();
        std::cout << std::endl;

        demonstrateMemoryPool();
        std::cout << std::endl;

        runBenchmarks();

    } catch (const std::exception& e) {
        spdlog::error("Error during demonstration: {}", e.what());
        return 1;
    }

    spdlog::info("Demonstration completed successfully!");
    return 0;
}
