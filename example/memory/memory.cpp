/**
 * @file memory_example.cpp
 * @brief Comprehensive examples of using the advanced MemoryPool class
 * @author Example Author
 * @date 2025-03-23
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>

#include <string>
#include <thread>
#include <vector>

#include "atom/memory/memory.hpp"

using namespace atom::memory;

// Helper function to print section titlesvoid printSection(const std::string&
// title) {
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n";
}

// Custom block size strategy for demonstrationclass LinearBlockSizeStrategy :
// public atom::memory::BlockSizeStrategy {
public:
explicit LinearBlockSizeStrategy(size_t increment = 1024)
    : increment_(increment) {}

[[nodiscard]] size_t calculate(size_t requested_size) const noexcept override {
    return requested_size + increment_;
}

private:
size_t increment_;
}
;

// Test class for memory pool allocationsclass TestData {
public:
TestData() : id_(0), value_(0.0), data_(256, 0) {
    std::cout << "TestData default constructed" << std::endl;
}

TestData(int id, double value)
    : id_(id), value_(value), data_(256, static_cast<char>(id % 256)) {
    std::cout << "TestData constructed: ID=" << id_ << ", Value=" << value_
              << std::endl;
}

~TestData() { std::cout << "TestData destroyed: ID=" << id_ << std::endl; }

int getId() const { return id_; }
double getValue() const { return value_; }
const std::vector<char>& getData() const { return data_; }

void setValue(double value) { value_ = value; }
void setData(char fill) { std::fill(data_.begin(), data_.end(), fill); }

private:
int id_;
double value_;
std::vector<char> data_;
}
;

// Helper function to measure execution timetemplate <typename Func>
double measureTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

int main() {
    std::cout << "ADVANCED MEMORY POOL COMPREHENSIVE EXAMPLES\n";
    std::cout << "==========================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Memory Pool Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Memory Pool Usage");

    // Create a memory pool for TestData objects
    MemoryPool<TestData> basicPool;

    std::cout << "Created memory pool for TestData objects" << std::endl;
    std::cout << "Initial stats:" << std::endl;
    std::cout << "  Total allocated: " << basicPool.getTotalAllocated()
              << " bytes" << std::endl;
    std::cout << "  Total available: " << basicPool.getTotalAvailable()
              << " bytes" << std::endl;
    std::cout << "  Allocation count: " << basicPool.getAllocationCount()
              << std::endl;

    // Allocate some objects
    std::cout << "\nAllocating objects from the pool..." << std::endl;
    TestData* obj1 = basicPool.allocate(1);
    new (obj1) TestData(1, 3.14);

    TestData* obj2 = basicPool.allocate(1);
    new (obj2) TestData(2, 2.71);

    TestData* obj3 = basicPool.allocate(1);
    new (obj3) TestData(3, 1.41);

    std::cout << "\nAfter allocations:" << std::endl;
    std::cout << "  Total allocated: " << basicPool.getTotalAllocated()
              << " bytes" << std::endl;
    std::cout << "  Total available: " << basicPool.getTotalAvailable()
              << " bytes" << std::endl;
    std::cout << "  Allocation count: " << basicPool.getAllocationCount()
              << std::endl;
    std::cout << "  Fragmentation ratio: " << std::fixed << std::setprecision(3)
              << basicPool.getFragmentationRatio() << std::endl;

    // Use the objects
    std::cout << "\nUsing allocated objects:" << std::endl;
    std::cout << "  Object 1: ID=" << obj1->getId()
              << ", Value=" << obj1->getValue() << std::endl;
    std::cout << "  Object 2: ID=" << obj2->getId()
              << ", Value=" << obj2->getValue() << std::endl;
    std::cout << "  Object 3: ID=" << obj3->getId()
              << ", Value=" << obj3->getValue() << std::endl;

    // Deallocate objects
    std::cout << "\nDeallocating objects..." << std::endl;
    obj1->~TestData();
    basicPool.deallocate(obj1, 1);

    obj2->~TestData();
    basicPool.deallocate(obj2, 1);

    obj3->~TestData();
    basicPool.deallocate(obj3, 1);

    std::cout << "After deallocations:" << std::endl;
    std::cout << "  Total allocated: " << basicPool.getTotalAllocated()
              << " bytes" << std::endl;
    std::cout << "  Deallocation count: " << basicPool.getDeallocationCount()
              << std::endl;
    std::cout << "  Fragmentation ratio: " << std::fixed << std::setprecision(3)
              << basicPool.getFragmentationRatio() << std::endl;

    //--------------------------------------------------------------------------
    // 2. Tagged Allocations
    //--------------------------------------------------------------------------
    printSection("2. Tagged Allocations");

    // Create a new pool for tagged allocation demonstration
    MemoryPool<int> taggedPool;

    std::cout << "Demonstrating tagged allocations for memory tracking..."
              << std::endl;

    // Allocate memory with tags
    int* array1 = taggedPool.allocateTagged(100, "Array1", __FILE__, __LINE__);
    std::cout << "Allocated array1 with 100 integers, tagged as 'Array1'"
              << std::endl;

    int* array2 = taggedPool.allocateTagged(200, "Array2", __FILE__, __LINE__);
    std::cout << "Allocated array2 with 200 integers, tagged as 'Array2'"
              << std::endl;

    int* array3 =
        taggedPool.allocateTagged(50, "TempArray", __FILE__, __LINE__);
    std::cout << "Allocated array3 with 50 integers, tagged as 'TempArray'"
              << std::endl;

    // Initialize arrays
    for (int i = 0; i < 100; ++i)
        array1[i] = i;
    for (int i = 0; i < 200; ++i)
        array2[i] = i * 2;
    for (int i = 0; i < 50; ++i)
        array3[i] = i * 3;

    // Find tags for specific pointers
    std::cout << "\nLooking up tags for allocated pointers:" << std::endl;
    auto tag1 = taggedPool.findTag(array1);
    if (tag1) {
        std::cout << "  array1 tag: " << tag1->name << " (from " << tag1->file
                  << ":" << tag1->line << ")" << std::endl;
    }

    auto tag2 = taggedPool.findTag(array2);
    if (tag2) {
        std::cout << "  array2 tag: " << tag2->name << " (from " << tag2->file
                  << ":" << tag2->line << ")" << std::endl;
    }

    // Get all tagged allocations
    std::cout << "\nAll tagged allocations:" << std::endl;
    auto allTags = taggedPool.getTaggedAllocations();
    for (const auto& [ptr, tag] : allTags) {
        std::cout << "  Pointer " << ptr << ": " << tag.name << " (from "
                  << tag.file << ":" << tag.line << ")" << std::endl;
    }

    // Deallocate tagged memory
    std::cout << "\nDeallocating tagged memory..." << std::endl;
    taggedPool.deallocate(array1, 100);
    taggedPool.deallocate(array2, 200);
    taggedPool.deallocate(array3, 50);

    // Verify tags are removed
    std::cout << "Remaining tagged allocations: "
              << taggedPool.getTaggedAllocations().size() << std::endl;

    //--------------------------------------------------------------------------
    // 3. Custom Block Size Strategies
    //--------------------------------------------------------------------------
    printSection("3. Custom Block Size Strategies");

    // Create pools with different block size strategies
    std::cout << "Testing different block size strategies..." << std::endl;

    // Exponential growth strategy (default)
    auto exponentialPool = std::make_unique<MemoryPool<char>>(
        std::make_unique<atom::memory::ExponentialBlockSizeStrategy>(1.5));

    // Linear growth strategy (custom)
    auto linearPool = std::make_unique<MemoryPool<char>>(
        std::make_unique<LinearBlockSizeStrategy>(2048));

    std::cout << "\nTesting exponential growth strategy (1.5x growth):"
              << std::endl;
    std::vector<char*> exponentialPtrs;
    for (int i = 0; i < 5; ++i) {
        size_t size = 1000 + i * 500;
        char* ptr = exponentialPool->allocate(size);
        exponentialPtrs.push_back(ptr);
        std::cout << "  Allocated " << size << " bytes, total available: "
                  << exponentialPool->getTotalAvailable() << " bytes"
                  << std::endl;
    }

    std::cout << "\nTesting linear growth strategy (+2048 bytes):" << std::endl;
    std::vector<char*> linearPtrs;
    for (int i = 0; i < 5; ++i) {
        size_t size = 1000 + i * 500;
        char* ptr = linearPool->allocate(size);
        linearPtrs.push_back(ptr);
        std::cout << "  Allocated " << size << " bytes, total available: "
                  << linearPool->getTotalAvailable() << " bytes" << std::endl;
    }

    // Clean up
    for (size_t i = 0; i < exponentialPtrs.size(); ++i) {
        exponentialPool->deallocate(exponentialPtrs[i], 1000 + i * 500);
    }
    for (size_t i = 0; i < linearPtrs.size(); ++i) {
        linearPool->deallocate(linearPtrs[i], 1000 + i * 500);
    }

    //--------------------------------------------------------------------------
    // 4. Memory Compaction and Fragmentation
    //--------------------------------------------------------------------------
    printSection("4. Memory Compaction and Fragmentation");

    MemoryPool<int> fragmentationPool;

    std::cout << "Demonstrating memory fragmentation and compaction..."
              << std::endl;

    // Allocate several blocks
    std::vector<int*> blocks;
    for (int i = 0; i < 10; ++i) {
        int* block = fragmentationPool.allocate(100 + i * 50);
        blocks.push_back(block);
        std::cout << "Allocated block " << i << " with " << (100 + i * 50)
                  << " integers" << std::endl;
    }

    std::cout << "\nAfter allocations:" << std::endl;
    std::cout << "  Fragmentation ratio: " << std::fixed << std::setprecision(3)
              << fragmentationPool.getFragmentationRatio() << std::endl;

    // Deallocate every other block to create fragmentation
    std::cout << "\nDeallocating every other block to create fragmentation..."
              << std::endl;
    for (size_t i = 1; i < blocks.size(); i += 2) {
        fragmentationPool.deallocate(blocks[i], 100 + i * 50);
        blocks[i] = nullptr;
        std::cout << "Deallocated block " << i << std::endl;
    }

    std::cout << "\nAfter creating fragmentation:" << std::endl;
    std::cout << "  Fragmentation ratio: " << std::fixed << std::setprecision(3)
              << fragmentationPool.getFragmentationRatio() << std::endl;

    // Compact the memory pool
    std::cout << "\nCompacting memory pool..." << std::endl;
    size_t compacted_bytes = fragmentationPool.compact();
    std::cout << "Compacted " << compacted_bytes << " bytes" << std::endl;

    std::cout << "After compaction:" << std::endl;
    std::cout << "  Fragmentation ratio: " << std::fixed << std::setprecision(3)
              << fragmentationPool.getFragmentationRatio() << std::endl;

    // Clean up remaining blocks
    for (size_t i = 0; i < blocks.size(); i += 2) {
        if (blocks[i]) {
            fragmentationPool.deallocate(blocks[i], 100 + i * 50);
        }
    }

    //--------------------------------------------------------------------------
    // 5. Memory Pool as PMR Resource
    //--------------------------------------------------------------------------
    printSection("5. Memory Pool as PMR Resource");

    std::cout << "Using MemoryPool as a polymorphic memory resource..."
              << std::endl;

    // Create a memory pool and use it as a PMR resource
    MemoryPool<std::byte> pmrPool;
    std::pmr::memory_resource* resource = &pmrPool;

    // Use with PMR containers
    std::pmr::vector<int> pmrVector(resource);
    std::pmr::string pmrString("Hello from PMR!", resource);

    std::cout << "Created PMR vector and string using custom memory pool"
              << std::endl;

    // Add elements to demonstrate usage
    for (int i = 0; i < 100; ++i) {
        pmrVector.push_back(i * i);
    }

    pmrString += " Extended with more text to trigger allocations.";

    std::cout << "PMR vector size: " << pmrVector.size() << std::endl;
    std::cout << "PMR string: " << pmrString << std::endl;
    std::cout << "Pool stats after PMR usage:" << std::endl;
    std::cout << "  Total allocated: " << pmrPool.getTotalAllocated()
              << " bytes" << std::endl;
    std::cout << "  Allocation count: " << pmrPool.getAllocationCount()
              << std::endl;

    //--------------------------------------------------------------------------
    // 6. Performance Comparison
    //--------------------------------------------------------------------------
    printSection("6. Performance Comparison");

    const int numAllocations = 10000;
    const size_t allocationSize = 64;

    std::cout << "Comparing performance: MemoryPool vs standard allocator"
              << std::endl;
    std::cout << "Number of allocations: " << numAllocations << std::endl;
    std::cout << "Allocation size: " << allocationSize << " bytes each"
              << std::endl;

    // Test standard allocator
    std::cout << "\nTesting standard allocator..." << std::endl;
    double stdTime = measureTime([&]() {
        std::vector<char*> ptrs;
        ptrs.reserve(numAllocations);

        // Allocate
        for (int i = 0; i < numAllocations; ++i) {
            ptrs.push_back(new char[allocationSize]);
        }

        // Deallocate
        for (char* ptr : ptrs) {
            delete[] ptr;
        }
    });

    std::cout << "Standard allocator time: " << std::fixed
              << std::setprecision(3) << stdTime << " ms" << std::endl;

    // Test memory pool
    std::cout << "\nTesting MemoryPool..." << std::endl;
    double poolTime = measureTime([&]() {
        MemoryPool<char> perfPool;
        std::vector<char*> ptrs;
        ptrs.reserve(numAllocations);

        // Allocate
        for (int i = 0; i < numAllocations; ++i) {
            ptrs.push_back(perfPool.allocate(allocationSize));
        }

        // Deallocate
        for (char* ptr : ptrs) {
            perfPool.deallocate(ptr, allocationSize);
        }
    });

    std::cout << "MemoryPool time: " << std::fixed << std::setprecision(3)
              << poolTime << " ms" << std::endl;

    double speedup = stdTime / poolTime;
    std::cout << "\nSpeedup: " << std::fixed << std::setprecision(2) << speedup
              << "x faster" << std::endl;

    //--------------------------------------------------------------------------
    // 7. Thread Safety Demonstration
    //--------------------------------------------------------------------------
    printSection("7. Thread Safety Demonstration");

    std::cout << "Testing thread safety with concurrent allocations..."
              << std::endl;

    MemoryPool<int> threadSafePool;
    const int numThreads = 4;
    const int allocationsPerThread = 1000;

    std::vector<std::thread> threads;
    std::vector<std::vector<int*>> threadAllocations(numThreads);

    // Launch threads
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            std::cout << "Thread " << t << " starting allocations..."
                      << std::endl;

            for (int i = 0; i < allocationsPerThread; ++i) {
                int* ptr = threadSafePool.allocate(10 + (i % 100));
                threadAllocations[t].push_back(ptr);

                // Initialize memory
                for (int j = 0; j < 10 + (i % 100); ++j) {
                    ptr[j] = t * 1000 + i;
                }

                // Small delay to increase contention
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }

            std::cout << "Thread " << t << " completed allocations"
                      << std::endl;
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "\nAll threads completed. Pool statistics:" << std::endl;
    std::cout << "  Total allocated: " << threadSafePool.getTotalAllocated()
              << " bytes" << std::endl;
    std::cout << "  Allocation count: " << threadSafePool.getAllocationCount()
              << std::endl;

    // Clean up allocations
    std::cout << "\nCleaning up thread allocations..." << std::endl;
    for (int t = 0; t < numThreads; ++t) {
        for (size_t i = 0; i < threadAllocations[t].size(); ++i) {
            threadSafePool.deallocate(threadAllocations[t][i], 10 + (i % 100));
        }
    }

    std::cout << "Final deallocation count: "
              << threadSafePool.getDeallocationCount() << std::endl;

    //--------------------------------------------------------------------------
    // 8. Memory Reservation and Optimization
    //--------------------------------------------------------------------------
    printSection("8. Memory Reservation and Optimization");

    std::cout << "Demonstrating memory reservation for optimal performance..."
              << std::endl;

    MemoryPool<double> reservationPool;

    // Check initial state
    std::cout << "Initial pool state:" << std::endl;
    std::cout << "  Available memory: " << reservationPool.getTotalAvailable()
              << " bytes" << std::endl;

    // Reserve memory for expected allocations
    const size_t expectedAllocations = 1000;
    const size_t avgSize = sizeof(double) * 50;  // 50 doubles per allocation

    std::cout << "\nReserving memory for " << expectedAllocations
              << " allocations of " << avgSize << " bytes each..." << std::endl;

    reservationPool.reserve(expectedAllocations, avgSize);

    std::cout << "After reservation:" << std::endl;
    std::cout << "  Available memory: " << reservationPool.getTotalAvailable()
              << " bytes" << std::endl;

    // Perform the expected allocations
    std::cout << "\nPerforming expected allocations..." << std::endl;
    std::vector<double*> reservedPtrs;
    reservedPtrs.reserve(expectedAllocations);

    double reservedTime = measureTime([&]() {
        for (size_t i = 0; i < expectedAllocations; ++i) {
            double* ptr = reservationPool.allocate(50);
            reservedPtrs.push_back(ptr);

            // Initialize with some data
            for (int j = 0; j < 50; ++j) {
                ptr[j] = static_cast<double>(i * 50 + j);
            }
        }
    });

    std::cout << "Reserved allocation time: " << std::fixed
              << std::setprecision(3) << reservedTime << " ms" << std::endl;

    // Compare with non-reserved pool
    std::cout << "\nComparing with non-reserved pool..." << std::endl;
    MemoryPool<double> nonReservedPool;
    std::vector<double*> nonReservedPtrs;
    nonReservedPtrs.reserve(expectedAllocations);

    double nonReservedTime = measureTime([&]() {
        for (size_t i = 0; i < expectedAllocations; ++i) {
            double* ptr = nonReservedPool.allocate(50);
            nonReservedPtrs.push_back(ptr);

            // Initialize with some data
            for (int j = 0; j < 50; ++j) {
                ptr[j] = static_cast<double>(i * 50 + j);
            }
        }
    });

    std::cout << "Non-reserved allocation time: " << std::fixed
              << std::setprecision(3) << nonReservedTime << " ms" << std::endl;

    double reservationSpeedup = nonReservedTime / reservedTime;
    std::cout << "Reservation speedup: " << std::fixed << std::setprecision(2)
              << reservationSpeedup << "x faster" << std::endl;

    // Clean up
    for (double* ptr : reservedPtrs) {
        reservationPool.deallocate(ptr, 50);
    }
    for (double* ptr : nonReservedPtrs) {
        nonReservedPool.deallocate(ptr, 50);
    }

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout << "This example demonstrated the following MemoryPool features:"
              << std::endl;
    std::cout << "  1. Basic memory allocation and deallocation" << std::endl;
    std::cout << "  2. Tagged allocations for memory tracking" << std::endl;
    std::cout << "  3. Custom block size strategies (exponential vs linear)"
              << std::endl;
    std::cout << "  4. Memory compaction and fragmentation analysis"
              << std::endl;
    std::cout << "  5. Integration with PMR (Polymorphic Memory Resources)"
              << std::endl;
    std::cout << "  6. Performance comparison with standard allocators"
              << std::endl;
    std::cout << "  7. Thread safety in concurrent environments" << std::endl;
    std::cout << "  8. Memory reservation for performance optimization"
              << std::endl;

    std::cout << "\nThe MemoryPool provides significant performance benefits"
              << std::endl;
    std::cout
        << "and advanced features for memory management in C++ applications."
        << std::endl;

    return 0;
}
