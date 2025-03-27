/**
 * @file memory_pool_example.cpp
 * @brief Comprehensive examples of using the MemoryPool class
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/memory.hpp"

// Utility function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// A sample class that will be allocated from the memory pool
class Widget {
private:
    int id_;
    std::string name_;
    std::vector<double> data_;

public:
    Widget(int id, std::string name, size_t data_size = 10)
        : id_(id), name_(std::move(name)), data_(data_size, 0.0) {
        // Initialize data with some values
        std::iota(data_.begin(), data_.end(), 0.0);
    }

    int getId() const { return id_; }
    const std::string& getName() const { return name_; }

    void setName(const std::string& name) { name_ = name; }

    // Simple operation to use the data
    double calculateSum() const {
        return std::accumulate(data_.begin(), data_.end(), 0.0);
    }

    // Print widget information
    friend std::ostream& operator<<(std::ostream& os, const Widget& widget) {
        os << "Widget{id=" << widget.id_ << ", name='" << widget.name_
           << "', data_size=" << widget.data_.size()
           << ", sum=" << widget.calculateSum() << "}";
        return os;
    }
};

// A class that uses a lot of memory to test large allocations
class LargeObject {
private:
    std::vector<double> huge_array_;
    std::string description_;

public:
    LargeObject(size_t size, std::string desc)
        : huge_array_(size, 1.0), description_(std::move(desc)) {}

    size_t getSize() const { return huge_array_.size(); }
    const std::string& getDescription() const { return description_; }

    friend std::ostream& operator<<(std::ostream& os, const LargeObject& obj) {
        os << "LargeObject{size=" << obj.huge_array_.size() << ", description='"
           << obj.description_ << "'}";
        return os;
    }
};

// Custom block size strategy for testing
class CustomBlockSizeStrategy : public atom::memory::BlockSizeStrategy {
public:
    explicit CustomBlockSizeStrategy(size_t fixed_increment = 8192)
        : increment_(fixed_increment) {}

    [[nodiscard]] size_t calculate(
        size_t requested_size) const noexcept override {
        return requested_size + increment_;
    }

private:
    size_t increment_;
};

// Performance testing function
template <typename PoolType>
void runPerformanceTest(PoolType& pool, const std::string& test_name,
                        size_t iterations) {
    printSection("Performance Test: " + test_name);

    std::vector<Widget*> widgets;
    widgets.reserve(iterations);

    // Time the allocations
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        widgets.push_back(pool.allocate(1));
        new (widgets.back()) Widget(i, "Widget_" + std::to_string(i), 10);
    }

    auto mid = std::chrono::high_resolution_clock::now();

    // Time the deallocations
    for (auto* widget : widgets) {
        widget->~Widget();
        pool.deallocate(widget, 1);
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Calculate and display timing
    auto alloc_time =
        std::chrono::duration_cast<std::chrono::microseconds>(mid - start)
            .count();
    auto dealloc_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - mid)
            .count();
    auto total_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    std::cout << "Allocation time:   " << std::setw(10) << alloc_time << " µs ("
              << std::fixed << std::setprecision(2)
              << (alloc_time / static_cast<double>(iterations)) << " µs/op)\n";
    std::cout << "Deallocation time: " << std::setw(10) << dealloc_time
              << " µs (" << std::fixed << std::setprecision(2)
              << (dealloc_time / static_cast<double>(iterations))
              << " µs/op)\n";
    std::cout << "Total time:        " << std::setw(10) << total_time
              << " µs\n";

    // Print statistics
    std::cout << "\nPool statistics after test:\n";
    std::cout << "Total allocated:   " << pool.getTotalAllocated()
              << " bytes\n";
    std::cout << "Total available:   " << pool.getTotalAvailable()
              << " bytes\n";
    std::cout << "Allocation count:  " << pool.getAllocationCount() << "\n";
    std::cout << "Deallocation count:" << pool.getDeallocationCount() << "\n";
    std::cout << "Fragmentation:     " << std::fixed << std::setprecision(4)
              << pool.getFragmentationRatio() * 100 << "%\n";
}

// Thread safety test function
void threadSafetyTest(MemoryPool<Widget>& pool, int thread_id, int iterations) {
    std::vector<Widget*> local_widgets;
    local_widgets.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        int widget_id = thread_id * 1000 + i;
        Widget* widget = pool.allocate(1);
        new (widget) Widget(widget_id, "Thread_" + std::to_string(thread_id) +
                                           "_Widget_" + std::to_string(i));
        local_widgets.push_back(widget);

        // Introduce some random pauses to increase thread interleaving
        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Deallocate half of the widgets
    for (int i = 0; i < iterations / 2; ++i) {
        local_widgets[i]->~Widget();
        pool.deallocate(local_widgets[i], 1);
        local_widgets[i] = nullptr;
    }

    // Allocate some more
    for (int i = 0; i < iterations / 4; ++i) {
        int widget_id = thread_id * 2000 + i;
        Widget* widget = pool.allocate(1);
        new (widget) Widget(widget_id, "Thread_" + std::to_string(thread_id) +
                                           "_SecondBatch_" + std::to_string(i));
        local_widgets.push_back(widget);
    }

    // Deallocate remaining widgets
    for (auto* widget : local_widgets) {
        if (widget) {
            widget->~Widget();
            pool.deallocate(widget, 1);
        }
    }
}

int main() {
    std::cout << "MEMORY POOL COMPREHENSIVE EXAMPLES\n";
    std::cout << "==================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Usage");

    // Create a memory pool for Widget objects with default block size
    MemoryPool<Widget> widgetPool;

    std::cout << "Initialized Widget memory pool\n";
    std::cout << "Default block size: 4096 bytes\n";
    std::cout << "Widget size: " << sizeof(Widget) << " bytes\n";

    // Allocate some widgets
    Widget* widget1 = widgetPool.allocate(1);
    new (widget1) Widget(1, "First Widget");

    Widget* widget2 = widgetPool.allocate(1);
    new (widget2) Widget(2, "Second Widget", 20);

    Widget* widget3 = widgetPool.allocate(1);
    new (widget3) Widget(3, "Third Widget", 30);

    std::cout << "\nAllocated 3 widgets from pool:\n";
    std::cout << "Widget 1: " << *widget1 << "\n";
    std::cout << "Widget 2: " << *widget2 << "\n";
    std::cout << "Widget 3: " << *widget3 << "\n";

    // Deallocate one widget
    widget2->~Widget();
    widgetPool.deallocate(widget2, 1);

    std::cout << "\nDeallocated Widget 2\n";

    // Allocate another widget (should reuse the space from widget2)
    Widget* widget4 = widgetPool.allocate(1);
    new (widget4) Widget(4, "Fourth Widget (reusing space)", 25);

    std::cout << "Allocated Widget 4 (should reuse memory): " << *widget4
              << "\n";

    // Cleanup
    widget1->~Widget();
    widget3->~Widget();
    widget4->~Widget();

    widgetPool.deallocate(widget1, 1);
    widgetPool.deallocate(widget3, 1);
    widgetPool.deallocate(widget4, 1);

    std::cout << "\nAll widgets deallocated\n";

    //--------------------------------------------------------------------------
    // 2. Custom Block Size Strategy
    //--------------------------------------------------------------------------
    printSection("2. Custom Block Size Strategy");

    // Create a memory pool with a custom block size strategy
    auto customStrategy = std::make_unique<CustomBlockSizeStrategy>(16384);
    MemoryPool<Widget, 8192> customWidgetPool(std::move(customStrategy));

    std::cout << "Created memory pool with custom block size strategy\n";
    std::cout << "Initial block size: 8192 bytes\n";
    std::cout << "Strategy: Fixed increment of 16384 bytes\n";

    // Allocate a bunch of widgets to force multiple blocks
    constexpr int kWidgetsToAllocate = 50;
    std::vector<Widget*> widgets;

    for (int i = 0; i < kWidgetsToAllocate; ++i) {
        Widget* w = customWidgetPool.allocate(1);
        new (w) Widget(i, "CustomPool_Widget_" + std::to_string(i), i * 5);
        widgets.push_back(w);
    }

    std::cout << "\nAllocated " << kWidgetsToAllocate << " widgets\n";
    std::cout << "First widget: " << *widgets.front() << "\n";
    std::cout << "Last widget: " << *widgets.back() << "\n";

    // Print stats to see multiple chunks were created
    std::cout << "\nPool statistics:\n";
    std::cout << "Total allocated: " << customWidgetPool.getTotalAllocated()
              << " bytes\n";
    std::cout << "Total available: " << customWidgetPool.getTotalAvailable()
              << " bytes\n";
    std::cout << "Allocation count: " << customWidgetPool.getAllocationCount()
              << "\n";

    // Cleanup
    for (auto* w : widgets) {
        w->~Widget();
        customWidgetPool.deallocate(w, 1);
    }

    std::cout << "\nAll widgets deallocated\n";

    //--------------------------------------------------------------------------
    // 3. Tagged Allocations for Debugging
    //--------------------------------------------------------------------------
    printSection("3. Tagged Allocations for Debugging");

    // Create a pool for debugging
    MemoryPool<Widget> debugPool;

    // Allocate with tags
    Widget* debug_widget1 =
        debugPool.allocateTagged(1, "UI_Widget", "ui_module.cpp", 42);
    new (debug_widget1) Widget(101, "UI Button");

    Widget* debug_widget2 =
        debugPool.allocateTagged(1, "Logic_Widget", "business_logic.cpp", 128);
    new (debug_widget2) Widget(102, "Data Processor");

    Widget* debug_widget3 =
        debugPool.allocateTagged(1, "Network_Widget", "network.cpp", 256);
    new (debug_widget3) Widget(103, "Connection Manager");

    std::cout << "Allocated 3 tagged widgets for debugging\n";

    // Find tag information
    auto tag1 = debugPool.findTag(debug_widget1);
    auto tag2 = debugPool.findTag(debug_widget2);
    auto tag3 = debugPool.findTag(debug_widget3);

    std::cout << "\nTag information retrieved:\n";

    if (tag1) {
        std::cout << "Widget 1 tag: " << tag1->name << " (in " << tag1->file
                  << ", line " << tag1->line << ")\n";
    }

    if (tag2) {
        std::cout << "Widget 2 tag: " << tag2->name << " (in " << tag2->file
                  << ", line " << tag2->line << ")\n";
    }

    if (tag3) {
        std::cout << "Widget 3 tag: " << tag3->name << " (in " << tag3->file
                  << ", line " << tag3->line << ")\n";
    }

    // Get all tagged allocations
    auto all_tags = debugPool.getTaggedAllocations();
    std::cout << "\nTotal tagged allocations: " << all_tags.size() << "\n";

    // Cleanup
    debug_widget1->~Widget();
    debug_widget2->~Widget();
    debug_widget3->~Widget();

    debugPool.deallocate(debug_widget1, 1);
    debugPool.deallocate(debug_widget2, 1);
    debugPool.deallocate(debug_widget3, 1);

    std::cout << "\nAll tagged widgets deallocated\n";

    //--------------------------------------------------------------------------
    // 4. Memory Pool for Large Objects
    //--------------------------------------------------------------------------
    printSection("4. Memory Pool for Large Objects");

    // Create a pool with larger block size for storing large objects
    constexpr size_t kLargeBlockSize = 1024 * 1024;  // 1MB blocks
    MemoryPool<LargeObject, kLargeBlockSize> largeObjectPool;

    std::cout << "Created pool for large objects with " << kLargeBlockSize
              << " byte blocks\n";

    // Allocate some large objects
    LargeObject* large1 = largeObjectPool.allocate(1);
    new (large1) LargeObject(100000, "Big Data Set");

    LargeObject* large2 = largeObjectPool.allocate(1);
    new (large2) LargeObject(200000, "Huge Array");

    LargeObject* large3 = largeObjectPool.allocate(1);
    new (large3) LargeObject(300000, "Massive Collection");

    std::cout << "\nAllocated large objects:\n";
    std::cout << "Object 1: " << *large1 << "\n";
    std::cout << "Object 2: " << *large2 << "\n";
    std::cout << "Object 3: " << *large3 << "\n";

    // Check statistics
    std::cout << "\nPool statistics:\n";
    std::cout << "Total allocated: " << largeObjectPool.getTotalAllocated()
              << " bytes\n";
    std::cout << "Total available: " << largeObjectPool.getTotalAvailable()
              << " bytes\n";

    // Cleanup
    large1->~LargeObject();
    large2->~LargeObject();
    large3->~LargeObject();

    largeObjectPool.deallocate(large1, 1);
    largeObjectPool.deallocate(large2, 1);
    largeObjectPool.deallocate(large3, 1);

    std::cout << "\nAll large objects deallocated\n";

    //--------------------------------------------------------------------------
    // 5. Pool Reset and Compaction
    //--------------------------------------------------------------------------
    printSection("5. Pool Reset and Compaction");

    // Create a pool for testing reset and compaction
    MemoryPool<int, 4096> intPool;

    // Allocate a bunch of integers
    std::vector<int*> intPtrs;
    for (int i = 0; i < 1000; ++i) {
        int* p = intPool.allocate(1);
        *p = i;
        intPtrs.push_back(p);
    }

    std::cout << "Allocated 1000 integers\n";
    std::cout << "Total allocated: " << intPool.getTotalAllocated()
              << " bytes\n";

    // Deallocate every other integer to create fragmentation
    for (size_t i = 0; i < intPtrs.size(); i += 2) {
        intPool.deallocate(intPtrs[i], 1);
        intPtrs[i] = nullptr;
    }

    std::cout << "\nDeallocated every other integer to create fragmentation\n";
    std::cout << "Fragmentation ratio: " << std::fixed << std::setprecision(4)
              << intPool.getFragmentationRatio() * 100 << "%\n";

    // Compact the pool
    size_t bytes_compacted = intPool.compact();

    std::cout << "\nCompacted pool, merged " << bytes_compacted
              << " bytes of free space\n";
    std::cout << "New fragmentation ratio: " << std::fixed
              << std::setprecision(4) << intPool.getFragmentationRatio() * 100
              << "%\n";

    // Reset the pool
    intPool.reset();

    std::cout << "\nReset pool to initial state\n";
    std::cout << "Total allocated: " << intPool.getTotalAllocated()
              << " bytes\n";
    std::cout << "Total available: " << intPool.getTotalAvailable()
              << " bytes\n";

    // Cleanup remaining pointers (after reset, these are invalid)
    intPtrs.clear();

    //--------------------------------------------------------------------------
    // 6. Thread Safety Testing
    //--------------------------------------------------------------------------
    printSection("6. Thread Safety Testing");

    // Create a shared pool for thread testing
    MemoryPool<Widget, 16384> threadSafePool;

    // Reserve space to reduce allocations during thread testing
    threadSafePool.reserve(1000, sizeof(Widget));

    std::cout
        << "Created thread-safe pool and reserved space for 1000 widgets\n";

    // Create multiple threads that allocate/deallocate from the same pool
    constexpr int kNumThreads = 4;
    constexpr int kWidgetsPerThread = 200;

    std::vector<std::thread> threads;

    std::cout << "Starting " << kNumThreads << " threads, each working with "
              << kWidgetsPerThread << " widgets\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(threadSafetyTest, std::ref(threadSafePool), i,
                             kWidgetsPerThread);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "\nAll threads completed in " << elapsed << " ms\n";

    // Check for memory leaks or other issues
    std::cout << "Pool statistics after thread test:\n";
    std::cout << "Total allocated: " << threadSafePool.getTotalAllocated()
              << " bytes\n";
    std::cout << "Total available: " << threadSafePool.getTotalAvailable()
              << " bytes\n";
    std::cout << "Allocation count: " << threadSafePool.getAllocationCount()
              << "\n";
    std::cout << "Deallocation count: " << threadSafePool.getDeallocationCount()
              << "\n";

    // Verify allocations and deallocations match
    if (threadSafePool.getAllocationCount() ==
        threadSafePool.getDeallocationCount()) {
        std::cout << "SUCCESS: All allocations were properly deallocated\n";
    } else {
        std::cout
            << "WARNING: Mismatch between allocations and deallocations!\n";
    }

    //--------------------------------------------------------------------------
    // 7. Comparison with Standard Allocator
    //--------------------------------------------------------------------------
    printSection("7. Comparison with Standard Allocator");

    constexpr size_t kTestIterations = 10000;

    // Test MemoryPool
    MemoryPool<Widget, 1024 * 1024> benchmarkPool;  // 1MB blocks
    runPerformanceTest(benchmarkPool, "MemoryPool Allocator", kTestIterations);

    // Create an equivalent adapter for standard allocator
    struct StdAllocatorAdapter {
        Widget* allocate(size_t n) {
            return static_cast<Widget*>(::operator new(n * sizeof(Widget)));
        }

        void deallocate(Widget* p, size_t) { ::operator delete(p); }

        size_t getTotalAllocated() const { return 0; }
        size_t getTotalAvailable() const { return 0; }
        size_t getAllocationCount() const { return 0; }
        size_t getDeallocationCount() const { return 0; }
        double getFragmentationRatio() const { return 0.0; }
    };

    // Test standard allocator
    StdAllocatorAdapter stdAllocator;
    runPerformanceTest(stdAllocator, "Standard Allocator", kTestIterations);

    //--------------------------------------------------------------------------
    // 8. PMR Memory Resource Interface
    //--------------------------------------------------------------------------
    printSection("8. PMR Memory Resource Interface");

    // Create a memory pool as pmr::memory_resource
    MemoryPool<std::byte, 4096>* resourcePool =
        new MemoryPool<std::byte, 4096>();

    // Create a polymorphic allocator using our memory resource
    std::pmr::polymorphic_allocator<Widget> pmr_alloc(resourcePool);

    std::cout << "Created MemoryPool as a PMR memory resource\n";

    // Allocate using the PMR interface
    Widget* pmr_widget1 = pmr_alloc.allocate(1);
    std::construct_at(pmr_widget1, 201, "PMR Widget 1");

    Widget* pmr_widget2 = pmr_alloc.allocate(1);
    std::construct_at(pmr_widget2, 202, "PMR Widget 2");

    std::cout << "\nAllocated widgets using PMR interface:\n";
    std::cout << "Widget 1: " << *pmr_widget1 << "\n";
    std::cout << "Widget 2: " << *pmr_widget2 << "\n";

    // Deallocate
    std::destroy_at(pmr_widget1);
    std::destroy_at(pmr_widget2);
    pmr_alloc.deallocate(pmr_widget1, 1);
    pmr_alloc.deallocate(pmr_widget2, 1);

    std::cout << "\nDeallocated PMR widgets\n";

    // Create a PMR container that uses our memory resource
    std::pmr::vector<int> vec(resourcePool);

    std::cout << "\nCreated PMR vector using our memory resource\n";

    // Add some elements
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }

    std::cout << "Added 100 elements to PMR vector\n";
    std::cout << "First element: " << vec.front() << "\n";
    std::cout << "Last element: " << vec.back() << "\n";

    // Clean up the memory resource (which also clears the memory pool)
    delete resourcePool;

    std::cout << "\nMemory resource cleaned up\n";

    std::cout << "\nAll Memory Pool examples completed successfully!\n";

    return 0;
}