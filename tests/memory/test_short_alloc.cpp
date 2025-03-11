
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "atom/memory/short_alloc.hpp"

using namespace atom::memory;

class ArenaTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(ArenaTest, Constructor) {
    Arena<1024> arena;
    EXPECT_EQ(arena.size(), 1024);
    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.remaining(), 1024);
}

TEST_F(ArenaTest, AllocateAndDeallocate) {
    Arena<1024> arena;
    void* ptr = arena.allocate(100);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(arena.used(), 100);
    EXPECT_EQ(arena.remaining(), 924);

    arena.deallocate(ptr, 100);
    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.remaining(), 1024);
}

TEST_F(ArenaTest, AllocateExceedingSize) {
    Arena<1024> arena;
    EXPECT_THROW(arena.allocate(2048), std::bad_alloc);
}

TEST_F(ArenaTest, Reset) {
    Arena<1024> arena;
    void* ptr = arena.allocate(100);
    EXPECT_NE(ptr, nullptr);
    arena.reset();
    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.remaining(), 1024);
}

TEST_F(ArenaTest, ThreadSafety) {
    Arena<1024> arena;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&arena]() {
            for (int j = 0; j < 10; ++j) {
                void* ptr = arena.allocate(10);
                arena.deallocate(ptr, 10);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.remaining(), 1024);
}

class ShortAllocTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(ShortAllocTest, Constructor) {
    Arena<1024> arena;
    ShortAlloc<int, 1024> alloc(arena);
    EXPECT_EQ(alloc.SIZE, 1024);
    EXPECT_EQ(alloc.ALIGNMENT, alignof(std::max_align_t));
}

TEST_F(ShortAllocTest, AllocateAndDeallocate) {
    Arena<1024> arena;
    ShortAlloc<int, 1024> alloc(arena);
    int* ptr = alloc.allocate(10);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(arena.used(), 10 * sizeof(int));
    EXPECT_EQ(arena.remaining(), 1024 - 10 * sizeof(int));

    alloc.deallocate(ptr, 10);
    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.remaining(), 1024);
}

TEST_F(ShortAllocTest, AllocateExceedingSize) {
    Arena<1024> arena;
    ShortAlloc<int, 1024> alloc(arena);
    EXPECT_THROW(alloc.allocate(1025), std::bad_alloc);
}

TEST_F(ShortAllocTest, ConstructAndDestroy) {
    Arena<1024> arena;
    ShortAlloc<int, 1024> alloc(arena);
    int* ptr = alloc.allocate(1);
    alloc.construct(ptr, 42);
    EXPECT_EQ(*ptr, 42);
    alloc.destroy(ptr);
    alloc.deallocate(ptr, 1);
}

TEST_F(ShortAllocTest, ThreadSafety) {
    Arena<1024> arena;
    ShortAlloc<int, 1024> alloc(arena);
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&alloc]() {
            for (int j = 0; j < 10; ++j) {
                int* ptr = alloc.allocate(10);
                alloc.deallocate(ptr, 10);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(arena.used(), 0);
    EXPECT_EQ(arena.remaining(), 1024);
}

// Tests for allocation strategies
TEST_F(ArenaTest, AllocationStrategies) {
    // First Fit strategy (default)
    Arena<1024, alignof(std::max_align_t), true, AllocationStrategy::FirstFit>
        firstFitArena;

    // Best Fit strategy
    Arena<1024, alignof(std::max_align_t), true, AllocationStrategy::BestFit>
        bestFitArena;

    // Worst Fit strategy
    Arena<1024, alignof(std::max_align_t), true, AllocationStrategy::WorstFit>
        worstFitArena;

    // Create varied sized allocations to test strategies
    void* ptr1 = firstFitArena.allocate(100);
    void* ptr2 = firstFitArena.allocate(50);
    firstFitArena.deallocate(ptr1);

    void* ptr3 = bestFitArena.allocate(100);
    void* ptr4 = bestFitArena.allocate(50);
    bestFitArena.deallocate(ptr3);

    void* ptr5 = worstFitArena.allocate(100);
    void* ptr6 = worstFitArena.allocate(50);
    worstFitArena.deallocate(ptr5);

    // Now allocate a smaller block and check where it goes
    void* ptr7 =
        firstFitArena.allocate(40);  // Should reuse ptr1's space in first fit
    void* ptr8 =
        bestFitArena.allocate(40);  // Should reuse ptr3's space in best fit
    void* ptr9 =
        worstFitArena.allocate(40);  // Should reuse ptr5's space in worst fit

    // Verify all allocations were successful
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr3, nullptr);
    EXPECT_NE(ptr4, nullptr);
    EXPECT_NE(ptr5, nullptr);
    EXPECT_NE(ptr6, nullptr);
    EXPECT_NE(ptr7, nullptr);
    EXPECT_NE(ptr8, nullptr);
    EXPECT_NE(ptr9, nullptr);

    // Clean up
    firstFitArena.deallocate(ptr2);
    firstFitArena.deallocate(ptr7);

    bestFitArena.deallocate(ptr4);
    bestFitArena.deallocate(ptr8);

    worstFitArena.deallocate(ptr6);
    worstFitArena.deallocate(ptr9);
}

// Test defragmentation
TEST_F(ArenaTest, Defragmentation) {
    Arena<1024> arena;

    // Allocate several blocks
    void* ptr1 = arena.allocate(100);
    void* ptr2 = arena.allocate(100);
    void* ptr3 = arena.allocate(100);
    void* ptr4 = arena.allocate(100);

    // Free alternate blocks to create fragmentation
    arena.deallocate(ptr1);
    arena.deallocate(ptr3);

    // Try to allocate a larger block that won't fit in fragmented memory
    EXPECT_THROW(arena.allocate(250), std::bad_alloc);

    // Run defragmentation
    size_t mergeCount = arena.defragment();

    // Defragmentation should have merged at least one block
    EXPECT_GT(mergeCount, 0);

    // Now we should be able to allocate the larger block
    void* ptrLarge = nullptr;
    EXPECT_NO_THROW(ptrLarge = arena.allocate(250));
    EXPECT_NE(ptrLarge, nullptr);

    // Clean up
    arena.deallocate(ptr2);
    arena.deallocate(ptr4);
    arena.deallocate(ptrLarge);
}

// Test for memory corruption detection
TEST_F(ArenaTest, MemoryCorruptionDetection) {
    Arena<1024> arena;

    // Allocate a block
    void* ptr = arena.allocate(100);
    EXPECT_NE(ptr, nullptr);

    // Validate should return true initially
    EXPECT_TRUE(arena.validate());

    // Clean up properly
    arena.deallocate(ptr);
}

// Test for memory alignment
TEST_F(ArenaTest, MemoryAlignment) {
    constexpr size_t customAlignment = 64;
    Arena<1024, customAlignment> arena;

    void* ptr = arena.allocate(100);
    EXPECT_NE(ptr, nullptr);

    // Check if ptr is aligned to customAlignment
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % customAlignment, 0);

    arena.deallocate(ptr);
}

// Test non-thread safe arena
TEST_F(ArenaTest, NonThreadSafeArena) {
    Arena<1024, alignof(std::max_align_t), false> arena;

    void* ptr = arena.allocate(100);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(arena.used(), 100);

    arena.deallocate(ptr);
    EXPECT_EQ(arena.used(), 0);
}

// Test ownership verification
TEST_F(ArenaTest, OwnershipVerification) {
    Arena<1024> arena1;
    Arena<1024> arena2;

    void* ptr1 = arena1.allocate(100);
    void* ptr2 = arena2.allocate(100);

    EXPECT_TRUE(arena1.owns(ptr1));
    EXPECT_FALSE(arena1.owns(ptr2));
    EXPECT_TRUE(arena2.owns(ptr2));
    EXPECT_FALSE(arena2.owns(ptr1));

    arena1.deallocate(ptr1);
    arena2.deallocate(ptr2);
}

// Test allocating many small objects
TEST_F(ArenaTest, ManySmallAllocations) {
    Arena<4096> arena;
    std::vector<void*> pointers;

    // Allocate many small blocks
    for (int i = 0; i < 100; i++) {
        void* ptr = arena.allocate(8);
        EXPECT_NE(ptr, nullptr);
        pointers.push_back(ptr);
    }

    // Deallocate them in reverse order
    for (auto it = pointers.rbegin(); it != pointers.rend(); ++it) {
        arena.deallocate(*it);
    }

    // Check that all memory is returned
    EXPECT_EQ(arena.used(), 0);
}

// Test zero-sized allocations
TEST_F(ArenaTest, ZeroSizeAllocation) {
    Arena<1024> arena;

    void* ptr = arena.allocate(0);
    EXPECT_EQ(ptr, nullptr);

    // Deallocating nullptr should not cause issues
    EXPECT_NO_THROW(arena.deallocate(nullptr));
}

// Test for ShortAlloc with complex types
class ComplexType {
public:
    ComplexType(int v = 0) : value(v), data(new char[128]) {
        memset(data, v, 128);
    }

    ComplexType(const ComplexType& other)
        : value(other.value), data(new char[128]) {
        memcpy(data, other.data, 128);
    }

    ComplexType& operator=(const ComplexType& other) {
        if (this != &other) {
            value = other.value;
            memcpy(data, other.data, 128);
        }
        return *this;
    }

    ~ComplexType() { delete[] data; }

    int getValue() const { return value; }

private:
    int value;
    char* data;
};

TEST_F(ShortAllocTest, ComplexTypeAllocation) {
    Arena<4096> arena;
    ShortAlloc<ComplexType, 4096> alloc(arena);

    // Allocate memory for the complex type
    ComplexType* ptr = alloc.allocate(1);
    EXPECT_NE(ptr, nullptr);

    // Construct the object in-place
    alloc.construct(ptr, 42);

    // Check the object's value
    EXPECT_EQ(ptr->getValue(), 42);

    // Destroy and deallocate
    alloc.destroy(ptr);
    alloc.deallocate(ptr, 1);

    // Check that memory was released
    EXPECT_EQ(arena.used(), 0);
}

// Test allocateUnique helper function
TEST_F(ShortAllocTest, AllocateUnique) {
    Arena<1024> arena;
    ShortAlloc<int, 1024> alloc(arena);

    auto uniquePtr = allocateUnique<ShortAlloc<int, 1024>, int>(alloc, 42);
    EXPECT_NE(uniquePtr, nullptr);
    EXPECT_EQ(*uniquePtr, 42);

    // uniquePtr will automatically release memory when it goes out of scope
}

// Test makeArenaContainer helper function
TEST_F(ShortAllocTest, MakeArenaContainer) {
    Arena<4096> arena;

    // Create a vector using the arena
    auto vec = makeArenaContainer<std::vector, int, 4096>(arena);

    // Add elements
    for (int i = 0; i < 100; i++) {
        vec.push_back(i);
    }

    // Verify elements
    EXPECT_EQ(vec.size(), 100);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(vec[i], i);
    }

    // Check that memory was allocated from the arena
    EXPECT_GT(arena.used(), 0);

    // Clear the vector
    vec.clear();
}

// Test ShortAlloc with STL containers
TEST_F(ShortAllocTest, STLContainers) {
    Arena<16384> arena;

    // Create a vector with arena allocator
    std::vector<int, ShortAlloc<int, 16384>> vec{ShortAlloc<int, 16384>{arena}};

    // Fill the vector
    for (int i = 0; i < 1000; i++) {
        vec.push_back(i);
    }

    // Create a map with arena allocator
    using PairType = std::pair<const int, std::string>;
    std::map<int, std::string, std::less<int>, ShortAlloc<PairType, 16384>> map{
        ShortAlloc<PairType, 16384>{arena}};

    // Fill the map
    for (int i = 0; i < 100; i++) {
        map[i] = "Value " + std::to_string(i);
    }

    // Verify the containers
    EXPECT_EQ(vec.size(), 1000);
    EXPECT_EQ(map.size(), 100);

    // Check that memory was allocated from the arena
    EXPECT_GT(arena.used(), 0);
}

// Test rebind capability of ShortAlloc
TEST_F(ShortAllocTest, RebindAllocator) {
    Arena<8192> arena;
    ShortAlloc<int, 8192> intAlloc(arena);

    // Rebind to a different type
    typename ShortAlloc<int, 8192>::template rebind<double>::other doubleAlloc(
        intAlloc);

    // Allocate with both allocators
    int* intPtr = intAlloc.allocate(1);
    double* doublePtr = doubleAlloc.allocate(1);

    EXPECT_NE(intPtr, nullptr);
    EXPECT_NE(doublePtr, nullptr);

    intAlloc.construct(intPtr, 42);
    doubleAlloc.construct(doublePtr, 3.14159);

    EXPECT_EQ(*intPtr, 42);
    EXPECT_DOUBLE_EQ(*doublePtr, 3.14159);

    doubleAlloc.destroy(doublePtr);
    doubleAlloc.deallocate(doublePtr, 1);
    intAlloc.destroy(intPtr);
    intAlloc.deallocate(intPtr, 1);
}

// Test boundary conditions
TEST_F(ArenaTest, BoundaryConditions) {
    // Very small arena
    Arena<64> smallArena;

    // Allocate to fill the arena
    void* ptr = smallArena.allocate(32);
    EXPECT_NE(ptr, nullptr);

    // Try to allocate more than available
    EXPECT_THROW(smallArena.allocate(32), std::bad_alloc);

    smallArena.deallocate(ptr);

    // Exact fit allocation
    /*
    TODO: Uncomment this test when the allocation strategy is implemented
    void* exactPtr =
        smallArena.allocate(64 - sizeof(typename Arena<64>::Block));
    EXPECT_NE(exactPtr, nullptr);
    smallArena.deallocate(exactPtr);
    */
}

// Test comparison operators for ShortAlloc
TEST_F(ShortAllocTest, ComparisonOperators) {
    Arena<1024> arena1;
    Arena<1024> arena2;

    ShortAlloc<int, 1024> alloc1(arena1);
    ShortAlloc<int, 1024> alloc2(arena1);     // Same arena as alloc1
    ShortAlloc<int, 1024> alloc3(arena2);     // Different arena
    ShortAlloc<double, 1024> alloc4(arena1);  // Different type, same arena

    // Test equality operator
    EXPECT_TRUE(alloc1 == alloc2);
    EXPECT_FALSE(alloc1 == alloc3);
    EXPECT_TRUE(alloc1 ==
                alloc4);  // Same arena, different type should be equal

    // Test inequality operator
    EXPECT_FALSE(alloc1 != alloc2);
    EXPECT_TRUE(alloc1 != alloc3);
    EXPECT_FALSE(alloc1 != alloc4);
}

// Test the memory statistics functionality
TEST_F(ArenaTest, MemoryStatistics) {
#if ATOM_MEMORY_STATS_ENABLED
    Arena<4096> arena;

    // Get initial stats report
    std::string initialStats = arena.getStats();
    EXPECT_FALSE(initialStats.empty());

    // Allocate and deallocate to generate statistics
    void* ptr1 = arena.allocate(1024);
    void* ptr2 = arena.allocate(512);
    arena.deallocate(ptr1);

    // Get updated stats report
    std::string updatedStats = arena.getStats();
    EXPECT_FALSE(updatedStats.empty());
    EXPECT_NE(initialStats, updatedStats);

    // Cleanup
    arena.deallocate(ptr2);
#else
    SUCCEED() << "Memory statistics are disabled in this build";
#endif
}

// Test utils::alignPointer function
TEST(UtilsTest, AlignPointer) {
    char buffer[1024];
    size_t space = 1024;

    // Test alignment to 8 bytes
    void* ptr = utils::alignPointer(buffer, 8, space);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 8, 0);
    EXPECT_LE(space, 1024);

    // Test alignment to 16 bytes
    space = 1024;
    ptr = utils::alignPointer(buffer, 16, space);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 16, 0);
    EXPECT_LE(space, 1024);

    // Test alignment to 32 bytes
    space = 1024;
    ptr = utils::alignPointer(buffer, 32, space);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 32, 0);
    EXPECT_LE(space, 1024);

    // Test with insufficient space
    space = 10;
    ptr = utils::alignPointer(buffer, 64, space);
    if (reinterpret_cast<uintptr_t>(buffer) % 64 <= 10) {
        EXPECT_NE(ptr, nullptr);
    } else {
        EXPECT_EQ(ptr, nullptr);
    }
}

// Test for utils::BoundaryCheck
TEST(UtilsTest, BoundaryCheck) {
    char buffer[1024];
    constexpr size_t size = 1024;

    // Initialize with boundary checks
    utils::BoundaryCheck::initialize(buffer, size);

    // Validate should pass
    EXPECT_TRUE(utils::BoundaryCheck::validate(buffer));

    // Corrupt the start canary
    auto* check =
        static_cast<utils::BoundaryCheck*>(static_cast<void*>(buffer));
    check->startCanary = 0x12345678;

    // Validate should fail
    EXPECT_FALSE(utils::BoundaryCheck::validate(buffer));

    // Restore and corrupt the end canary
    check->startCanary = utils::MEMORY_CANARY;
    size_t* endMarker =
        reinterpret_cast<size_t*>(buffer + check->endCanaryOffset);
    *endMarker = 0x12345678;

    // Validate should fail
    EXPECT_FALSE(utils::BoundaryCheck::validate(buffer));
}

// Test memory fill functions
TEST(UtilsTest, MemoryFill) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // Fill with allocation pattern
    utils::fillMemory(buffer, 1024, utils::getAllocationPattern());

    // Check that the buffer was filled with the allocation pattern
    for (int i = 0; i < 1024; i++) {
        EXPECT_EQ(static_cast<uint8_t>(buffer[i]),
                  utils::getAllocationPattern());
    }

    // Fill with freed pattern
    utils::fillMemory(buffer, 1024, utils::getFreedPattern());

    // Check that the buffer was filled with the freed pattern
    for (int i = 0; i < 1024; i++) {
        EXPECT_EQ(static_cast<uint8_t>(buffer[i]), utils::getFreedPattern());
    }
}