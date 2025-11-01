/*
 * performance_optimization_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Performance Optimization Example
Demonstrates profiling, benchmarking, optimization techniques,
and comprehensive performance analysis with the component system.

**************************************************/

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"

// Note: Registry is in the global namespace, not atom::components

/**
 * @brief Performance profiler for component operations
 */
class PerformanceProfiler {
public:
    struct ProfileData {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
        std::chrono::microseconds duration{0};
        size_t callCount{0};
        size_t totalCalls{0};
        double averageDuration{0.0};
        double minDuration{std::numeric_limits<double>::max()};
        double maxDuration{0.0};
    };

    static PerformanceProfiler& instance() {
        static PerformanceProfiler instance;
        return instance;
    }

    void startProfile(const std::string& name) {
        auto& data = profiles_[name];
        data.name = name;
        data.startTime = std::chrono::high_resolution_clock::now();
    }

    void endProfile(const std::string& name) {
        auto end = std::chrono::high_resolution_clock::now();
        auto& data = profiles_[name];

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - data.startTime);
        data.duration = duration;
        data.callCount++;
        data.totalCalls++;

        double durationMs = duration.count() / 1000.0;
        data.averageDuration =
            (data.averageDuration * (data.totalCalls - 1) + durationMs) /
            data.totalCalls;
        data.minDuration = std::min(data.minDuration, durationMs);
        data.maxDuration = std::max(data.maxDuration, durationMs);
    }

    const std::unordered_map<std::string, ProfileData>& getProfiles() const {
        return profiles_;
    }

    void reset() { profiles_.clear(); }

    void printReport() const {
        std::cout << "\n=== Performance Profile Report ===" << std::endl;
        std::cout << std::left << std::setw(25) << "Operation" << std::setw(10)
                  << "Calls" << std::setw(12) << "Avg (ms)" << std::setw(12)
                  << "Min (ms)" << std::setw(12) << "Max (ms)" << std::setw(15)
                  << "Total (ms)" << std::endl;
        std::cout << std::string(86, '-') << std::endl;

        for (const auto& pair : profiles_) {
            const auto& data = pair.second;
            double totalMs = data.averageDuration * data.totalCalls;

            std::cout << std::left << std::setw(25) << data.name
                      << std::setw(10) << data.totalCalls << std::setw(12)
                      << std::fixed << std::setprecision(3)
                      << data.averageDuration << std::setw(12)
                      << data.minDuration << std::setw(12) << data.maxDuration
                      << std::setw(15) << totalMs << std::endl;
        }
    }

private:
    std::unordered_map<std::string, ProfileData> profiles_;
};

/**
 * @brief RAII profiler helper
 */
class ScopedProfiler {
public:
    explicit ScopedProfiler(const std::string& name) : name_(name) {
        PerformanceProfiler::instance().startProfile(name_);
    }

    ~ScopedProfiler() { PerformanceProfiler::instance().endProfile(name_); }

private:
    std::string name_;
};

#define PROFILE(name) ScopedProfiler _prof(name)

/**
 * @brief Component for performance testing and optimization
 */
class PerformanceTestComponent : public Component {
public:
    explicit PerformanceTestComponent(const std::string& name)
        : Component(name) {
        std::cout << "PerformanceTestComponent '" << name << "' created"
                  << std::endl;

        // Initialize test data
        initializeTestData();
        setupCommands();
    }

private:
    std::vector<int> largeDataSet_;
    std::vector<double> computationData_;
    std::unordered_map<std::string, int> lookupTable_;

    void initializeTestData() {
        PROFILE("InitializeTestData");

        // Create large dataset for testing
        largeDataSet_.reserve(100000);
        computationData_.reserve(100000);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> intDis(1, 1000);
        std::uniform_real_distribution<> realDis(0.0, 100.0);

        for (int i = 0; i < 100000; ++i) {
            largeDataSet_.push_back(intDis(gen));
            computationData_.push_back(realDis(gen));
        }

        // Create lookup table
        for (int i = 0; i < 10000; ++i) {
            lookupTable_["key_" + std::to_string(i)] = intDis(gen);
        }

        std::cout << "  [" << getName()
                  << "] Initialized test data: " << largeDataSet_.size()
                  << " integers, " << computationData_.size() << " doubles, "
                  << lookupTable_.size() << " lookup entries" << std::endl;
    }

    void setupCommands() {
        // Unoptimized operations
        def("slowSort", [this]() -> double {
            PROFILE("SlowSort");

            auto data = largeDataSet_;  // Copy for sorting

            // Bubble sort (intentionally slow)
            for (size_t i = 0; i < data.size(); ++i) {
                for (size_t j = 0; j < data.size() - 1 - i; ++j) {
                    if (data[j] > data[j + 1]) {
                        std::swap(data[j], data[j + 1]);
                    }
                }
            }

            return static_cast<double>(data.back());
        });

        def("fastSort", [this]() -> double {
            PROFILE("FastSort");

            auto data = largeDataSet_;  // Copy for sorting
            std::sort(data.begin(), data.end());

            return static_cast<double>(data.back());
        });

        // Memory access patterns
        def("randomAccess", [this]() -> double {
            PROFILE("RandomAccess");

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(
                0, static_cast<int>(computationData_.size() - 1));

            double sum = 0.0;
            for (int i = 0; i < 10000; ++i) {
                int index = dis(gen);
                sum += computationData_[index];
            }

            return sum;
        });

        def("sequentialAccess", [this]() -> double {
            PROFILE("SequentialAccess");

            double sum = 0.0;
            for (size_t i = 0;
                 i < std::min(size_t(10000), computationData_.size()); ++i) {
                sum += computationData_[i];
            }

            return sum;
        });

        // Computation optimization
        def("unoptimizedComputation", [this]() -> double {
            PROFILE("UnoptimizedComputation");

            double result = 0.0;
            for (size_t i = 0; i < computationData_.size(); ++i) {
                // Inefficient computation
                result += std::sin(computationData_[i]) *
                          std::cos(computationData_[i]);
                result += std::sqrt(computationData_[i]);
                result += std::pow(computationData_[i], 0.5);
            }

            return result;
        });

        def("optimizedComputation", [this]() -> double {
            PROFILE("OptimizedComputation");

            double result = 0.0;
            for (size_t i = 0; i < computationData_.size(); ++i) {
                double val = computationData_[i];
                double sqrtVal = std::sqrt(val);

                // Optimized: reuse sqrt calculation, use sin*cos = 0.5*sin(2x)
                result += 0.5 * std::sin(2.0 * val);
                result += sqrtVal;
                result += sqrtVal;  // pow(x, 0.5) == sqrt(x)
            }

            return result;
        });

        // Memory allocation patterns
        def("inefficientAllocation", [this]() -> int {
            PROFILE("InefficientAllocation");

            int count = 0;
            for (int i = 0; i < 1000; ++i) {
                // Inefficient: many small allocations
                auto vec = std::make_unique<std::vector<int>>();
                for (int j = 0; j < 100; ++j) {
                    vec->push_back(j);
                }
                count += static_cast<int>(vec->size());
            }

            return count;
        });

        def("efficientAllocation", [this]() -> int {
            PROFILE("EfficientAllocation");

            int count = 0;

            // Efficient: single large allocation with reserve
            std::vector<int> vec;
            vec.reserve(100000);

            for (int i = 0; i < 1000; ++i) {
                for (int j = 0; j < 100; ++j) {
                    vec.push_back(j);
                }
            }

            count = static_cast<int>(vec.size());
            return count;
        });

        // Lookup operations
        def("linearSearch", [this]() -> int {
            PROFILE("LinearSearch");

            int found = 0;
            std::vector<std::string> keys;

            // Create keys to search for
            for (int i = 0; i < 1000; ++i) {
                keys.push_back("key_" + std::to_string(i * 10));
            }

            // Linear search through map
            for (const auto& key : keys) {
                for (const auto& pair : lookupTable_) {
                    if (pair.first == key) {
                        found++;
                        break;
                    }
                }
            }

            return found;
        });

        def("hashLookup", [this]() -> int {
            PROFILE("HashLookup");

            int found = 0;

            // Hash table lookup
            for (int i = 0; i < 1000; ++i) {
                std::string key = "key_" + std::to_string(i * 10);
                if (lookupTable_.find(key) != lookupTable_.end()) {
                    found++;
                }
            }

            return found;
        });

        // Batch operations
        def("individualOperations", [this]() -> double {
            PROFILE("IndividualOperations");

            double sum = 0.0;

            // Process each element individually
            for (size_t i = 0;
                 i < std::min(size_t(1000), computationData_.size()); ++i) {
                sum += std::sin(computationData_[i]);
            }

            return sum;
        });

        def("batchOperations", [this]() -> double {
            PROFILE("BatchOperations");

            // Process in batches for better cache utilization
            double sum = 0.0;
            const size_t batchSize = 64;
            const size_t maxElements =
                std::min(size_t(1000), computationData_.size());

            for (size_t i = 0; i < maxElements; i += batchSize) {
                size_t end = std::min(i + batchSize, maxElements);

                // Process batch
                for (size_t j = i; j < end; ++j) {
                    sum += std::sin(computationData_[j]);
                }
            }

            return sum;
        });

        // Profiling and benchmarking commands
        def("runBenchmark",
            [this](const std::string& operation,
                   int iterations) -> std::string {
                std::cout << "  [" << getName()
                          << "] Running benchmark: " << operation << " ("
                          << iterations << " iterations)" << std::endl;

                auto start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < iterations; ++i) {
                    [[maybe_unused]] auto result = runCommand(operation, {});
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end - start);

                double avgTime =
                    static_cast<double>(duration.count()) / iterations;

                return "Total: " + std::to_string(duration.count()) + " μs, " +
                       "Average: " + std::to_string(avgTime) + " μs";
            });

        def("compareOperations",
            [this](const std::string& op1, const std::string& op2,
                   int iterations) -> std::string {
                std::cout << "  [" << getName()
                          << "] Comparing operations: " << op1 << " vs " << op2
                          << std::endl;

                // Benchmark first operation
                auto start1 = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < iterations; ++i) {
                    [[maybe_unused]] auto result = runCommand(op1, {});
                }
                auto end1 = std::chrono::high_resolution_clock::now();
                auto duration1 =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end1 - start1);

                // Benchmark second operation
                auto start2 = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < iterations; ++i) {
                    [[maybe_unused]] auto result = runCommand(op2, {});
                }
                auto end2 = std::chrono::high_resolution_clock::now();
                auto duration2 =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end2 - start2);

                double ratio =
                    static_cast<double>(duration1.count()) / duration2.count();

                return op1 + ": " + std::to_string(duration1.count()) +
                       " μs, " + op2 + ": " +
                       std::to_string(duration2.count()) + " μs, " +
                       "Ratio: " + std::to_string(ratio) + "x";
            });

        def("getProfileReport", []() -> std::string {
            PerformanceProfiler::instance().printReport();
            return "Profile report printed to console";
        });

        def("resetProfiler", []() -> std::string {
            PerformanceProfiler::instance().reset();
            return "Profiler reset";
        });
    }
};

void demonstrateBasicProfiling() {
    std::cout << "\n=== Basic Profiling Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n1. Creating performance test component..." << std::endl;
    auto component =
        registry.createComponent<PerformanceTestComponent>("PerfTest");

    std::cout << "\n2. Running basic profiled operations..." << std::endl;

    // Test basic operations with profiling
    [[maybe_unused]] auto result1 =
        component->runCommand("sequentialAccess", {});
    [[maybe_unused]] auto result2 = component->runCommand("randomAccess", {});
    [[maybe_unused]] auto result3 = component->runCommand("hashLookup", {});
    [[maybe_unused]] auto result4 = component->runCommand("linearSearch", {});

    // Print initial profile report
    [[maybe_unused]] auto reportResult =
        component->runCommand("getProfileReport", {});
}

void demonstrateAlgorithmOptimization() {
    std::cout << "\n=== Algorithm Optimization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("PerfTest");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n3. Comparing sorting algorithms..." << std::endl;

    // Note: Slow sort is disabled for large datasets to avoid excessive runtime
    std::cout << "Running fast sort..." << std::endl;
    auto fastResult = component->runCommand("fastSort", {});
    std::cout << "Fast sort result: " << std::any_cast<std::string>(fastResult)
              << std::endl;

    std::cout << "\n4. Comparing computation optimizations..." << std::endl;
    std::vector<std::any> compArgs = {
        std::any(std::string("unoptimizedComputation")),
        std::any(std::string("optimizedComputation")),
        std::any(std::string("3"))};
    auto compResult = component->runCommand("compareOperations", compArgs);
    std::cout << "Computation comparison: "
              << std::any_cast<std::string>(compResult) << std::endl;
}

void demonstrateMemoryOptimization() {
    std::cout << "\n=== Memory Optimization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("PerfTest");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n5. Comparing memory allocation patterns..." << std::endl;
    std::vector<std::any> allocArgs = {
        std::any(std::string("inefficientAllocation")),
        std::any(std::string("efficientAllocation")),
        std::any(std::string("5"))};
    auto allocResult = component->runCommand("compareOperations", allocArgs);
    std::cout << "Allocation comparison: "
              << std::any_cast<std::string>(allocResult) << std::endl;

    std::cout << "\n6. Comparing memory access patterns..." << std::endl;
    std::vector<std::any> accessArgs = {
        std::any(std::string("randomAccess")),
        std::any(std::string("sequentialAccess")), std::any(std::string("10"))};
    auto accessResult = component->runCommand("compareOperations", accessArgs);
    std::cout << "Access pattern comparison: "
              << std::any_cast<std::string>(accessResult) << std::endl;
}

void demonstrateLookupOptimization() {
    std::cout << "\n=== Lookup Optimization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("PerfTest");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n7. Comparing lookup strategies..." << std::endl;
    std::vector<std::any> lookupArgs = {std::any(std::string("linearSearch")),
                                        std::any(std::string("hashLookup")),
                                        std::any(std::string("5"))};
    auto lookupResult = component->runCommand("compareOperations", lookupArgs);
    std::cout << "Lookup comparison: "
              << std::any_cast<std::string>(lookupResult) << std::endl;
}

void demonstrateBatchOptimization() {
    std::cout << "\n=== Batch Optimization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("PerfTest");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n8. Comparing batch vs individual operations..."
              << std::endl;
    std::vector<std::any> batchArgs = {
        std::any(std::string("individualOperations")),
        std::any(std::string("batchOperations")), std::any(std::string("10"))};
    auto batchResult = component->runCommand("compareOperations", batchArgs);
    std::cout << "Batch processing comparison: "
              << std::any_cast<std::string>(batchResult) << std::endl;
}

void demonstrateComprehensiveBenchmark() {
    std::cout << "\n=== Comprehensive Benchmark Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto component = registry.getComponent("PerfTest");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n9. Running comprehensive benchmark suite..." << std::endl;

    // Reset profiler for clean benchmark
    [[maybe_unused]] auto resetResult =
        component->runCommand("resetProfiler", {});

    // Run multiple iterations of each operation
    std::vector<std::pair<std::string, int>> benchmarks = {
        {"sequentialAccess", 50},    {"randomAccess", 50},
        {"hashLookup", 20},          {"optimizedComputation", 5},
        {"efficientAllocation", 10}, {"batchOperations", 20}};

    for (const auto& benchmark : benchmarks) {
        std::cout << "\nBenchmarking " << benchmark.first << "..." << std::endl;
        std::vector<std::any> benchArgs = {
            std::any(benchmark.first),
            std::any(std::to_string(benchmark.second))};
        auto result = component->runCommand("runBenchmark", benchArgs);
        std::cout << "Result: " << std::any_cast<std::string>(result)
                  << std::endl;
    }

    // Final comprehensive report
    std::cout << "\n=== Final Performance Report ===" << std::endl;
    [[maybe_unused]] auto finalReportResult =
        component->runCommand("getProfileReport", {});
}

void demonstrateOptimizationRecommendations() {
    std::cout << "\n=== Optimization Recommendations Demo ===" << std::endl;

    std::cout << "\n10. Performance optimization recommendations..."
              << std::endl;

    std::cout << "\n--- Algorithm Optimization ---" << std::endl;
    std::cout << "• Use efficient algorithms (O(n log n) vs O(n²))"
              << std::endl;
    std::cout << "• Prefer standard library implementations" << std::endl;
    std::cout << "• Consider algorithmic complexity for large datasets"
              << std::endl;

    std::cout << "\n--- Memory Optimization ---" << std::endl;
    std::cout << "• Reserve memory for containers when size is known"
              << std::endl;
    std::cout << "• Prefer sequential memory access patterns" << std::endl;
    std::cout << "• Minimize memory allocations in hot paths" << std::endl;
    std::cout << "• Use object pools for frequently allocated objects"
              << std::endl;

    std::cout << "\n--- Data Structure Optimization ---" << std::endl;
    std::cout << "• Use hash tables for O(1) lookups" << std::endl;
    std::cout << "• Consider cache-friendly data layouts (SoA vs AoS)"
              << std::endl;
    std::cout << "• Use appropriate container types for use case" << std::endl;

    std::cout << "\n--- Computation Optimization ---" << std::endl;
    std::cout << "• Avoid redundant calculations" << std::endl;
    std::cout << "• Use batch processing for better cache utilization"
              << std::endl;
    std::cout << "• Consider SIMD operations for parallel data processing"
              << std::endl;
    std::cout << "• Profile before optimizing to identify bottlenecks"
              << std::endl;

    std::cout << "\n--- Component System Optimization ---" << std::endl;
    std::cout << "• Minimize command dispatch overhead" << std::endl;
    std::cout << "• Use component pools for frequent creation/destruction"
              << std::endl;
    std::cout << "• Batch component operations when possible" << std::endl;
    std::cout << "• Consider component lifecycle optimization" << std::endl;
}

int main() {
    std::cout << "=== Atom Component Performance Optimization Examples ==="
              << std::endl;

    try {
        demonstrateBasicProfiling();
        demonstrateAlgorithmOptimization();
        demonstrateMemoryOptimization();
        demonstrateLookupOptimization();
        demonstrateBatchOptimization();
        demonstrateComprehensiveBenchmark();
        demonstrateOptimizationRecommendations();

        std::cout << "\n=== All Performance Optimization Examples Completed "
                     "Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in performance optimization examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
