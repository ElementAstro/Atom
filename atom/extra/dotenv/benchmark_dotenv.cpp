#include "dotenv.hpp"

#include <iostream>
#include <chrono>
#include <vector>
#include <fstream>
#include <random>

using namespace dotenv;

/**
 * @brief Benchmark concurrent hash map operations
 */
void benchmark_hashmap() {
    std::cout << "=== Concurrent HashMap Benchmark ===\n";
    
    const std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    const int operations_per_thread = 100000;
    
    for (int num_threads : thread_counts) {
        concurrency::ConcurrentHashMap<std::string, std::string> map;
        concurrency::ThreadPool pool(num_threads);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<void>> futures;
        
        for (int t = 0; t < num_threads; ++t) {
            futures.emplace_back(pool.submit([&map, t, operations_per_thread]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, operations_per_thread * 10);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    std::string key = "key_" + std::to_string(t * operations_per_thread + i);
                    std::string value = "value_" + std::to_string(dis(gen));
                    
                    map.insert_or_assign(key, value);
                    
                    // 20% reads
                    if (i % 5 == 0) {
                        auto result = map.find(key);
                        (void)result;
                    }
                }
            }));
        }
        
        for (auto& future : futures) {
            future.get();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        int total_ops = num_threads * operations_per_thread;
        double ops_per_sec = total_ops * 1000000.0 / duration.count();
        
        std::cout << "Threads: " << num_threads 
                  << ", Operations: " << total_ops
                  << ", Time: " << duration.count() << "μs"
                  << ", Ops/sec: " << static_cast<int>(ops_per_sec)
                  << ", Map size: " << map.size() << "\n";
    }
}

/**
 * @brief Benchmark file loading performance
 */
void benchmark_file_loading() {
    std::cout << "\n=== File Loading Benchmark ===\n";
    
    // Create test files
    const int num_files = 10;
    std::vector<std::filesystem::path> files;
    
    for (int i = 0; i < num_files; ++i) {
        std::string filename = "bench_test_" + std::to_string(i) + ".env";
        files.push_back(filename);
        
        std::ofstream file(filename);
        file << "# Benchmark test file " << i << "\n";
        for (int j = 0; j < 100; ++j) {
            file << "VAR_" << i << "_" << j << "=value_" << j << "\n";
        }
        file.close();
    }
    
    DotenvOptions options;
    Dotenv dotenv(options);
    
    // Sequential loading
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& file : files) {
            auto result = dotenv.load(file);
            (void)result;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Sequential loading: " << duration.count() << "μs\n";
    }
    
    // Parallel loading
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto future_result = dotenv.loadMultipleParallel(files);
        auto result = future_result.get();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Parallel loading: " << duration.count() << "μs\n";
        std::cout << "Variables loaded: " << result.variables.size() << "\n";
    }
    
    // Cleanup
    for (const auto& file : files) {
        std::filesystem::remove(file);
    }
}

/**
 * @brief Benchmark thread pool performance
 */
void benchmark_thread_pool() {
    std::cout << "\n=== Thread Pool Benchmark ===\n";
    
    const std::vector<int> pool_sizes = {1, 2, 4, 8};
    const int num_tasks = 10000;
    
    for (int pool_size : pool_sizes) {
        concurrency::ThreadPool pool(pool_size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<int>> futures;
        
        for (int i = 0; i < num_tasks; ++i) {
            futures.emplace_back(pool.submit([i]() {
                // Simulate some work
                int sum = 0;
                for (int j = 0; j < 1000; ++j) {
                    sum += i * j;
                }
                return sum;
            }));
        }
        
        // Collect results
        long long total = 0;
        for (auto& future : futures) {
            total += future.get();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double tasks_per_sec = num_tasks * 1000000.0 / duration.count();
        
        std::cout << "Pool size: " << pool_size
                  << ", Tasks: " << num_tasks
                  << ", Time: " << duration.count() << "μs"
                  << ", Tasks/sec: " << static_cast<int>(tasks_per_sec)
                  << ", Result: " << total << "\n";
    }
}

/**
 * @brief Memory allocation benchmark
 */
void benchmark_memory_allocation() {
    std::cout << "\n=== Memory Allocation Benchmark ===\n";
    
    const int num_allocations = 100000;
    
    // Standard allocation
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::string*> ptrs;
        ptrs.reserve(num_allocations);
        
        for (int i = 0; i < num_allocations; ++i) {
            ptrs.push_back(new std::string("test_string_" + std::to_string(i)));
        }
        
        for (auto* ptr : ptrs) {
            delete ptr;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Standard allocation: " << duration.count() << "μs\n";
    }
    
    // Pool allocation
    {
        memory::LockFreeMemoryPool<std::string> pool;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::string*> ptrs;
        ptrs.reserve(num_allocations);
        
        for (int i = 0; i < num_allocations; ++i) {
            ptrs.push_back(pool.construct("test_string_" + std::to_string(i)));
        }
        
        for (auto* ptr : ptrs) {
            pool.destroy(ptr);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Pool allocation: " << duration.count() << "μs\n";
    }
}

int main() {
    std::cout << "=== Dotenv Advanced Concurrency Benchmarks ===\n\n";
    
    try {
        benchmark_hashmap();
        benchmark_file_loading();
        benchmark_thread_pool();
        benchmark_memory_allocation();
        
        // Performance monitoring summary
        auto& monitor = performance::get_monitor();
        monitor.log_report();
        
        std::cout << "\n=== Benchmarks Completed ===\n";
        
    } catch (const std::exception& e) {
        std::cout << "Benchmark failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
