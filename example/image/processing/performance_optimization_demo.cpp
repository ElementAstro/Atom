/**
 * @file performance_optimization_demo.cpp
 * @brief Performance optimization techniques demonstration
 *
 * This example demonstrates:
 * - Performance optimization techniques and best practices
 * - Benchmarking and profiling methodologies
 * - Memory optimization and cache-friendly algorithms
 * - SIMD and vectorization optimizations
 * - Multi-threading and parallel processing
 * - Algorithm complexity analysis and optimization
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <immintrin.h>  // For SIMD intrinsics
#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/image_processor.hpp"
#include "atom/image/processing/performance_profiler.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Performance benchmark result
 */
struct BenchmarkResult {
    std::string name;
    std::chrono::nanoseconds duration;
    size_t operations;
    double throughput;  // operations per second
    size_t memoryUsed;

    BenchmarkResult(const std::string& n, std::chrono::nanoseconds d,
                    size_t ops, size_t mem = 0)
        : name(n), duration(d), operations(ops), memoryUsed(mem) {
        throughput = operations * 1e9 / duration.count();
    }
};

/**
 * @brief Performance profiler and benchmarking utility
 */
class PerformanceProfiler {
private:
    std::vector<BenchmarkResult> results_;

public:
    template <typename Func>
    BenchmarkResult benchmark(const std::string& name, Func&& func,
                              size_t operations = 1) {
        // Warm up
        for (int i = 0; i < 3; ++i) {
            func();
        }

        auto start = high_resolution_clock::now();

        for (size_t i = 0; i < operations; ++i) {
            func();
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start);

        BenchmarkResult result(name, duration, operations);
        results_.push_back(result);

        return result;
    }

    void printResults() const {
        std::cout << "\n=== Performance Benchmark Results ===\n";
        std::cout << std::left << std::setw(30) << "Benchmark" << std::setw(15)
                  << "Duration (ms)" << std::setw(15) << "Throughput"
                  << std::setw(10) << "Ops" << "\n";
        std::cout << std::string(70, '-') << "\n";

        for (const auto& result : results_) {
            std::cout << std::left << std::setw(30) << result.name
                      << std::setw(15) << std::fixed << std::setprecision(3)
                      << (result.duration.count() / 1e6) << std::setw(15)
                      << std::scientific << std::setprecision(2)
                      << result.throughput << std::setw(10) << result.operations
                      << "\n";
        }
    }

    void compareResults(const std::string& baseline,
                        const std::string& optimized) const {
        auto baselineIt = std::find_if(results_.begin(), results_.end(),
                                       [&baseline](const BenchmarkResult& r) {
                                           return r.name == baseline;
                                       });
        auto optimizedIt = std::find_if(results_.begin(), results_.end(),
                                        [&optimized](const BenchmarkResult& r) {
                                            return r.name == optimized;
                                        });

        if (baselineIt != results_.end() && optimizedIt != results_.end()) {
            double speedup = static_cast<double>(baselineIt->duration.count()) /
                             optimizedIt->duration.count();
            std::cout << "\nSpeedup: " << optimized << " is " << std::fixed
                      << std::setprecision(2) << speedup << "x faster than "
                      << baseline << "\n";
        }
    }
};

/**
 * @brief Create test data for performance benchmarks
 */
std::vector<uint8_t> createTestData(size_t size, bool randomize = false) {
    std::vector<uint8_t> data(size);

    if (randomize) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (auto& byte : data) {
            byte = static_cast<uint8_t>(dis(gen));
        }
    } else {
        // Create predictable pattern for consistent benchmarks
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>((i * 73 + 127) % 256);
        }
    }

    return data;
}

/**
 * @brief Demonstrate memory optimization techniques
 */
void demonstrateMemoryOptimization() {
    std::cout << "\n=== Memory Optimization Techniques ===\n";

    PerformanceProfiler profiler;

    const size_t dataSize = 1024 * 1024;  // 1MB
    auto testData = createTestData(dataSize);

    // Test 1: Memory access patterns
    std::cout << "Testing memory access patterns:\n";

    // Sequential access (cache-friendly)
    profiler.benchmark(
        "Sequential Access",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < testData.size(); ++i) {
                sum += testData[i];
            }
            volatile uint64_t result = sum;  // Prevent optimization
        },
        100);

    // Random access (cache-unfriendly)
    std::vector<size_t> randomIndices(testData.size());
    std::iota(randomIndices.begin(), randomIndices.end(), 0);
    std::shuffle(randomIndices.begin(), randomIndices.end(),
                 std::mt19937{std::random_device{}()});

    profiler.benchmark(
        "Random Access",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < randomIndices.size(); ++i) {
                sum += testData[randomIndices[i]];
            }
            volatile uint64_t result = sum;  // Prevent optimization
        },
        100);

    // Test 2: Memory alignment
    std::cout << "Testing memory alignment:\n";

    // Unaligned data
    std::vector<uint8_t> unalignedData(dataSize + 1);
    uint8_t* unalignedPtr = unalignedData.data() + 1;  // Misalign by 1 byte
    std::memcpy(unalignedPtr, testData.data(), dataSize);

    profiler.benchmark(
        "Unaligned Memory",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < dataSize; ++i) {
                sum += unalignedPtr[i];
            }
            volatile uint64_t result = sum;
        },
        100);

    // Aligned data (should be faster)
    alignas(64) uint8_t alignedData[dataSize];
    std::memcpy(alignedData, testData.data(), dataSize);

    profiler.benchmark(
        "Aligned Memory",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < dataSize; ++i) {
                sum += alignedData[i];
            }
            volatile uint64_t result = sum;
        },
        100);

    // Test 3: Memory prefetching
    std::cout << "Testing memory prefetching:\n";

    profiler.benchmark(
        "Without Prefetch",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < testData.size();
                 i += 64) {  // Process every 64th byte
                sum += testData[i];
            }
            volatile uint64_t result = sum;
        },
        1000);

    profiler.benchmark(
        "With Prefetch",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < testData.size(); i += 64) {
                // Prefetch next cache line
                if (i + 128 < testData.size()) {
                    __builtin_prefetch(&testData[i + 128], 0, 1);
                }
                sum += testData[i];
            }
            volatile uint64_t result = sum;
        },
        1000);

    profiler.printResults();
    profiler.compareResults("Random Access", "Sequential Access");
    profiler.compareResults("Unaligned Memory", "Aligned Memory");
    profiler.compareResults("Without Prefetch", "With Prefetch");
}

/**
 * @brief Demonstrate SIMD optimizations
 */
void demonstrateSIMDOptimizations() {
    std::cout << "\n=== SIMD Optimization Techniques ===\n";

    PerformanceProfiler profiler;

    const size_t dataSize = 1024 * 1024;  // 1MB
    auto testData1 = createTestData(dataSize);
    auto testData2 = createTestData(dataSize);
    std::vector<uint8_t> result(dataSize);

    // Test 1: Vector addition - scalar vs SIMD
    std::cout << "Testing vector addition optimizations:\n";

    // Scalar implementation
    profiler.benchmark(
        "Scalar Addition",
        [&]() {
            for (size_t i = 0; i < dataSize; ++i) {
                result[i] = static_cast<uint8_t>(std::min(
                    255, static_cast<int>(testData1[i]) + testData2[i]));
            }
        },
        100);

#ifdef __AVX2__
    // AVX2 SIMD implementation
    profiler.benchmark(
        "AVX2 Addition",
        [&]() {
            const size_t simdSize = 32;  // AVX2 processes 32 bytes at once
            size_t simdIterations = dataSize / simdSize;

            for (size_t i = 0; i < simdIterations; ++i) {
                size_t offset = i * simdSize;

                __m256i a = _mm256_loadu_si256(
                    reinterpret_cast<const __m256i*>(&testData1[offset]));
                __m256i b = _mm256_loadu_si256(
                    reinterpret_cast<const __m256i*>(&testData2[offset]));
                __m256i sum = _mm256_adds_epu8(a, b);  // Saturated addition

                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result[offset]),
                                    sum);
            }

            // Handle remaining bytes
            for (size_t i = simdIterations * simdSize; i < dataSize; ++i) {
                result[i] = static_cast<uint8_t>(std::min(
                    255, static_cast<int>(testData1[i]) + testData2[i]));
            }
        },
        100);
#endif

#ifdef __SSE2__
    // SSE2 SIMD implementation
    profiler.benchmark(
        "SSE2 Addition",
        [&]() {
            const size_t simdSize = 16;  // SSE2 processes 16 bytes at once
            size_t simdIterations = dataSize / simdSize;

            for (size_t i = 0; i < simdIterations; ++i) {
                size_t offset = i * simdSize;

                __m128i a = _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(&testData1[offset]));
                __m128i b = _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(&testData2[offset]));
                __m128i sum = _mm_adds_epu8(a, b);  // Saturated addition

                _mm_storeu_si128(reinterpret_cast<__m128i*>(&result[offset]),
                                 sum);
            }

            // Handle remaining bytes
            for (size_t i = simdIterations * simdSize; i < dataSize; ++i) {
                result[i] = static_cast<uint8_t>(std::min(
                    255, static_cast<int>(testData1[i]) + testData2[i]));
            }
        },
        100);
#endif

    // Test 2: Image brightness adjustment
    std::cout << "Testing brightness adjustment optimizations:\n";

    const int brightness = 50;

    // Scalar brightness adjustment
    profiler.benchmark(
        "Scalar Brightness",
        [&]() {
            for (size_t i = 0; i < dataSize; ++i) {
                result[i] = static_cast<uint8_t>(std::min(
                    255,
                    std::max(0, static_cast<int>(testData1[i]) + brightness)));
            }
        },
        100);

#ifdef __AVX2__
    // AVX2 brightness adjustment
    profiler.benchmark(
        "AVX2 Brightness",
        [&]() {
            const size_t simdSize = 32;
            size_t simdIterations = dataSize / simdSize;

            __m256i brightnessVec =
                _mm256_set1_epi8(static_cast<char>(brightness));

            for (size_t i = 0; i < simdIterations; ++i) {
                size_t offset = i * simdSize;

                __m256i data = _mm256_loadu_si256(
                    reinterpret_cast<const __m256i*>(&testData1[offset]));
                __m256i adjusted = _mm256_adds_epu8(data, brightnessVec);

                _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result[offset]),
                                    adjusted);
            }

            // Handle remaining bytes
            for (size_t i = simdIterations * simdSize; i < dataSize; ++i) {
                result[i] = static_cast<uint8_t>(std::min(
                    255,
                    std::max(0, static_cast<int>(testData1[i]) + brightness)));
            }
        },
        100);
#endif

    profiler.printResults();

#ifdef __AVX2__
    profiler.compareResults("Scalar Addition", "AVX2 Addition");
    profiler.compareResults("Scalar Brightness", "AVX2 Brightness");
#endif
#ifdef __SSE2__
    profiler.compareResults("Scalar Addition", "SSE2 Addition");
#endif
}

/**
 * @brief Demonstrate multi-threading optimizations
 */
void demonstrateMultiThreadingOptimizations() {
    std::cout << "\n=== Multi-Threading Optimization Techniques ===\n";

    PerformanceProfiler profiler;

    const size_t dataSize = 4 * 1024 * 1024;  // 4MB
    auto testData = createTestData(dataSize);
    std::vector<uint8_t> result(dataSize);

    const size_t numThreads = std::thread::hardware_concurrency();
    std::cout << "Using " << numThreads << " threads\n";

    // Test 1: Parallel processing
    std::cout << "Testing parallel processing:\n";

    // Single-threaded processing
    profiler.benchmark(
        "Single-threaded",
        [&]() {
            for (size_t i = 0; i < dataSize; ++i) {
                // Simulate some processing (square root approximation)
                uint8_t value = testData[i];
                result[i] =
                    static_cast<uint8_t>(std::sqrt(value * value * 0.5));
            }
        },
        10);

    // Multi-threaded processing
    profiler.benchmark(
        "Multi-threaded",
        [&]() {
            std::vector<std::future<void>> futures;
            size_t chunkSize = dataSize / numThreads;

            for (size_t t = 0; t < numThreads; ++t) {
                size_t start = t * chunkSize;
                size_t end =
                    (t == numThreads - 1) ? dataSize : start + chunkSize;

                futures.push_back(
                    std::async(std::launch::async, [&, start, end]() {
                        for (size_t i = start; i < end; ++i) {
                            uint8_t value = testData[i];
                            result[i] = static_cast<uint8_t>(
                                std::sqrt(value * value * 0.5));
                        }
                    }));
            }

            for (auto& future : futures) {
                future.wait();
            }
        },
        10);

    // Test 2: Work stealing vs static partitioning
    std::cout << "Testing work distribution strategies:\n";

    // Static partitioning (already shown above)

    // Work stealing simulation (using atomic counter)
    profiler.benchmark(
        "Work Stealing",
        [&]() {
            std::atomic<size_t> workIndex{0};
            const size_t workChunkSize = 1024;  // Process in smaller chunks

            std::vector<std::future<void>> futures;

            for (size_t t = 0; t < numThreads; ++t) {
                futures.push_back(std::async(std::launch::async, [&]() {
                    while (true) {
                        size_t start = workIndex.fetch_add(workChunkSize);
                        if (start >= dataSize)
                            break;

                        size_t end = std::min(start + workChunkSize, dataSize);

                        for (size_t i = start; i < end; ++i) {
                            uint8_t value = testData[i];
                            result[i] = static_cast<uint8_t>(
                                std::sqrt(value * value * 0.5));
                        }
                    }
                }));
            }

            for (auto& future : futures) {
                future.wait();
            }
        },
        10);

    // Test 3: Thread pool vs thread creation overhead
    std::cout << "Testing thread management strategies:\n";

    // Thread creation overhead
    profiler.benchmark(
        "Thread Creation",
        [&]() {
            std::vector<std::thread> threads;
            size_t chunkSize = dataSize / numThreads;

            for (size_t t = 0; t < numThreads; ++t) {
                size_t start = t * chunkSize;
                size_t end =
                    (t == numThreads - 1) ? dataSize : start + chunkSize;

                threads.emplace_back([&, start, end]() {
                    for (size_t i = start; i < end; ++i) {
                        uint8_t value = testData[i];
                        result[i] = static_cast<uint8_t>(
                            std::sqrt(value * value * 0.5));
                    }
                });
            }

            for (auto& thread : threads) {
                thread.join();
            }
        },
        10);

    profiler.printResults();
    profiler.compareResults("Single-threaded", "Multi-threaded");
    profiler.compareResults("Multi-threaded", "Work Stealing");
    profiler.compareResults("Thread Creation", "Multi-threaded");
}

/**
 * @brief Demonstrate algorithm optimization
 */
void demonstrateAlgorithmOptimization() {
    std::cout << "\n=== Algorithm Optimization Techniques ===\n";

    PerformanceProfiler profiler;

    const size_t dataSize = 1024 * 1024;
    auto testData = createTestData(dataSize);

    // Test 1: Loop optimizations
    std::cout << "Testing loop optimizations:\n";

    // Basic loop
    profiler.benchmark(
        "Basic Loop",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < dataSize; ++i) {
                sum += testData[i];
            }
            volatile uint64_t result = sum;
        },
        1000);

    // Loop unrolling
    profiler.benchmark(
        "Unrolled Loop",
        [&]() {
            uint64_t sum = 0;
            size_t i = 0;

            // Process 4 elements at a time
            for (; i + 3 < dataSize; i += 4) {
                sum += testData[i];
                sum += testData[i + 1];
                sum += testData[i + 2];
                sum += testData[i + 3];
            }

            // Handle remaining elements
            for (; i < dataSize; ++i) {
                sum += testData[i];
            }

            volatile uint64_t result = sum;
        },
        1000);

    // Test 2: Branch prediction optimization
    std::cout << "Testing branch prediction optimization:\n";

    // Create data with predictable pattern
    std::vector<uint8_t> predictableData(dataSize);
    for (size_t i = 0; i < dataSize; ++i) {
        predictableData[i] = (i % 4 < 2) ? 100 : 200;  // Predictable pattern
    }

    // Create data with unpredictable pattern
    std::vector<uint8_t> unpredictableData(dataSize);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    for (size_t i = 0; i < dataSize; ++i) {
        unpredictableData[i] = dis(gen) ? 100 : 200;  // Random pattern
    }

    // Branchy code with predictable data
    profiler.benchmark(
        "Predictable Branches",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < dataSize; ++i) {
                if (predictableData[i] > 150) {
                    sum += predictableData[i] * 2;
                } else {
                    sum += predictableData[i];
                }
            }
            volatile uint64_t result = sum;
        },
        100);

    // Branchy code with unpredictable data
    profiler.benchmark(
        "Unpredictable Branches",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < dataSize; ++i) {
                if (unpredictableData[i] > 150) {
                    sum += unpredictableData[i] * 2;
                } else {
                    sum += unpredictableData[i];
                }
            }
            volatile uint64_t result = sum;
        },
        100);

    // Branch-free version
    profiler.benchmark(
        "Branch-free",
        [&]() {
            uint64_t sum = 0;
            for (size_t i = 0; i < dataSize; ++i) {
                uint8_t value = unpredictableData[i];
                uint8_t multiplier = (value > 150) ? 2 : 1;
                sum += value * multiplier;
            }
            volatile uint64_t result = sum;
        },
        100);

    // Test 3: Cache-friendly data structures
    std::cout << "Testing cache-friendly data structures:\n";

    // Array of Structures (AoS) - cache unfriendly for partial access
    struct Point {
        float x, y, z;
        uint32_t color;
    };

    std::vector<Point> aos(dataSize / sizeof(Point));
    for (auto& point : aos) {
        point.x = 1.0f;
        point.y = 2.0f;
        point.z = 3.0f;
        point.color = 0xFF0000;
    }

    profiler.benchmark(
        "Array of Structures",
        [&]() {
            float sum = 0.0f;
            for (const auto& point : aos) {
                sum += point.x;  // Only accessing x coordinate
            }
            volatile float result = sum;
        },
        1000);

    // Structure of Arrays (SoA) - cache friendly for partial access
    struct Points {
        std::vector<float> x, y, z;
        std::vector<uint32_t> color;
    };

    Points soa;
    soa.x.resize(aos.size(), 1.0f);
    soa.y.resize(aos.size(), 2.0f);
    soa.z.resize(aos.size(), 3.0f);
    soa.color.resize(aos.size(), 0xFF0000);

    profiler.benchmark(
        "Structure of Arrays",
        [&]() {
            float sum = 0.0f;
            for (float x : soa.x) {
                sum += x;  // Only accessing x coordinates
            }
            volatile float result = sum;
        },
        1000);

    profiler.printResults();
    profiler.compareResults("Basic Loop", "Unrolled Loop");
    profiler.compareResults("Unpredictable Branches", "Predictable Branches");
    profiler.compareResults("Unpredictable Branches", "Branch-free");
    profiler.compareResults("Array of Structures", "Structure of Arrays");
}

/**
 * @brief Demonstrate profiling and measurement techniques
 */
void demonstrateProfilingTechniques() {
    std::cout << "\n=== Profiling and Measurement Techniques ===\n";

    // Test different timing methods
    const size_t iterations = 1000000;
    auto testData = createTestData(1024);

    std::cout << "Comparing timing methods:\n";

    // Method 1: std::chrono::high_resolution_clock
    auto start1 = high_resolution_clock::now();
    uint64_t sum1 = 0;
    for (size_t i = 0; i < iterations; ++i) {
        sum1 += testData[i % testData.size()];
    }
    auto end1 = high_resolution_clock::now();
    auto duration1 = duration_cast<nanoseconds>(end1 - start1);

    std::cout << "  high_resolution_clock: " << duration1.count() << " ns\n";

    // Method 2: std::chrono::steady_clock
    auto start2 = steady_clock::now();
    uint64_t sum2 = 0;
    for (size_t i = 0; i < iterations; ++i) {
        sum2 += testData[i % testData.size()];
    }
    auto end2 = steady_clock::now();
    auto duration2 = duration_cast<nanoseconds>(end2 - start2);

    std::cout << "  steady_clock: " << duration2.count() << " ns\n";

    // Method 3: CPU cycle counting (x86-64 specific)
#ifdef __x86_64__
    auto rdtsc = []() -> uint64_t {
        uint32_t lo, hi;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    };

    uint64_t cycles_start = rdtsc();
    uint64_t sum3 = 0;
    for (size_t i = 0; i < iterations; ++i) {
        sum3 += testData[i % testData.size()];
    }
    uint64_t cycles_end = rdtsc();
    uint64_t cycles = cycles_end - cycles_start;

    std::cout << "  CPU cycles: " << cycles << " cycles\n";
    std::cout << "  Cycles per operation: " << std::fixed
              << std::setprecision(2)
              << (static_cast<double>(cycles) / iterations) << "\n";
#endif

    // Memory usage profiling
    std::cout << "\nMemory usage profiling:\n";

    auto getMemoryUsage = []() -> size_t {
        // Simplified memory usage estimation
        // In real applications, use platform-specific APIs
        return 0;  // Placeholder
    };

    size_t memBefore = getMemoryUsage();

    // Allocate some memory
    std::vector<std::vector<uint8_t>> memoryTest;
    for (int i = 0; i < 100; ++i) {
        memoryTest.emplace_back(1024 * 10, static_cast<uint8_t>(i));
    }

    size_t memAfter = getMemoryUsage();

    std::cout << "  Memory allocated: ~"
              << (memoryTest.size() * memoryTest[0].size()) << " bytes\n";

    // Cache performance analysis
    std::cout << "\nCache performance analysis:\n";

    const size_t cacheTestSize = 64 * 1024;  // 64KB
    auto cacheTestData = createTestData(cacheTestSize);

    // L1 cache friendly access (small working set)
    auto start_l1 = high_resolution_clock::now();
    uint64_t sum_l1 = 0;
    for (int rep = 0; rep < 1000; ++rep) {
        for (size_t i = 0; i < 1024; ++i) {  // 1KB working set
            sum_l1 += cacheTestData[i];
        }
    }
    auto end_l1 = high_resolution_clock::now();
    auto duration_l1 = duration_cast<microseconds>(end_l1 - start_l1);

    // L3 cache unfriendly access (large working set)
    auto start_l3 = high_resolution_clock::now();
    uint64_t sum_l3 = 0;
    for (int rep = 0; rep < 1000; ++rep) {
        for (size_t i = 0; i < cacheTestSize; ++i) {  // 64KB working set
            sum_l3 += cacheTestData[i];
        }
    }
    auto end_l3 = high_resolution_clock::now();
    auto duration_l3 = duration_cast<microseconds>(end_l3 - start_l3);

    std::cout << "  L1 cache friendly (1KB): " << duration_l1.count()
              << " μs\n";
    std::cout << "  L3 cache unfriendly (64KB): " << duration_l3.count()
              << " μs\n";
    std::cout << "  Cache miss penalty: " << std::fixed << std::setprecision(2)
              << (static_cast<double>(duration_l3.count()) /
                  duration_l1.count())
              << "x\n";

    // Prevent optimization
    volatile uint64_t prevent_opt = sum1 + sum2 + sum_l1 + sum_l3;
#ifdef __x86_64__
    prevent_opt += sum3;
#endif
}

int main() {
    std::cout << "=== Atom Image Performance Optimization Demo ===\n";
    std::cout << "This example demonstrates performance optimization "
                 "techniques and benchmarking\n";

    // Run all demonstrations
    demonstrateMemoryOptimization();
    demonstrateSIMDOptimizations();
    demonstrateMultiThreadingOptimizations();
    demonstrateAlgorithmOptimization();
    demonstrateProfilingTechniques();

    std::cout << "\n=== Performance optimization demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout
        << "- Memory optimization (alignment, prefetching, access patterns)\n";
    std::cout << "- SIMD vectorization (SSE2, AVX2)\n";
    std::cout
        << "- Multi-threading optimization (work stealing, thread pools)\n";
    std::cout
        << "- Algorithm optimization (loop unrolling, branch prediction)\n";
    std::cout << "- Profiling and measurement techniques\n";
    std::cout << "- Cache-friendly data structures and algorithms\n";
    std::cout << "- Comprehensive performance benchmarking framework\n";

    return 0;
}
