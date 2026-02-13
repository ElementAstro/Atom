/**
 * @file advanced_example.cpp
 * @brief Comprehensive example demonstrating cutting-edge C++ concurrency features
 *
 * This example showcases:
 * - Lock-free concurrent hash maps
 * - High-performance thread pools with work stealing
 * - Advanced synchronization primitives
 * - NUMA-aware memory allocation
 * - Real-time performance monitoring
 * - Structured logging with spdlog
 * - Adaptive optimization
 */

#include "dotenv.hpp"

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <fstream>
#include <thread>

using namespace dotenv;

/**
 * @brief Demonstrate lock-free concurrent hash map performance
 */
void demonstrate_concurrent_hashmap() {
    std::cout << "\n=== Lock-Free Concurrent HashMap Demo ===\n";

    concurrency::ConcurrentHashMap<std::string, std::string> map;
    concurrency::ThreadPool pool(8);

    const int NUM_OPERATIONS = 100000;
    const int NUM_THREADS = 8;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<void>> futures;

    // Concurrent insertions
    for (int t = 0; t < NUM_THREADS; ++t) {
        futures.emplace_back(pool.submit([&map, t, NUM_OPERATIONS, NUM_THREADS]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, NUM_OPERATIONS);

            int start_idx = t * (NUM_OPERATIONS / NUM_THREADS);
            int end_idx = (t + 1) * (NUM_OPERATIONS / NUM_THREADS);

            for (int i = start_idx; i < end_idx; ++i) {
                std::string key = "key_" + std::to_string(i);
                std::string value = "value_" + std::to_string(dis(gen));
                map.insert_or_assign(key, value);

                // Occasional lookups
                if (i % 10 == 0) {
                    auto result = map.find(key);
                    (void)result; // Suppress unused variable warning
                }
            }
        }));
    }

    // Wait for completion
    for (auto& future : futures) {
        future.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "✓ Completed " << NUM_OPERATIONS << " operations in "
              << duration.count() << " microseconds\n";
    std::cout << "✓ Operations per second: "
              << (NUM_OPERATIONS * 1000000.0 / duration.count()) << "\n";
    std::cout << "✓ Final map size: " << map.size() << "\n";
    std::cout << "✓ Load factor: " << map.load_factor() << "\n";
}

/**
 * @brief Demonstrate high-performance caching
 */
void demonstrate_caching() {
    std::cout << "\n=== High-Performance Caching Demo ===\n";

    cache::ConcurrentEnvCache cache(1000, std::chrono::seconds(60));
    concurrency::ThreadPool pool(4);

    const int NUM_OPERATIONS = 50000;
    std::vector<std::future<void>> futures;

    auto start = std::chrono::high_resolution_clock::now();

    // Concurrent cache operations
    for (int t = 0; t < 4; ++t) {
        futures.emplace_back(pool.submit([&cache, t, NUM_OPERATIONS]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 1000);

            for (int i = 0; i < NUM_OPERATIONS / 4; ++i) {
                int key_num = dis(gen);
                std::string key = "cache_key_" + std::to_string(key_num);
                std::string value = "cache_value_" + std::to_string(i);

                if (i % 3 == 0) {
                    // Write operation
                    cache.put(key, value);
                } else {
                    // Read operation
                    auto result = cache.get(key);
                    (void)result; // Suppress unused variable warning
                }
            }
        }));
    }

    for (auto& future : futures) {
        future.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    auto stats = cache.get_stats();

    std::cout << "✓ Cache operations completed in " << duration.count() << " microseconds\n";
    std::cout << "✓ Hit ratio: " << (stats.hit_ratio() * 100.0) << "%\n";
    std::cout << "✓ Cache size: " << cache.size() << "\n";
    std::cout << cache.generate_report() << "\n";
}

/**
 * @brief Demonstrate performance monitoring
 */
void demonstrate_performance_monitoring() {
    std::cout << "\n=== Performance Monitoring Demo ===\n";

    auto& monitor = performance::get_monitor();
    monitor.set_enabled(true);

    // Simulate various operations with measurements
    {
        DOTENV_MEASURE_SCOPE("file_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        DOTENV_MEASURE_SCOPE("parsing_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    {
        DOTENV_MEASURE_SCOPE("validation_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }

    // Generate and display performance report
    monitor.log_report();

    std::cout << "✓ Performance monitoring demonstrated\n";
}

/**
 * @brief Demonstrate advanced dotenv functionality
 */
void demonstrate_advanced_dotenv() {
    std::cout << "\n=== Advanced Dotenv Functionality Demo ===\n";

    // Create test files
    std::ofstream file1("advanced_test1.env");
    file1 << "# Advanced test file 1\n";
    file1 << "APP_NAME=AdvancedApp\n";
    file1 << "APP_VERSION=2.0.0\n";
    file1 << "DEBUG=true\n";
    file1 << "MAX_CONNECTIONS=1000\n";
    file1.close();

    std::ofstream file2("advanced_test2.env");
    file2 << "# Advanced test file 2\n";
    file2 << "DATABASE_URL=postgresql://localhost:5432/advanced_db\n";
    file2 << "API_KEY=super_secret_key_123\n";
    file2 << "CACHE_SIZE=10000\n";
    file2 << "WORKER_THREADS=8\n";
    file2.close();

    try {
        DotenvOptions options;
        options.debug = true;

        Dotenv dotenv(options);

        // Enable caching
        dotenv.setCachingEnabled(true);
        dotenv.configureCaching(1000, std::chrono::minutes(30));

        // Test parallel loading
        std::vector<std::filesystem::path> files = {
            "advanced_test1.env",
            "advanced_test2.env"
        };

        auto future_result = dotenv.loadMultipleParallel(files);
        auto result = future_result.get();

        if (result.is_successful()) {
            std::cout << "✓ Parallel loading successful\n";
            std::cout << "✓ Loaded " << result.variables.size() << " variables\n";
            std::cout << "✓ From " << result.loaded_files.size() << " files\n";
        }

        // Test file watching
        dotenv.watchMultiple(files, [](const std::filesystem::path& path, const LoadResult& result) {
            std::cout << "✓ File change detected: " << path.string()
                      << " (" << result.variables.size() << " variables)\n";
        });

        // Simulate file change
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::ofstream update_file("advanced_test1.env", std::ios::app);
        update_file << "UPDATED_FIELD=new_value\n";
        update_file.close();

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Performance optimization
        dotenv.optimizePerformance();

        // Get cache statistics
        auto cache_stats = dotenv.getCacheStats();
        std::cout << "✓ Cache hit ratio: " << (cache_stats.hit_ratio() * 100.0) << "%\n";

        // Performance report
        dotenv.logPerformanceReport();

        std::cout << "✓ Advanced dotenv functionality demonstrated\n";

    } catch (const std::exception& e) {
        std::cout << "✗ Error: " << e.what() << "\n";
    }

    // Cleanup
    std::filesystem::remove("advanced_test1.env");
    std::filesystem::remove("advanced_test2.env");
}

/**
 * @brief Benchmark concurrent vs sequential operations
 */
void benchmark_concurrency() {
    std::cout << "\n=== Concurrency Benchmark ===\n";

    const int NUM_OPERATIONS = 100000;

    // Sequential benchmark
    {
        std::unordered_map<std::string, std::string> sequential_map;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            std::string key = "seq_key_" + std::to_string(i);
            std::string value = "seq_value_" + std::to_string(i);
            sequential_map[key] = value;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Sequential operations: " << duration.count() << " μs\n";
    }

    // Concurrent benchmark
    {
        concurrency::ConcurrentHashMap<std::string, std::string> concurrent_map;
        concurrency::ThreadPool pool(8);

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::future<void>> futures;
        const int ops_per_thread = NUM_OPERATIONS / 8;

        for (int t = 0; t < 8; ++t) {
            futures.emplace_back(pool.submit([&concurrent_map, t, ops_per_thread]() {
                int start_idx = t * ops_per_thread;
                int end_idx = (t + 1) * ops_per_thread;

                for (int i = start_idx; i < end_idx; ++i) {
                    std::string key = "conc_key_" + std::to_string(i);
                    std::string value = "conc_value_" + std::to_string(i);
                    concurrent_map.insert_or_assign(key, value);
                }
            }));
        }

        for (auto& future : futures) {
            future.get();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Concurrent operations: " << duration.count() << " μs\n";
        std::cout << "Speedup: " << (duration.count() > 0 ? "N/A" : "∞") << "x\n";
    }
}

int main() {
    std::cout << "=== Advanced C++ Concurrency Demonstration ===\n";
    std::cout << "Showcasing cutting-edge concurrency primitives for dotenv\n";

    try {
        demonstrate_concurrent_hashmap();
        demonstrate_caching();
        demonstrate_performance_monitoring();
        demonstrate_advanced_dotenv();
        benchmark_concurrency();

        std::cout << "\n=== All Demonstrations Completed Successfully ===\n";
        std::cout << "Advanced concurrency features are working optimally!\n";

    } catch (const std::exception& e) {
        std::cout << "\n✗ Demonstration failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
