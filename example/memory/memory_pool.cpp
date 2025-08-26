/**
 * @file memory_pool_example.cpp
 * @brief Comprehensive examples of using the fixed-size MemoryPool class
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "atom/memory/memory_pool.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Test class for memory pool allocations
class TestObject {
public:
    TestObject() : id_(0), value_(0.0) {
        std::cout << "TestObject default constructed" << std::endl;
    }

    TestObject(int id, double value) : id_(id), value_(value) {
        std::cout << "TestObject constructed: ID=" << id_
                  << ", Value=" << value_ << std::endl;
    }

    ~TestObject() {
        std::cout << "TestObject destroyed: ID=" << id_ << std::endl;
    }

    int getId() const { return id_; }
    double getValue() const { return value_; }
    void setValue(double value) { value_ = value; }

private:
    int id_;
    double value_;
};

// Helper function to measure execution time
template <typename Func>
double measureTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

int main() {
    std::cout << "FIXED-SIZE MEMORY POOL COMPREHENSIVE EXAMPLES\n";
    std::cout << "============================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Memory Pool Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Memory Pool Usage");

    // Create a memory pool with 64-byte blocks and 100 blocks per chunk
    constexpr size_t BlockSize = 64;
    constexpr size_t BlocksPerChunk = 100;
    atom::memory::MemoryPool<BlockSize, BlocksPerChunk> basicPool;

    std::cout << "Created memory pool with:" << std::endl;
    std::cout << "  Block size: " << BlockSize << " bytes" << std::endl;
    std::cout << "  Blocks per chunk: " << BlocksPerChunk << std::endl;
    std::cout << "  Total chunk size: " << (BlockSize * BlocksPerChunk)
              << " bytes" << std::endl;

    // Allocate some blocks
    std::cout << "\nAllocating blocks from the pool..." << std::endl;
    std::vector<void*> allocatedBlocks;

    for (int i = 0; i < 10; ++i) {
        void* block = basicPool.allocate();
        if (block) {
            allocatedBlocks.push_back(block);
            std::cout << "Allocated block " << i << " at address " << block
                      << std::endl;

            // Initialize the block with some data
            std::memset(block, i + 1, BlockSize);
        } else {
            std::cout << "Failed to allocate block " << i << std::endl;
        }
    }

    std::cout << "\nPool statistics after allocations:" << std::endl;
    auto [allocated, total] = basicPool.get_stats();
    std::cout << "  Allocated blocks: " << allocated << std::endl;
    std::cout << "  Available blocks: " << (total - allocated) << std::endl;
    std::cout << "  Total blocks: " << total << std::endl;

    // Verify data integrity
    std::cout << "\nVerifying data integrity..." << std::endl;
    for (size_t i = 0; i < allocatedBlocks.size(); ++i) {
        auto* data = static_cast<unsigned char*>(allocatedBlocks[i]);
        bool dataValid = true;
        for (size_t j = 0; j < BlockSize; ++j) {
            if (data[j] != static_cast<unsigned char>(i + 1)) {
                dataValid = false;
                break;
            }
        }
        std::cout << "Block " << i
                  << " data integrity: " << (dataValid ? "PASSED" : "FAILED")
                  << std::endl;
    }

    // Deallocate some blocks
    std::cout << "\nDeallocating every other block..." << std::endl;
    for (size_t i = 1; i < allocatedBlocks.size(); i += 2) {
        basicPool.deallocate(allocatedBlocks[i]);
        allocatedBlocks[i] = nullptr;
        std::cout << "Deallocated block " << i << std::endl;
    }

    std::cout << "\nPool statistics after partial deallocation:" << std::endl;
    auto [allocated2, total2] = basicPool.get_stats();
    std::cout << "  Allocated blocks: " << allocated2 << std::endl;
    std::cout << "  Available blocks: " << (total2 - allocated2) << std::endl;

    // Clean up remaining blocks
    for (void* block : allocatedBlocks) {
        if (block) {
            basicPool.deallocate(block);
        }
    }

    //--------------------------------------------------------------------------
    // 2. Object Pool Usage
    //--------------------------------------------------------------------------
    printSection("2. Object Pool Usage");

    // Create an object pool for TestObject
    atom::memory::SimpleObjectPool<TestObject> objectPool;

    std::cout << "Created object pool for TestObject" << std::endl;

    // Allocate and construct objects
    std::cout << "\nAllocating and constructing objects..." << std::endl;
    std::vector<TestObject*> objects;

    for (int i = 0; i < 5; ++i) {
        TestObject* obj = objectPool.allocate();
        if (obj) {
            new (obj) TestObject(i, i * 3.14);
            objects.push_back(obj);
        }
    }

    // Use the objects
    std::cout << "\nUsing allocated objects:" << std::endl;
    for (size_t i = 0; i < objects.size(); ++i) {
        std::cout << "Object " << i << ": ID=" << objects[i]->getId()
                  << ", Value=" << objects[i]->getValue() << std::endl;
    }

    // Destroy and deallocate objects
    std::cout << "\nDestroying and deallocating objects..." << std::endl;
    for (TestObject* obj : objects) {
        obj->~TestObject();
        objectPool.deallocate(obj);
    }

    //--------------------------------------------------------------------------
    // 3. Performance Comparison
    //--------------------------------------------------------------------------
    printSection("3. Performance Comparison");

    const int numAllocations = 100000;
    std::cout << "Comparing performance with " << numAllocations
              << " allocations" << std::endl;

    // Test standard allocator
    std::cout << "\nTesting standard allocator..." << std::endl;
    double stdTime = measureTime([&]() {
        std::vector<void*> ptrs;
        ptrs.reserve(numAllocations);

        // Allocate
        for (int i = 0; i < numAllocations; ++i) {
            ptrs.push_back(std::malloc(BlockSize));
        }

        // Deallocate
        for (void* ptr : ptrs) {
            std::free(ptr);
        }
    });

    std::cout << "Standard allocator time: " << std::fixed
              << std::setprecision(3) << stdTime << " ms" << std::endl;

    // Test memory pool
    std::cout << "\nTesting MemoryPool..." << std::endl;
    double poolTime = measureTime([&]() {
        atom::memory::MemoryPool<BlockSize, BlocksPerChunk> perfPool;
        std::vector<void*> ptrs;
        ptrs.reserve(numAllocations);

        // Allocate
        for (int i = 0; i < numAllocations; ++i) {
            ptrs.push_back(perfPool.allocate());
        }

        // Deallocate
        for (void* ptr : ptrs) {
            perfPool.deallocate(ptr);
        }
    });

    std::cout << "MemoryPool time: " << std::fixed << std::setprecision(3)
              << poolTime << " ms" << std::endl;

    double speedup = stdTime / poolTime;
    std::cout << "\nSpeedup: " << std::fixed << std::setprecision(2) << speedup
              << "x faster" << std::endl;

    return 0;
}
