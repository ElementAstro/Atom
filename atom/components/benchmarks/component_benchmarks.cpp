/*
 * component_benchmarks.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "component_benchmarks.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace atom::components::benchmarks {

ComponentBenchmarkSuite::ComponentBenchmarkSuite(const BenchmarkConfig& config)
    : config_(config) {
    // Initialize component pool
    PoolConfig poolConfig;
    poolConfig.initialPoolSize = config_.componentCount;
    poolConfig.maxPoolSize = config_.componentCount * 2;
    poolConfig.enableStatistics = true;
    componentPool_ =
        std::make_unique<ComponentPool<BenchmarkComponent>>(poolConfig);

    // Initialize batch processor
    ComponentBatchProcessor::Config batchConfig;
    batchConfig.batchSize = 64;
    batchConfig.numThreads = config_.numThreads;
    batchConfig.enableSIMD = config_.enableSIMDBenchmarks;
    batchProcessor_ = std::make_unique<ComponentBatchProcessor>(batchConfig);

    warmup();
}

void ComponentBenchmarkSuite::warmup() {
    // Warm up the system with some operations
    for (size_t i = 0; i < config_.warmupIterations; ++i) {
        auto component =
            std::make_shared<BenchmarkComponent>("warmup_" + std::to_string(i));
        component->setValue(static_cast<int>(i));
        component->dispatch("testFunction");
    }
}

std::vector<BenchmarkResult> ComponentBenchmarkSuite::runAllBenchmarks() {
    std::vector<BenchmarkResult> allResults;

    std::cout << "Running Component System Benchmarks...\n";
    std::cout << "Configuration:\n";
    std::cout << "  Iterations: " << config_.iterations << "\n";
    std::cout << "  Component Count: " << config_.componentCount << "\n";
    std::cout << "  Threads: "
              << (config_.numThreads == 0 ? std::thread::hardware_concurrency()
                                          : config_.numThreads)
              << "\n\n";

    // Run all benchmark categories
    auto lifecycleResults = benchmarkComponentLifecycle();
    auto poolResults = benchmarkMemoryPools();
    auto iterationResults = benchmarkIteration();
    auto dispatchResults = benchmarkCommandDispatch();
    auto memoryResults = benchmarkMemoryUsage();
    auto cacheResults = benchmarkCachePerformance();

    // Combine all results
    allResults.insert(allResults.end(), lifecycleResults.begin(),
                      lifecycleResults.end());
    allResults.insert(allResults.end(), poolResults.begin(), poolResults.end());
    allResults.insert(allResults.end(), iterationResults.begin(),
                      iterationResults.end());
    allResults.insert(allResults.end(), dispatchResults.begin(),
                      dispatchResults.end());
    allResults.insert(allResults.end(), memoryResults.begin(),
                      memoryResults.end());
    allResults.insert(allResults.end(), cacheResults.begin(),
                      cacheResults.end());

    if (config_.enableSIMDBenchmarks) {
        auto simdResults = benchmarkSIMD();
        allResults.insert(allResults.end(), simdResults.begin(),
                          simdResults.end());
    }

    if (config_.enableParallelBenchmarks) {
        auto parallelResults = benchmarkParallel();
        allResults.insert(allResults.end(), parallelResults.begin(),
                          parallelResults.end());
    }

    return allResults;
}

std::vector<BenchmarkResult>
ComponentBenchmarkSuite::benchmarkComponentLifecycle() {
    std::vector<BenchmarkResult> results;

    // Component creation benchmark
    results.push_back(measurePerformance(
        "Component Creation",
        [this]() {
            for (size_t i = 0; i < config_.iterations; ++i) {
                auto component = std::make_shared<BenchmarkComponent>(
                    "test_" + std::to_string(i));
                testComponents_.push_back(component);
            }
        },
        config_.iterations));

    // Component initialization benchmark
    results.push_back(measurePerformance(
        "Component Initialization",
        [this]() {
            for (auto& component : testComponents_) {
                component->initialize();
            }
        },
        testComponents_.size()));

    // Component destruction benchmark
    results.push_back(measurePerformance(
        "Component Destruction",
        [this]() {
            for (auto& component : testComponents_) {
                component->destroy();
            }
            testComponents_.clear();
        },
        testComponents_.size()));

    return results;
}

std::vector<BenchmarkResult> ComponentBenchmarkSuite::benchmarkMemoryPools() {
    std::vector<BenchmarkResult> results;

    // Pool allocation benchmark
    results.push_back(measurePerformance(
        "Pool Allocation",
        [this]() {
            std::vector<std::shared_ptr<BenchmarkComponent>> poolComponents;
            for (size_t i = 0; i < config_.iterations; ++i) {
                auto component =
                    componentPool_->allocate("pool_test_" + std::to_string(i));
                poolComponents.push_back(component);
            }
        },
        config_.iterations));

    // Compare with standard allocation
    results.push_back(measurePerformance(
        "Standard Allocation",
        [this]() {
            std::vector<std::shared_ptr<BenchmarkComponent>> standardComponents;
            for (size_t i = 0; i < config_.iterations; ++i) {
                auto component = std::make_shared<BenchmarkComponent>(
                    "std_test_" + std::to_string(i));
                standardComponents.push_back(component);
            }
        },
        config_.iterations));

    return results;
}

std::vector<BenchmarkResult> ComponentBenchmarkSuite::benchmarkIteration() {
    std::vector<BenchmarkResult> results;

    // Prepare test components
    std::vector<std::shared_ptr<BenchmarkComponent>> components;
    for (size_t i = 0; i < config_.componentCount; ++i) {
        components.push_back(std::make_shared<BenchmarkComponent>(
            "iter_test_" + std::to_string(i)));
    }

    // Standard iteration benchmark
    results.push_back(measurePerformance(
        "Standard Iteration",
        [&components]() {
            for (auto& component : components) {
                component->setValue(component->getValue() + 1);
            }
        },
        components.size()));

    // Batch processing benchmark
    results.push_back(measurePerformance(
        "Batch Processing",
        [this, &components]() {
            std::vector<BenchmarkComponent*> rawPtrs;
            rawPtrs.reserve(components.size());
            for (auto& comp : components) {
                rawPtrs.push_back(comp.get());
            }

            batchProcessor_->processBatches(
                rawPtrs.data(), rawPtrs.size(),
                [](BenchmarkComponent* comp) { comp->batchUpdate(); });
        },
        components.size()));

    return results;
}

std::vector<BenchmarkResult>
ComponentBenchmarkSuite::benchmarkCommandDispatch() {
    std::vector<BenchmarkResult> results;

    // Prepare test component
    auto component = std::make_shared<BenchmarkComponent>("dispatch_test");

    // Standard dispatch benchmark
    results.push_back(measurePerformance(
        "Command Dispatch",
        [&component, this]() {
            for (size_t i = 0; i < config_.iterations; ++i) {
                component->dispatch("testFunction");
            }
        },
        config_.iterations));

    // Fast dispatch benchmark
    results.push_back(measurePerformance(
        "Fast Dispatch",
        [&component, this]() {
            for (size_t i = 0; i < config_.iterations; ++i) {
                component->fastDispatch("testFunction");
            }
        },
        config_.iterations));

    return results;
}

std::vector<BenchmarkResult> ComponentBenchmarkSuite::benchmarkMemoryUsage() {
    std::vector<BenchmarkResult> results;

    size_t initialMemory = getCurrentMemoryUsage();

    // Memory usage with standard components
    std::vector<std::shared_ptr<BenchmarkComponent>> standardComponents;
    for (size_t i = 0; i < config_.componentCount; ++i) {
        standardComponents.push_back(std::make_shared<BenchmarkComponent>(
            "mem_test_" + std::to_string(i)));
    }

    size_t standardMemory = getCurrentMemoryUsage() - initialMemory;
    results.emplace_back("Standard Component Memory",
                         std::chrono::nanoseconds{0}, config_.componentCount,
                         standardMemory);

    standardComponents.clear();

    // Memory usage with pooled components
    std::vector<std::shared_ptr<BenchmarkComponent>> pooledComponents;
    for (size_t i = 0; i < config_.componentCount; ++i) {
        pooledComponents.push_back(
            componentPool_->allocate("pool_mem_test_" + std::to_string(i)));
    }

    size_t pooledMemory = getCurrentMemoryUsage() - initialMemory;
    results.emplace_back("Pooled Component Memory", std::chrono::nanoseconds{0},
                         config_.componentCount, pooledMemory);

    return results;
}

std::vector<BenchmarkResult>
ComponentBenchmarkSuite::benchmarkCachePerformance() {
    std::vector<BenchmarkResult> results;

    // Create components with different access patterns
    std::vector<std::shared_ptr<BenchmarkComponent>> components;
    for (size_t i = 0; i < config_.componentCount; ++i) {
        components.push_back(std::make_shared<BenchmarkComponent>(
            "cache_test_" + std::to_string(i)));
    }

    // Sequential access benchmark
    results.push_back(measurePerformance(
        "Sequential Access",
        [&components]() {
            for (size_t i = 0; i < components.size(); ++i) {
                components[i]->setValue(static_cast<int>(i));
            }
        },
        components.size()));

    // Random access benchmark
    results.push_back(measurePerformance(
        "Random Access",
        [&components]() {
            for (size_t i = 0; i < components.size(); ++i) {
                size_t randomIndex =
                    (i * 7919) % components.size();  // Simple pseudo-random
                components[randomIndex]->setValue(
                    static_cast<int>(randomIndex));
            }
        },
        components.size()));

    return results;
}

template <typename Func>
BenchmarkResult ComponentBenchmarkSuite::measurePerformance(
    const std::string& name, Func&& func, size_t operations) {
    size_t memoryBefore =
        config_.enableMemoryTracking ? getCurrentMemoryUsage() : 0;

    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();

    size_t memoryAfter =
        config_.enableMemoryTracking ? getCurrentMemoryUsage() : 0;
    size_t memoryUsed =
        memoryAfter > memoryBefore ? memoryAfter - memoryBefore : 0;

    auto duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    size_t ops = operations > 0 ? operations : 1;

    return BenchmarkResult(name, duration, ops, memoryUsed);
}

size_t ComponentBenchmarkSuite::getCurrentMemoryUsage() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024;  // Convert KB to bytes on Linux
    }
#endif
    return 0;
}

void ComponentBenchmarkSuite::printResults(
    const std::vector<BenchmarkResult>& results) {
    std::cout << std::left << std::setw(30) << "Benchmark Name" << std::right
              << std::setw(15) << "Duration (ms)" << std::setw(15)
              << "Operations" << std::setw(18) << "Ops/Second" << std::setw(15)
              << "Memory (KB)" << std::setw(15) << "Efficiency" << std::endl;

    std::cout << std::string(108, '-') << std::endl;

    for (const auto& result : results) {
        std::cout << std::left << std::setw(30) << result.name << std::right
                  << std::setw(15) << std::fixed << std::setprecision(3)
                  << (result.duration.count() / 1e6) << std::setw(15)
                  << result.operations << std::setw(18) << std::fixed
                  << std::setprecision(0) << result.operationsPerSecond
                  << std::setw(15) << (result.memoryUsed / 1024)
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << result.memoryEfficiency << std::endl;
    }
    std::cout << std::endl;
}

}  // namespace atom::components::benchmarks
