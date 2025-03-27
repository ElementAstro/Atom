/**
 * @file short_alloc_example.cpp
 * @brief Comprehensive examples of using ShortAlloc and Arena classes
 * @author Example Author
 * @date 2025-03-23
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/short_alloc.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Helper function to measure execution time
template <typename Func>
double measureTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

// Small class for allocation testing
class TestObject {
public:
    TestObject() : value_(0), data_() { constructionCount++; }

    explicit TestObject(int val) : value_(val), data_() {
        std::fill(data_.begin(), data_.end(), static_cast<char>(val % 256));
        constructionCount++;
    }

    ~TestObject() { destructionCount++; }

    int getValue() const { return value_; }

    void setValue(int value) {
        value_ = value;
        std::fill(data_.begin(), data_.end(), static_cast<char>(value % 256));
    }

    static void resetCounters() {
        constructionCount = 0;
        destructionCount = 0;
    }

    static int getConstructionCount() { return constructionCount; }
    static int getDestructionCount() { return destructionCount; }

private:
    int value_;
    std::array<char, 128> data_;  // Make the object reasonably sized

    static inline int constructionCount = 0;
    static inline int destructionCount = 0;
};

// Large object for testing different allocation sizes
class LargeObject {
public:
    LargeObject() : data_(1024, 0) {}
    explicit LargeObject(int val) : data_(1024, static_cast<char>(val % 256)) {}

    std::vector<char>& getData() { return data_; }
    const std::vector<char>& getData() const { return data_; }

private:
    std::vector<char> data_;  // 1KB of data
};

// Custom structure to test with Arena directly
struct CustomStruct {
    int id;
    double values[16];
    char name[64];
    bool active;

    CustomStruct() : id(0), active(false) {
        std::fill_n(values, 16, 0.0);
        std::fill_n(name, 64, '\0');
    }

    CustomStruct(int i, const std::string& n) : id(i), active(true) {
        std::fill_n(values, 16, static_cast<double>(i));
        std::strncpy(name, n.c_str(), 63);
        name[63] = '\0';  // Ensure null termination
    }

    void print() const {
        std::cout << "CustomStruct { id: " << id << ", name: \"" << name
                  << "\", active: " << (active ? "true" : "false") << " }"
                  << std::endl;
    }
};

int main() {
    std::cout << "SHORT ALLOCATOR COMPREHENSIVE EXAMPLES\n";
    std::cout << "=====================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Arena Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Arena Usage");

    // Create an arena with 16KB of memory
    constexpr size_t ArenaSize = 16 * 1024;
    atom::memory::Arena<ArenaSize> basicArena;

    std::cout << "Created arena with " << ArenaSize << " bytes of memory"
              << std::endl;
    std::cout << "Used memory: " << basicArena.used() << " bytes" << std::endl;
    std::cout << "Remaining memory: " << basicArena.remaining() << " bytes"
              << std::endl;

    // Allocate some memory directly from the arena
    std::cout << "\nAllocating memory directly from arena..." << std::endl;
    void* ptr1 = basicArena.allocate(1024);
    std::cout << "Allocated 1024 bytes at " << ptr1 << std::endl;
    std::cout << "Used memory: " << basicArena.used() << " bytes" << std::endl;
    std::cout << "Remaining memory: " << basicArena.remaining() << " bytes"
              << std::endl;

    void* ptr2 = basicArena.allocate(2048);
    std::cout << "Allocated 2048 bytes at " << ptr2 << std::endl;
    std::cout << "Used memory: " << basicArena.used() << " bytes" << std::endl;
    std::cout << "Remaining memory: " << basicArena.remaining() << " bytes"
              << std::endl;

    // Deallocate memory
    std::cout << "\nDeallocating memory..." << std::endl;
    basicArena.deallocate(ptr1);
    std::cout << "Deallocated memory at " << ptr1 << std::endl;
    std::cout << "Used memory: " << basicArena.used() << " bytes" << std::endl;
    std::cout << "Remaining memory: " << basicArena.remaining() << " bytes"
              << std::endl;

    // Reallocate in the freed space
    void* ptr3 = basicArena.allocate(512);
    std::cout << "Allocated 512 bytes at " << ptr3 << std::endl;
    std::cout << "Used memory: " << basicArena.used() << " bytes" << std::endl;
    std::cout << "Remaining memory: " << basicArena.remaining() << " bytes"
              << std::endl;

    // Check if pointer is owned by arena
    std::cout << "\nChecking if pointers are owned by arena:" << std::endl;
    std::cout << "ptr2 owned by arena: "
              << (basicArena.owns(ptr2) ? "Yes" : "No") << std::endl;
    std::cout << "ptr3 owned by arena: "
              << (basicArena.owns(ptr3) ? "Yes" : "No") << std::endl;
    std::cout << "Random pointer owned by arena: "
              << (basicArena.owns(&basicArena) ? "Yes" : "No") << std::endl;

    // Get memory stats
    std::cout << "\nMemory statistics:" << std::endl;
    std::cout << basicArena.getStats() << std::endl;

    // Reset the arena
    std::cout << "\nResetting arena..." << std::endl;
    basicArena.reset();
    std::cout << "Used memory after reset: " << basicArena.used() << " bytes"
              << std::endl;
    std::cout << "Remaining memory after reset: " << basicArena.remaining()
              << " bytes" << std::endl;

    //--------------------------------------------------------------------------
    // 2. ShortAlloc with STL Containers
    //--------------------------------------------------------------------------
    printSection("2. ShortAlloc with STL Containers");

    // Define a 32KB arena for STL containers
    constexpr size_t ContainerArenaSize = 32 * 1024;
    atom::memory::Arena<ContainerArenaSize> containerArena;

    // Create vector using ShortAlloc - 修复括号初始化语法
    std::cout << "Creating vector with ShortAlloc..." << std::endl;
    using IntVectorAlloc = atom::memory::ShortAlloc<int, ContainerArenaSize>;
    std::vector<int, IntVectorAlloc> shortVector{
        IntVectorAlloc(containerArena)};

    // Add elements to vector
    std::cout << "Adding elements to vector..." << std::endl;
    for (int i = 0; i < 1000; ++i) {
        shortVector.push_back(i);
    }

    std::cout << "Vector size: " << shortVector.size() << std::endl;
    std::cout << "Vector capacity: " << shortVector.capacity() << std::endl;
    std::cout << "Arena used after vector allocation: " << containerArena.used()
              << " bytes" << std::endl;

    // Create a string using ShortAlloc - 修复括号初始化语法
    std::cout << "\nCreating string with ShortAlloc..." << std::endl;
    using CharAlloc = atom::memory::ShortAlloc<char, ContainerArenaSize>;
    std::basic_string<char, std::char_traits<char>, CharAlloc> shortString{
        CharAlloc(containerArena)};

    // Set string value
    shortString =
        "This is a string allocated using ShortAlloc in a fixed-size arena.";
    std::cout << "String value: " << shortString << std::endl;
    std::cout << "String length: " << shortString.length() << std::endl;
    std::cout << "Arena used after string allocation: " << containerArena.used()
              << " bytes" << std::endl;

    // Create a map using ShortAlloc - 修复括号初始化语法
    std::cout << "\nCreating map with ShortAlloc..." << std::endl;
    using MapAlloc = atom::memory::ShortAlloc<std::pair<const int, std::string>,
                                              ContainerArenaSize>;
    std::map<int, std::string, std::less<int>, MapAlloc> shortMap{
        MapAlloc(containerArena)};

    // Add elements to map
    std::cout << "Adding elements to map..." << std::endl;
    shortMap[1] = "One";
    shortMap[2] = "Two";
    shortMap[3] = "Three";
    shortMap[4] = "Four";
    shortMap[5] = "Five";

    std::cout << "Map size: " << shortMap.size() << std::endl;
    std::cout << "Map contents:" << std::endl;
    for (const auto& [key, value] : shortMap) {
        std::cout << "  " << key << ": " << value << std::endl;
    }

    std::cout << "Arena used after map allocation: " << containerArena.used()
              << " bytes" << std::endl;

    // Use the utility function to create a container with arena
    std::cout << "\nCreating containers using makeArenaContainer utility..."
              << std::endl;
    auto shortList =
        atom::memory::makeArenaContainer<std::list, int, ContainerArenaSize>(
            containerArena);

    for (int i = 0; i < 10; ++i) {
        shortList.push_back(i * 10);
    }

    std::cout << "List size: " << shortList.size() << std::endl;
    std::cout << "List contents:";
    for (int value : shortList) {
        std::cout << " " << value;
    }
    std::cout << std::endl;

    std::cout << "Arena used after list allocation: " << containerArena.used()
              << " bytes" << std::endl;

    // Display memory stats for container arena
    std::cout << "\nContainer arena memory statistics:" << std::endl;
    std::cout << containerArena.getStats() << std::endl;

    //--------------------------------------------------------------------------
    // 3. Different Allocation Strategies
    //--------------------------------------------------------------------------
    printSection("3. Different Allocation Strategies");

    // Create arenas with different allocation strategies
    constexpr size_t StrategyArenaSize = 8 * 1024;

    atom::memory::Arena<StrategyArenaSize, alignof(std::max_align_t), true,
                        atom::memory::AllocationStrategy::FirstFit>
        firstFitArena;

    atom::memory::Arena<StrategyArenaSize, alignof(std::max_align_t), true,
                        atom::memory::AllocationStrategy::BestFit>
        bestFitArena;

    atom::memory::Arena<StrategyArenaSize, alignof(std::max_align_t), true,
                        atom::memory::AllocationStrategy::WorstFit>
        worstFitArena;

    std::cout << "Created three arenas with different allocation strategies:"
              << std::endl;
    std::cout << "  - FirstFit: Allocates the first block that fits"
              << std::endl;
    std::cout << "  - BestFit: Allocates the smallest block that fits"
              << std::endl;
    std::cout << "  - WorstFit: Allocates the largest block that fits"
              << std::endl;

    // Prepare a common allocation pattern to test different strategies
    struct AllocationRequest {
        size_t size;
        void* ptr = nullptr;
    };

    std::vector<AllocationRequest> allocations = {{256}, {128}, {512}, {1024},
                                                  {64},  {768}, {384}, {256}};

    // Function to allocate memory and track pointers
    auto allocateMemory = [](auto& arena,
                             std::vector<AllocationRequest>& requests) {
        for (auto& req : requests) {
            req.ptr = arena.allocate(req.size);
        }
    };

    // Function to deallocate specific indices and reallocate
    auto deallocateAndReallocate =
        [](auto& arena, std::vector<AllocationRequest>& requests,
           const std::vector<size_t>& indicesToFree, size_t newAllocationSize) {
            // Free selected allocations
            for (size_t index : indicesToFree) {
                if (index < requests.size() && requests[index].ptr) {
                    arena.deallocate(requests[index].ptr);
                    requests[index].ptr = nullptr;
                    requests[index].size = 0;
                }
            }

            // Allocate a new block
            void* newPtr = arena.allocate(newAllocationSize);

            // Add to requests
            requests.push_back({newAllocationSize, newPtr});
        };

    // Test FirstFit strategy
    std::cout << "\nTesting FirstFit strategy:" << std::endl;
    std::vector<AllocationRequest> firstFitRequests = allocations;
    allocateMemory(firstFitArena, firstFitRequests);

    std::cout << "Initial allocations:" << std::endl;
    for (size_t i = 0; i < firstFitRequests.size(); ++i) {
        std::cout << "  Block " << i << ": " << firstFitRequests[i].size
                  << " bytes at " << firstFitRequests[i].ptr << std::endl;
    }

    std::cout << "Memory used: " << firstFitArena.used() << " bytes"
              << std::endl;

    // Free some blocks and allocate a new one
    std::cout
        << "\nFreeing blocks 1, 3, 5 and allocating a new 300-byte block..."
        << std::endl;
    deallocateAndReallocate(firstFitArena, firstFitRequests, {1, 3, 5}, 300);

    std::cout << "New allocation: " << firstFitRequests.back().size
              << " bytes at " << firstFitRequests.back().ptr << std::endl;
    std::cout << "Memory used: " << firstFitArena.used() << " bytes"
              << std::endl;

    // Test BestFit strategy
    std::cout << "\nTesting BestFit strategy:" << std::endl;
    std::vector<AllocationRequest> bestFitRequests = allocations;
    allocateMemory(bestFitArena, bestFitRequests);

    // Free some blocks and allocate a new one
    std::cout << "Freeing blocks 1, 3, 5 and allocating a new 300-byte block..."
              << std::endl;
    deallocateAndReallocate(bestFitArena, bestFitRequests, {1, 3, 5}, 300);

    std::cout << "New allocation: " << bestFitRequests.back().size
              << " bytes at " << bestFitRequests.back().ptr << std::endl;
    std::cout << "Memory used: " << bestFitArena.used() << " bytes"
              << std::endl;

    // Test WorstFit strategy
    std::cout << "\nTesting WorstFit strategy:" << std::endl;
    std::vector<AllocationRequest> worstFitRequests = allocations;
    allocateMemory(worstFitArena, worstFitRequests);

    // Free some blocks and allocate a new one
    std::cout << "Freeing blocks 1, 3, 5 and allocating a new 300-byte block..."
              << std::endl;
    deallocateAndReallocate(worstFitArena, worstFitRequests, {1, 3, 5}, 300);

    std::cout << "New allocation: " << worstFitRequests.back().size
              << " bytes at " << worstFitRequests.back().ptr << std::endl;
    std::cout << "Memory used: " << worstFitArena.used() << " bytes"
              << std::endl;

    // Compare fragmentation levels
    std::cout << "\nComparing memory fragmentation between strategies:"
              << std::endl;
    size_t firstFitFragments = firstFitArena.defragment();
    size_t bestFitFragments = bestFitArena.defragment();
    size_t worstFitFragments = worstFitArena.defragment();

    std::cout << "FirstFit fragments merged: " << firstFitFragments
              << std::endl;
    std::cout << "BestFit fragments merged: " << bestFitFragments << std::endl;
    std::cout << "WorstFit fragments merged: " << worstFitFragments
              << std::endl;

    //--------------------------------------------------------------------------
    // 4. Object Construction and Destruction with ShortAlloc
    //--------------------------------------------------------------------------
    printSection("4. Object Construction and Destruction with ShortAlloc");

    constexpr size_t ObjectArenaSize = 16 * 1024;
    atom::memory::Arena<ObjectArenaSize> objectArena;

    // Reset object counters
    TestObject::resetCounters();

    // Create allocator
    using TestObjectAlloc =
        atom::memory::ShortAlloc<TestObject, ObjectArenaSize>;
    TestObjectAlloc objAlloc(objectArena);

    std::cout << "Initial construction count: "
              << TestObject::getConstructionCount() << std::endl;
    std::cout << "Initial destruction count: "
              << TestObject::getDestructionCount() << std::endl;

    // Allocate and construct objects
    std::cout << "\nAllocating and constructing objects..." << std::endl;
    TestObject* objPtr1 = objAlloc.allocate(1);
    objAlloc.construct(objPtr1, 42);

    std::cout << "Constructed object with value: " << objPtr1->getValue()
              << std::endl;
    std::cout << "Construction count: " << TestObject::getConstructionCount()
              << std::endl;
    std::cout << "Destruction count: " << TestObject::getDestructionCount()
              << std::endl;

    // Allocate and construct multiple objects
    std::cout << "\nAllocating and constructing multiple objects..."
              << std::endl;
    TestObject* objPtr2 = objAlloc.allocate(5);
    for (int i = 0; i < 5; ++i) {
        objAlloc.construct(objPtr2 + i, i * 100);
    }

    for (int i = 0; i < 5; ++i) {
        std::cout << "Object " << i << " value: " << (objPtr2 + i)->getValue()
                  << std::endl;
    }

    std::cout << "Construction count: " << TestObject::getConstructionCount()
              << std::endl;
    std::cout << "Destruction count: " << TestObject::getDestructionCount()
              << std::endl;

    // Destroy and deallocate objects
    std::cout << "\nDestroying and deallocating objects..." << std::endl;

    objAlloc.destroy(objPtr1);
    objAlloc.deallocate(objPtr1, 1);

    for (int i = 0; i < 5; ++i) {
        objAlloc.destroy(objPtr2 + i);
    }
    objAlloc.deallocate(objPtr2, 5);

    std::cout << "Construction count: " << TestObject::getConstructionCount()
              << std::endl;
    std::cout << "Destruction count: " << TestObject::getDestructionCount()
              << std::endl;

    // Using allocateUnique for automatic memory management
    std::cout << "\nUsing allocateUnique for automatic memory management..."
              << std::endl;

    {
        auto uniqueObj =
            atom::memory::allocateUnique<TestObjectAlloc, TestObject>(objAlloc,
                                                                      999);
        std::cout << "Unique object value: " << uniqueObj->getValue()
                  << std::endl;
        std::cout << "Construction count: "
                  << TestObject::getConstructionCount() << std::endl;

        // Object will be automatically destroyed and deallocated when it goes
        // out of scope
        std::cout << "Letting unique_ptr go out of scope..." << std::endl;
    }

    std::cout << "Construction count: " << TestObject::getConstructionCount()
              << std::endl;
    std::cout << "Destruction count: " << TestObject::getDestructionCount()
              << std::endl;

    //--------------------------------------------------------------------------
    // 5. Thread-Safety Features
    //--------------------------------------------------------------------------
    printSection("5. Thread-Safety Features");

    // Create thread-safe and non-thread-safe arenas
    constexpr size_t ThreadArenaSize = 32 * 1024;

    atom::memory::Arena<ThreadArenaSize, alignof(std::max_align_t), true>
        threadSafeArena;
    atom::memory::Arena<ThreadArenaSize, alignof(std::max_align_t), false>
        nonThreadSafeArena;

    std::cout << "Created thread-safe and non-thread-safe arenas" << std::endl;

    // Function to perform allocations in multiple threads
    auto threadAllocationTest = [](auto& arena, int threadId, int allocCount,
                                   std::vector<void*>& allocatedPtrs) {
        for (int i = 0; i < allocCount; ++i) {
            size_t size = 100 + (threadId * 10) + (i % 50);
            void* ptr = arena.allocate(size);
            allocatedPtrs.push_back(ptr);

            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    // Test thread-safe arena
    std::cout << "\nTesting thread-safe arena with concurrent allocations..."
              << std::endl;

    std::vector<std::vector<void*>> threadSafeAllocations(4);
    std::vector<std::thread> threadSafeThreads;

    double threadSafeTime = measureTime([&]() {
        // Start 4 threads making allocations
        for (int i = 0; i < 4; ++i) {
            threadSafeThreads.emplace_back(threadAllocationTest,
                                           std::ref(threadSafeArena), i, 100,
                                           std::ref(threadSafeAllocations[i]));
        }

        // Wait for all threads to finish
        for (auto& thread : threadSafeThreads) {
            thread.join();
        }
    });

    size_t totalThreadSafeAllocs = 0;
    for (const auto& allocs : threadSafeAllocations) {
        totalThreadSafeAllocs += allocs.size();
    }

    std::cout << "Completed " << totalThreadSafeAllocs << " allocations in "
              << threadSafeTime << " ms" << std::endl;
    std::cout << "Arena used memory: " << threadSafeArena.used() << " bytes"
              << std::endl;

    // Test non-thread-safe arena in a single thread for comparison
    std::cout
        << "\nTesting non-thread-safe arena with sequential allocations..."
        << std::endl;

    std::vector<void*> nonThreadSafeAllocations;

    double nonThreadSafeTime = measureTime([&]() {
        for (int i = 0; i < 4; ++i) {
            threadAllocationTest(nonThreadSafeArena, i, 100,
                                 nonThreadSafeAllocations);
        }
    });

    std::cout << "Completed " << nonThreadSafeAllocations.size()
              << " allocations in " << nonThreadSafeTime << " ms" << std::endl;
    std::cout << "Arena used memory: " << nonThreadSafeArena.used() << " bytes"
              << std::endl;

    std::cout << "\nThread-safe vs non-thread-safe performance ratio: "
              << (nonThreadSafeTime / threadSafeTime) << "x" << std::endl;

    // Clean up allocations
    std::cout << "\nFreeing allocations..." << std::endl;

    for (auto& threadAllocs : threadSafeAllocations) {
        for (void* ptr : threadAllocs) {
            threadSafeArena.deallocate(ptr);
        }
        threadAllocs.clear();
    }

    for (void* ptr : nonThreadSafeAllocations) {
        nonThreadSafeArena.deallocate(ptr);
    }
    nonThreadSafeAllocations.clear();

    //--------------------------------------------------------------------------
    // 6. Memory Validation and Debugging
    //--------------------------------------------------------------------------
    printSection("6. Memory Validation and Debugging");

    constexpr size_t DebugArenaSize = 8 * 1024;
    atom::memory::Arena<DebugArenaSize> debugArena;

    std::cout << "Created arena for debugging tests" << std::endl;

    // Validate empty arena
    std::cout << "\nValidating empty arena..." << std::endl;
    bool isValid = debugArena.validate();
    std::cout << "Arena validation: " << (isValid ? "PASSED" : "FAILED")
              << std::endl;

    // Allocate some memory
    std::cout << "\nAllocating memory blocks..." << std::endl;
    void* debugPtr1 = debugArena.allocate(256);
    void* debugPtr2 = debugArena.allocate(512);
    void* debugPtr3 = debugArena.allocate(128);

    std::cout << "Allocated 3 blocks: " << debugPtr1 << " (256 bytes), "
              << debugPtr2 << " (512 bytes), " << debugPtr3 << " (128 bytes)"
              << std::endl;

    // Validate arena with allocations
    std::cout << "\nValidating arena with allocations..." << std::endl;
    isValid = debugArena.validate();
    std::cout << "Arena validation: " << (isValid ? "PASSED" : "FAILED")
              << std::endl;

    // Deallocate the middle block to create fragmentation
    std::cout << "\nDeallocating middle block to create fragmentation..."
              << std::endl;
    debugArena.deallocate(debugPtr2);

    // Display memory statistics
    std::cout << "Arena memory statistics after deallocation:" << std::endl;
    std::cout << debugArena.getStats() << std::endl;

    // Attempt to defragment
    std::cout << "\nAttempting to defragment the arena..." << std::endl;
    size_t fragmentsMerged = debugArena.defragment();
    std::cout << "Fragments merged: " << fragmentsMerged << std::endl;

    // Display memory statistics after defragmentation
    std::cout << "Arena memory statistics after defragmentation:" << std::endl;
    std::cout << debugArena.getStats() << std::endl;

    // Validate arena after defragmentation
    std::cout << "\nValidating arena after defragmentation..." << std::endl;
    isValid = debugArena.validate();
    std::cout << "Arena validation: " << (isValid ? "PASSED" : "FAILED")
              << std::endl;

    // Clean up allocations
    std::cout << "\nFreeing all allocations..." << std::endl;
    debugArena.deallocate(debugPtr1);
    debugArena.deallocate(debugPtr3);

    // Final validation
    std::cout << "\nFinal arena validation..." << std::endl;
    isValid = debugArena.validate();
    std::cout << "Arena validation: " << (isValid ? "PASSED" : "FAILED")
              << std::endl;

    //--------------------------------------------------------------------------
    // 7. Custom Alignment
    //--------------------------------------------------------------------------
    printSection("7. Custom Alignment");

    // Create arenas with different alignments
    constexpr size_t AlignmentArenaSize = 8 * 1024;

    atom::memory::Arena<AlignmentArenaSize, 1> defaultAlignArena;
    atom::memory::Arena<AlignmentArenaSize, 16> align16Arena;
    atom::memory::Arena<AlignmentArenaSize, 64> align64Arena;
    atom::memory::Arena<AlignmentArenaSize, 128> align128Arena;

    std::cout << "Created arenas with different alignments:" << std::endl;
    std::cout << "  - Default alignment: " << alignof(std::max_align_t)
              << " bytes" << std::endl;
    std::cout << "  - Custom alignment 16: 16 bytes" << std::endl;
    std::cout << "  - Custom alignment 64: 64 bytes" << std::endl;
    std::cout << "  - Custom alignment 128: 128 bytes" << std::endl;

    // Function to check pointer alignment
    auto checkAlignment = [](void* ptr, size_t alignment) -> bool {
        return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
    };

    // Allocate and check alignment
    std::cout << "\nTesting alignment of allocations..." << std::endl;

    // Test default alignment
    void* defaultPtr = defaultAlignArena.allocate(100);
    std::cout << "Default alignment allocation: " << defaultPtr << std::endl;
    std::cout << "  Aligned to 1 byte: "
              << (checkAlignment(defaultPtr, 1) ? "Yes" : "No") << std::endl;
    std::cout << "  Aligned to 2 bytes: "
              << (checkAlignment(defaultPtr, 2) ? "Yes" : "No") << std::endl;
    std::cout << "  Aligned to 4 bytes: "
              << (checkAlignment(defaultPtr, 4) ? "Yes" : "No") << std::endl;
    std::cout << "  Aligned to 8 bytes: "
              << (checkAlignment(defaultPtr, 8) ? "Yes" : "No") << std::endl;

    // Test 16-byte alignment
    void* align16Ptr = align16Arena.allocate(100);
    std::cout << "\n16-byte alignment allocation: " << align16Ptr << std::endl;
    std::cout << "  Aligned to 16 bytes: "
              << (checkAlignment(align16Ptr, 16) ? "Yes" : "No") << std::endl;

    // Test 64-byte alignment
    void* align64Ptr = align64Arena.allocate(100);
    std::cout << "\n64-byte alignment allocation: " << align64Ptr << std::endl;
    std::cout << "  Aligned to 64 bytes: "
              << (checkAlignment(align64Ptr, 64) ? "Yes" : "No") << std::endl;

    // Test 128-byte alignment
    void* align128Ptr = align128Arena.allocate(100);
    std::cout << "\n128-byte alignment allocation: " << align128Ptr
              << std::endl;
    std::cout << "  Aligned to 128 bytes: "
              << (checkAlignment(align128Ptr, 128) ? "Yes" : "No") << std::endl;

    // Clean up allocations
    defaultAlignArena.deallocate(defaultPtr);
    align16Arena.deallocate(align16Ptr);
    align64Arena.deallocate(align64Ptr);
    align128Arena.deallocate(align128Ptr);

    //--------------------------------------------------------------------------
    // 8. Performance Comparison with Standard Allocator
    //--------------------------------------------------------------------------
    printSection("8. Performance Comparison with Standard Allocator");

    constexpr size_t PerfArenaSize = 50 * 1024 * 1024;  // 50MB arena
    atom::memory::Arena<PerfArenaSize> perfArena;
    using PerfObjectAlloc = atom::memory::ShortAlloc<TestObject, PerfArenaSize>;

    const int numElements = 100000;
    std::cout << "Testing performance with " << numElements << " elements"
              << std::endl;

    // Test vector with standard allocator
    std::cout << "\nStandard allocator:" << std::endl;
    double stdTime = measureTime([&]() {
        std::vector<TestObject> stdVector;
        stdVector.reserve(numElements);

        for (int i = 0; i < numElements; ++i) {
            stdVector.emplace_back(i);
        }
    });

    std::cout << "  Time taken: " << stdTime << " ms" << std::endl;

    // Test vector with short allocator - 修复括号初始化语法
    std::cout << "\nShortAlloc allocator:" << std::endl;
    double shortTime = measureTime([&]() {
        std::vector<TestObject, PerfObjectAlloc> shortVector{
            PerfObjectAlloc(perfArena)};
        shortVector.reserve(numElements);

        for (int i = 0; i < numElements; ++i) {
            shortVector.emplace_back(i);
        }
    });

    std::cout << "  Time taken: " << shortTime << " ms" << std::endl;
    std::cout << "  ShortAlloc is " << (stdTime / shortTime)
              << "x faster than standard allocator" << std::endl;

    // Small object allocation test
    std::cout << "\nTesting small object allocation performance:" << std::endl;

    // Standard allocator
    std::cout << "Standard allocator (small allocations):" << std::endl;
    double stdSmallTime = measureTime([&]() {
        std::vector<int*> pointers;
        pointers.reserve(numElements);

        for (int i = 0; i < numElements; ++i) {
            pointers.push_back(new int(i));
        }

        for (auto ptr : pointers) {
            delete ptr;
        }
    });

    std::cout << "  Time taken: " << stdSmallTime << " ms" << std::endl;

    // ShortAlloc
    std::cout << "ShortAlloc (small allocations):" << std::endl;
    atom::memory::Arena<PerfArenaSize> smallArena;
    atom::memory::ShortAlloc<int, PerfArenaSize> smallAlloc(smallArena);

    double shortSmallTime = measureTime([&]() {
        std::vector<int*> pointers;
        pointers.reserve(numElements);

        for (int i = 0; i < numElements; ++i) {
            int* ptr = smallAlloc.allocate(1);
            smallAlloc.construct(ptr, i);
            pointers.push_back(ptr);
        }

        for (auto ptr : pointers) {
            smallAlloc.destroy(ptr);
            smallAlloc.deallocate(ptr, 1);
        }
    });

    std::cout << "  Time taken: " << shortSmallTime << " ms" << std::endl;
    std::cout << "  ShortAlloc is " << (stdSmallTime / shortSmallTime)
              << "x faster for small allocations" << std::endl;

    // Show arena stats after performance test
    std::cout << "\nArena statistics after performance tests:" << std::endl;
    std::cout << perfArena.getStats() << std::endl;
    std::cout << smallArena.getStats() << std::endl;

    //--------------------------------------------------------------------------
    // 9. Advanced Usage: Complex Objects and Containers
    //--------------------------------------------------------------------------
    printSection("9. Advanced Usage: Complex Objects and Containers");

    constexpr size_t AdvancedArenaSize = 4 * 1024 * 1024;  // 4MB
    atom::memory::Arena<AdvancedArenaSize> advancedArena;

    // Create allocators for different types
    using StringAlloc = atom::memory::ShortAlloc<char, AdvancedArenaSize>;
    using ShortString =
        std::basic_string<char, std::char_traits<char>, StringAlloc>;
    using VectorAlloc =
        atom::memory::ShortAlloc<LargeObject, AdvancedArenaSize>;
    using MapPairAlloc =
        atom::memory::ShortAlloc<std::pair<const ShortString, LargeObject>,
                                 AdvancedArenaSize>;
    // String with custom allocator - 修复括号初始化语法
    std::cout << "Creating strings with ShortAlloc..." << std::endl;

    ShortString str1{StringAlloc(advancedArena)};
    str1 = "This is a string with a custom allocator";

    ShortString str2{StringAlloc(advancedArena)};
    str2 = "This is another string with the same arena";

    std::cout << "String 1: " << str1 << std::endl;
    std::cout << "String 2: " << str2 << std::endl;

    // Vector of large objects - 修复括号初始化语法
    std::cout << "\nCreating vector of large objects..." << std::endl;
    std::vector<LargeObject, VectorAlloc> largeVector{
        VectorAlloc(advancedArena)};

    for (int i = 0; i < 10; ++i) {
        largeVector.emplace_back(i);
    }

    std::cout << "Vector size: " << largeVector.size() << std::endl;
    std::cout << "First element data size: " << largeVector[0].getData().size()
              << " bytes" << std::endl;

    // Map with custom strings and large objects - 修复括号初始化语法
    std::cout << "\nCreating map with custom strings and large objects..."
              << std::endl;

    std::map<ShortString, LargeObject, std::less<ShortString>, MapPairAlloc>
        complexMap{MapPairAlloc(advancedArena)};

    complexMap[ShortString("key1", StringAlloc(advancedArena))] =
        LargeObject(1);
    complexMap[ShortString("key2", StringAlloc(advancedArena))] =
        LargeObject(2);
    complexMap[ShortString("key3", StringAlloc(advancedArena))] =
        LargeObject(3);

    std::cout << "Map size: " << complexMap.size() << std::endl;

    // Create a nested data structure - 修复括号初始化语法
    std::cout << "\nCreating nested data structure..." << std::endl;

    using NestedVectorAlloc = atom::memory::ShortAlloc<
        std::vector<int, atom::memory::ShortAlloc<int, AdvancedArenaSize>>,
        AdvancedArenaSize>;

    std::vector<
        std::vector<int, atom::memory::ShortAlloc<int, AdvancedArenaSize>>,
        NestedVectorAlloc>
        nestedVector{NestedVectorAlloc(advancedArena)};

    for (int i = 0; i < 5; ++i) {
        std::vector<int, atom::memory::ShortAlloc<int, AdvancedArenaSize>>
            innerVec{atom::memory::ShortAlloc<int, AdvancedArenaSize>(
                advancedArena)};

        for (int j = 0; j < 5; ++j) {
            innerVec.push_back(i * 10 + j);
        }

        nestedVector.push_back(std::move(innerVec));
    }

    std::cout << "Nested vector structure: " << std::endl;
    for (size_t i = 0; i < nestedVector.size(); ++i) {
        std::cout << "  Row " << i << ":";
        for (int val : nestedVector[i]) {
            std::cout << " " << val;
        }
        std::cout << std::endl;
    }

    // Display arena statistics
    std::cout << "\nAdvanced arena statistics:" << std::endl;
    std::cout << advancedArena.getStats() << std::endl;

    //--------------------------------------------------------------------------
    // 10. Direct Arena Allocation for Custom Structures
    //--------------------------------------------------------------------------
    printSection("10. Direct Arena Allocation for Custom Structures");

    constexpr size_t CustomArenaSize = 1 * 1024 * 1024;  // 1MB
    atom::memory::Arena<CustomArenaSize> customArena;

    std::cout << "Allocating and constructing custom structures directly..."
              << std::endl;

    // Allocate memory for CustomStruct
    CustomStruct* customPtr1 =
        static_cast<CustomStruct*>(customArena.allocate(sizeof(CustomStruct)));
    CustomStruct* customPtr2 =
        static_cast<CustomStruct*>(customArena.allocate(sizeof(CustomStruct)));
    CustomStruct* customPtr3 =
        static_cast<CustomStruct*>(customArena.allocate(sizeof(CustomStruct)));

    // Construct objects in place
    new (customPtr1) CustomStruct(1, "First Structure");
    new (customPtr2) CustomStruct(2, "Second Structure");
    new (customPtr3) CustomStruct(3, "Third Structure");

    // Display the objects
    std::cout << "\nConstructed custom structures:" << std::endl;
    customPtr1->print();
    customPtr2->print();
    customPtr3->print();

    // Use placement new to update an object
    std::cout << "\nUpdating second structure..." << std::endl;
    customPtr2->~CustomStruct();
    new (customPtr2) CustomStruct(42, "Updated Structure");
    customPtr2->print();

    // Manually destroy objects before deallocating
    std::cout << "\nDestroying objects..." << std::endl;
    customPtr1->~CustomStruct();
    customPtr2->~CustomStruct();
    customPtr3->~CustomStruct();

    // Deallocate memory
    customArena.deallocate(customPtr1);
    customArena.deallocate(customPtr2);
    customArena.deallocate(customPtr3);

    std::cout << "Custom structures deallocated" << std::endl;

    // Allocate an array of structures
    std::cout << "\nAllocating array of structures..." << std::endl;
    constexpr size_t arrayCount = 5;
    CustomStruct* arrayPtr = static_cast<CustomStruct*>(
        customArena.allocate(sizeof(CustomStruct) * arrayCount));

    // Construct array elements
    for (size_t i = 0; i < arrayCount; ++i) {
        new (&arrayPtr[i])
            CustomStruct(100 + i, "Array Element " + std::to_string(i));
    }

    // Display array elements
    std::cout << "Array elements:" << std::endl;
    for (size_t i = 0; i < arrayCount; ++i) {
        arrayPtr[i].print();
    }

    // Destroy array elements
    for (size_t i = 0; i < arrayCount; ++i) {
        arrayPtr[i].~CustomStruct();
    }

    // Deallocate array
    customArena.deallocate(arrayPtr);

    // Show arena statistics
    std::cout << "\nCustom arena statistics:" << std::endl;
    std::cout << customArena.getStats() << std::endl;

    //--------------------------------------------------------------------------
    // Summary and Cleanup
    //--------------------------------------------------------------------------
    printSection("Summary and Cleanup");

    std::cout << "This example demonstrated the following capabilities:"
              << std::endl;
    std::cout << "  1. Basic Arena usage for direct memory allocation"
              << std::endl;
    std::cout << "  2. Using ShortAlloc with STL containers" << std::endl;
    std::cout
        << "  3. Different allocation strategies (FirstFit, BestFit, WorstFit)"
        << std::endl;
    std::cout << "  4. Object construction and destruction with ShortAlloc"
              << std::endl;
    std::cout << "  5. Thread-safety features" << std::endl;
    std::cout << "  6. Memory validation and debugging" << std::endl;
    std::cout << "  7. Custom alignment support" << std::endl;
    std::cout << "  8. Performance comparison with standard allocators"
              << std::endl;
    std::cout
        << "  9. Advanced usage with complex objects and nested containers"
        << std::endl;
    std::cout << "  10. Direct arena allocation for custom structures"
              << std::endl;

    std::cout << "\nResetting all arenas..." << std::endl;

    // Reset all arenas to clean up memory
    basicArena.reset();
    containerArena.reset();
    firstFitArena.reset();
    bestFitArena.reset();
    worstFitArena.reset();
    objectArena.reset();
    threadSafeArena.reset();
    nonThreadSafeArena.reset();
    debugArena.reset();
    defaultAlignArena.reset();
    align16Arena.reset();
    align64Arena.reset();
    align128Arena.reset();
    perfArena.reset();
    smallArena.reset();
    advancedArena.reset();
    customArena.reset();

    std::cout << "All resources have been cleaned up successfully."
              << std::endl;

    return 0;
}