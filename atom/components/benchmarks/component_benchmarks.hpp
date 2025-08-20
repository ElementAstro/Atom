/*
 * component_benchmarks.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Performance Benchmarking Suite
for Enhanced Component System. Measures component creation,
destruction, iteration, memory usage, and SIMD optimizations.

**************************************************/

#ifndef ATOM_COMPONENT_BENCHMARKS_HPP
#define ATOM_COMPONENT_BENCHMARKS_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../component.hpp"
#include "../component_pool.hpp"
#include "../iteration.hpp"

namespace atom::components::benchmarks {

/**
 * @brief Benchmark result structure
 */
struct BenchmarkResult {
    std::string name;
    std::chrono::nanoseconds duration;
    size_t operations;
    size_t memoryUsed;
    double operationsPerSecond;
    double memoryEfficiency;  // Operations per MB

    BenchmarkResult(const std::string& n, std::chrono::nanoseconds d,
                    size_t ops, size_t mem = 0)
        : name(n), duration(d), operations(ops), memoryUsed(mem) {
        operationsPerSecond = operations * 1e9 / duration.count();
        memoryEfficiency =
            memoryUsed > 0 ? operations / (memoryUsed / 1024.0 / 1024.0) : 0.0;
    }
};

/**
 * @brief Benchmark configuration
 */
struct BenchmarkConfig {
    size_t iterations = 10000;
    size_t warmupIterations = 1000;
    size_t componentCount = 1000;
    bool enableMemoryTracking = true;
    bool enableSIMDBenchmarks = true;
    bool enableParallelBenchmarks = true;
    size_t numThreads = 0;  // 0 = auto-detect
};

/**
 * @brief Test component for benchmarking
 */
class BenchmarkComponent : public Component {
public:
    explicit BenchmarkComponent(const std::string& name) : Component(name) {
        // Add some test data
        addVariable<int>("testInt", 42);
        addVariable<float>("testFloat", 3.14f);
        addVariable<std::string>("testString", "benchmark");

        // Register some test functions
        def("testFunction", [this]() { return getValue(); });
        def("updateValue", [this](int newValue) { setValue(newValue); });
    }

    int getValue() const { return value_; }
    void setValue(int value) { value_ = value; }

    // Batch processing interface
    void batchUpdate() { value_ *= 2; }
    float* getUpdateData() { return &floatData_; }
    size_t getUpdateDataSize() const { return 1; }

private:
    int value_{0};
    float floatData_{1.0f};
    alignas(64) char padding_[64 - sizeof(int) -
                              sizeof(float)];  // Cache line padding
};

/**
 * @brief Main benchmark suite
 */
class ComponentBenchmarkSuite {
public:
    explicit ComponentBenchmarkSuite(const BenchmarkConfig& config = {});

    /**
     * @brief Runs all benchmarks
     * @return Vector of benchmark results
     */
    std::vector<BenchmarkResult> runAllBenchmarks();

    /**
     * @brief Component creation/destruction benchmarks
     */
    std::vector<BenchmarkResult> benchmarkComponentLifecycle();

    /**
     * @brief Memory pool allocation benchmarks
     */
    std::vector<BenchmarkResult> benchmarkMemoryPools();

    /**
     * @brief Component iteration benchmarks
     */
    std::vector<BenchmarkResult> benchmarkIteration();

    /**
     * @brief SIMD optimization benchmarks
     */
    std::vector<BenchmarkResult> benchmarkSIMD();

    /**
     * @brief Parallel processing benchmarks
     */
    std::vector<BenchmarkResult> benchmarkParallel();

    /**
     * @brief Command dispatch benchmarks
     */
    std::vector<BenchmarkResult> benchmarkCommandDispatch();

    /**
     * @brief Memory usage analysis
     */
    std::vector<BenchmarkResult> benchmarkMemoryUsage();

    /**
     * @brief Cache performance benchmarks
     */
    std::vector<BenchmarkResult> benchmarkCachePerformance();

    /**
     * @brief Prints benchmark results in a formatted table
     * @param results Benchmark results to print
     */
    static void printResults(const std::vector<BenchmarkResult>& results);

    /**
     * @brief Exports results to CSV format
     * @param results Benchmark results
     * @param filename Output filename
     */
    static void exportToCSV(const std::vector<BenchmarkResult>& results,
                            const std::string& filename);

private:
    BenchmarkConfig config_;

    // Utility methods
    template <typename Func>
    BenchmarkResult measurePerformance(const std::string& name, Func&& func,
                                       size_t operations = 0);

    size_t getCurrentMemoryUsage() const;
    void warmup();

    // Test data
    std::vector<std::shared_ptr<BenchmarkComponent>> testComponents_;
    std::unique_ptr<ComponentPool<BenchmarkComponent>> componentPool_;
    std::unique_ptr<ComponentBatchProcessor> batchProcessor_;
};

/**
 * @brief Comparison benchmark runner for before/after analysis
 */
class ComparisonBenchmarkRunner {
public:
    struct ComparisonResult {
        std::string benchmarkName;
        BenchmarkResult baseline;
        BenchmarkResult optimized;
        double speedupRatio;
        double memoryImprovement;

        ComparisonResult(const std::string& name, const BenchmarkResult& base,
                         const BenchmarkResult& opt)
            : benchmarkName(name), baseline(base), optimized(opt) {
            speedupRatio = opt.operationsPerSecond / base.operationsPerSecond;
            memoryImprovement = base.memoryUsed > 0 && opt.memoryUsed > 0
                                    ? (double)base.memoryUsed / opt.memoryUsed
                                    : 1.0;
        }
    };

    /**
     * @brief Runs comparison between baseline and optimized implementations
     * @param config Benchmark configuration
     * @return Comparison results
     */
    static std::vector<ComparisonResult> runComparison(
        const BenchmarkConfig& config = {});

    /**
     * @brief Prints comparison results
     * @param results Comparison results
     */
    static void printComparison(const std::vector<ComparisonResult>& results);
};

/**
 * @brief Regression test runner for performance validation
 */
class PerformanceRegressionTester {
public:
    struct RegressionThresholds {
        double maxSlowdownRatio = 1.1;   // 10% slowdown threshold
        double maxMemoryIncrease = 1.2;  // 20% memory increase threshold
        double minSpeedupRatio = 0.95;   // 5% minimum performance retention
    };

    /**
     * @brief Runs regression tests against baseline performance
     * @param baselineResults Baseline benchmark results
     * @param currentResults Current benchmark results
     * @param thresholds Performance thresholds
     * @return True if all tests pass
     */
    static bool runRegressionTests(
        const std::vector<BenchmarkResult>& baselineResults,
        const std::vector<BenchmarkResult>& currentResults,
        const RegressionThresholds& thresholds = {});

    /**
     * @brief Generates performance report
     * @param results Benchmark results
     * @param filename Output filename
     */
    static void generateReport(const std::vector<BenchmarkResult>& results,
                               const std::string& filename);
};

}  // namespace atom::components::benchmarks

#endif  // ATOM_COMPONENT_BENCHMARKS_HPP
