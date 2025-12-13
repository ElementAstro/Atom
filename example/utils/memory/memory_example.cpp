/**
 * @file memory_example.cpp
 * @brief Examples for atom::utils memory utilities
 */

#include "atom/utils/memory/aligned.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateAlignedAllocation() {
    printSection("1. Aligned Memory Allocation");

    std::cout << "Aligned allocation for SIMD operations:" << std::endl;

    constexpr size_t alignment = 64;  // Cache line alignment
    constexpr size_t count = 16;

    std::cout << "  Alignment: " << alignment << " bytes" << std::endl;
    std::cout << "  Element count: " << count << std::endl;

    // Demonstrate alignment concepts
    std::cout << "\nAlignment requirements:" << std::endl;
    std::cout << "  SSE: 16 bytes" << std::endl;
    std::cout << "  AVX: 32 bytes" << std::endl;
    std::cout << "  AVX-512: 64 bytes" << std::endl;
    std::cout << "  Cache line: 64 bytes (typical)" << std::endl;
}

void demonstrateMemoryPools() {
    printSection("2. Memory Pool Concepts");

    std::cout << "Memory pool benefits:" << std::endl;
    std::cout << "  - Reduced allocation overhead" << std::endl;
    std::cout << "  - Better cache locality" << std::endl;
    std::cout << "  - Predictable memory usage" << std::endl;
    std::cout << "  - Faster allocation/deallocation" << std::endl;

    std::cout << "\nTypical usage pattern:" << std::endl;
    std::cout << R"(
    // Pre-allocate pool
    MemoryPool<MyObject, 1000> pool;

    // Allocate from pool (fast)
    auto* obj = pool.allocate();

    // Use object...

    // Return to pool (fast)
    pool.deallocate(obj);
    )" << std::endl;
}

void demonstrateSIMDWrapper() {
    printSection("3. SIMD Wrapper Concepts");

    std::cout << "SIMD (Single Instruction, Multiple Data):" << std::endl;
    std::cout << "  Process multiple data elements in parallel" << std::endl;

    std::cout << "\nSupported operations:" << std::endl;
    std::cout << "  - Vector addition" << std::endl;
    std::cout << "  - Vector multiplication" << std::endl;
    std::cout << "  - Dot product" << std::endl;
    std::cout << "  - Min/Max operations" << std::endl;
    std::cout << "  - Horizontal sum" << std::endl;

    std::cout << "\nExample SIMD usage:" << std::endl;
    std::cout << R"(
    // Load 4 floats at once (SSE)
    __m128 a = _mm_load_ps(data1);
    __m128 b = _mm_load_ps(data2);

    // Add all 4 pairs simultaneously
    __m128 result = _mm_add_ps(a, b);

    // Store result
    _mm_store_ps(output, result);
    )" << std::endl;
}

void demonstrateMemoryLeakDetection() {
    printSection("4. Memory Leak Detection");

    std::cout << "Leak detection strategies:" << std::endl;
    std::cout << "  - Track all allocations" << std::endl;
    std::cout << "  - Match allocations with deallocations" << std::endl;
    std::cout << "  - Report unfreed memory at exit" << std::endl;

    std::cout << "\nUsage example:" << std::endl;
    std::cout << R"(
    // Enable leak detection
    LeakDetector::enable();

    // Allocate memory
    int* ptr = new int[100];

    // Forget to delete...

    // At program exit, detector reports:
    // "Memory leak: 400 bytes at 0x12345678"
    // "Allocated at: file.cpp:42"
    )" << std::endl;

    std::cout << "\nBest practices:" << std::endl;
    std::cout << "  - Use smart pointers (unique_ptr, shared_ptr)" << std::endl;
    std::cout << "  - Use RAII for resource management" << std::endl;
    std::cout << "  - Run leak detection in debug builds" << std::endl;
}

void demonstrateCacheOptimization() {
    printSection("5. Cache Optimization");

    std::cout << "Cache-friendly data structures:" << std::endl;

    std::cout << "\n--- Array of Structures (AoS) ---" << std::endl;
    std::cout << R"(
    struct Particle {
        float x, y, z;      // Position
        float vx, vy, vz;   // Velocity
    };
    Particle particles[1000];
    // Memory: [x,y,z,vx,vy,vz][x,y,z,vx,vy,vz]...
    )" << std::endl;

    std::cout << "\n--- Structure of Arrays (SoA) ---" << std::endl;
    std::cout << R"(
    struct Particles {
        float x[1000], y[1000], z[1000];
        float vx[1000], vy[1000], vz[1000];
    };
    // Memory: [x,x,x...][y,y,y...][z,z,z...]...
    // Better for SIMD and cache when processing one field
    )" << std::endl;

    std::cout << "\nCache line considerations:" << std::endl;
    std::cout << "  - Typical cache line: 64 bytes" << std::endl;
    std::cout << "  - Align hot data to cache lines" << std::endl;
    std::cout << "  - Avoid false sharing in multithreaded code" << std::endl;
}

void demonstrateSmartPointers() {
    printSection("6. Smart Pointer Usage");

    std::cout << "--- unique_ptr ---" << std::endl;
    {
        auto ptr = std::make_unique<int>(42);
        std::cout << "  Created unique_ptr with value: " << *ptr << std::endl;
    }
    std::cout << "  Automatically deleted when out of scope" << std::endl;

    std::cout << "\n--- shared_ptr ---" << std::endl;
    std::shared_ptr<int> shared1;
    {
        auto shared2 = std::make_shared<int>(100);
        shared1 = shared2;
        std::cout << "  Reference count: " << shared1.use_count() << std::endl;
    }
    std::cout << "  After inner scope, count: " << shared1.use_count() << std::endl;

    std::cout << "\n--- weak_ptr ---" << std::endl;
    std::cout << "  Used to break circular references" << std::endl;
    std::cout << "  Does not contribute to reference count" << std::endl;
}

void demonstrateMemoryBenchmark() {
    printSection("7. Memory Access Patterns");

    std::cout << "Sequential vs Random access:" << std::endl;

    const size_t size = 1000000;
    std::vector<int> data(size);

    std::cout << "\n--- Sequential Access ---" << std::endl;
    std::cout << "  for (int i = 0; i < N; ++i) sum += data[i];" << std::endl;
    std::cout << "  Cache-friendly: prefetcher works well" << std::endl;

    std::cout << "\n--- Random Access ---" << std::endl;
    std::cout << "  for (int i = 0; i < N; ++i) sum += data[random[i]];" << std::endl;
    std::cout << "  Cache-unfriendly: many cache misses" << std::endl;

    std::cout << "\nPerformance difference: 10-100x slower for random access" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Memory Utilities Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateAlignedAllocation();
        demonstrateMemoryPools();
        demonstrateSIMDWrapper();
        demonstrateMemoryLeakDetection();
        demonstrateCacheOptimization();
        demonstrateSmartPointers();
        demonstrateMemoryBenchmark();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All memory examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
