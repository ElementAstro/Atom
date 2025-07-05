/**
 * @file tracker_example.cpp
 * @brief Comprehensive examples of using the MemoryTracker class
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/tracker.hpp"

// Define the memory tracking macro to enable tracking
#ifndef ATOM_MEMORY_TRACKING_ENABLED
#define ATOM_MEMORY_TRACKING_ENABLED
#endif

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Class for testing memory leaks
class MemoryLeakTest {
public:
    MemoryLeakTest(int id, int dataSize) : id_(id), data_(nullptr) {
        std::cout << "Creating MemoryLeakTest object #" << id_ << std::endl;
        data_ = new int[dataSize];
        dataSize_ = dataSize;
    }

    ~MemoryLeakTest() {
        std::cout << "Destroying MemoryLeakTest object #" << id_ << std::endl;
        // Intentionally commented out to simulate a memory leak
        // delete[] data_;
    }

    void setValue(int index, int value) {
        if (index >= 0 && index < dataSize_) {
            data_[index] = value;
        }
    }

    int getValue(int index) const {
        if (index >= 0 && index < dataSize_) {
            return data_[index];
        }
        return -1;
    }

private:
    int id_;
    int* data_;
    int dataSize_;
};

// Class that properly cleans up memory
class ProperCleanupTest {
public:
    ProperCleanupTest(int id, int dataSize)
        : id_(id), data_(new int[dataSize]), dataSize_(dataSize) {
        std::cout << "Creating ProperCleanupTest object #" << id_ << std::endl;
    }

    ~ProperCleanupTest() {
        std::cout << "Destroying ProperCleanupTest object #" << id_
                  << std::endl;
        delete[] data_;  // Properly delete the allocated memory
    }

    void setValue(int index, int value) {
        if (index >= 0 && index < dataSize_) {
            data_[index] = value;
        }
    }

    int getValue(int index) const {
        if (index >= 0 && index < dataSize_) {
            return data_[index];
        }
        return -1;
    }

private:
    int id_;
    int* data_;
    int dataSize_;
};

// Function to perform allocations in a separate thread
void threadAllocationFunc(int id, int count) {
    std::cout << "Thread " << id << " started" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(10 * id));

    // Allocate memory in this thread
    std::vector<void*> allocations;
    for (int i = 0; i < count; ++i) {
        size_t size = 100 + (id * 10) + (i % 50);
        void* ptr = malloc(size);
        ATOM_TRACK_ALLOC(ptr, size);
        allocations.push_back(ptr);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Free half of the allocations
    for (size_t i = 0; i < allocations.size() / 2; ++i) {
        ATOM_TRACK_FREE(allocations[i]);
        free(allocations[i]);
    }

    std::cout << "Thread " << id << " completed" << std::endl;

    // Intentionally leak the rest of the allocations in odd-numbered threads
    if (id % 2 == 0) {
        for (size_t i = allocations.size() / 2; i < allocations.size(); ++i) {
            ATOM_TRACK_FREE(allocations[i]);
            free(allocations[i]);
        }
    }
}

// Custom error callback function
void customErrorCallback(const std::string& errorMessage) {
    std::cerr << "CUSTOM ERROR HANDLER: " << errorMessage << std::endl;
}

// Define a custom allocator that uses the memory tracker
template <typename T>
class TrackedAllocator {
public:
    using value_type = T;

    TrackedAllocator() = default;

    template <typename U>
    TrackedAllocator(const TrackedAllocator<U>&) {}

    T* allocate(std::size_t n) {
        T* ptr = static_cast<T*>(malloc(n * sizeof(T)));
        ATOM_TRACK_ALLOC(ptr, n * sizeof(T));
        return ptr;
    }

    void deallocate(T* ptr, std::size_t n) {
        ATOM_TRACK_FREE(ptr);
        free(ptr);
    }
};

int main() {
    std::cout << "MEMORY TRACKER COMPREHENSIVE EXAMPLES\n";
    std::cout << "====================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Initialization and Configuration
    //--------------------------------------------------------------------------
    printSection("1. Basic Initialization and Configuration");

    // Initialize memory tracker with default settings
    atom::memory::MemoryTracker::instance().initialize();

    std::cout << "Memory tracker initialized with default settings"
              << std::endl;

    // Reset and reinitialize with custom configuration
    atom::memory::MemoryTracker::instance().reset();

    atom::memory::MemoryTrackerConfig config;
    config.enabled = true;
    config.trackStackTrace = true;
    config.autoReportLeaks = true;
    config.logToConsole = true;
    config.logFilePath = "memory_tracker.log";
    config.maxStackFrames = 10;
    config.minAllocationSize = 32;  // Only track allocations >= 32 bytes
    config.trackAllocationCount = true;
    config.trackPeakMemory = true;
    config.errorCallback = customErrorCallback;

    atom::memory::MemoryTracker::instance().initialize(config);

    std::cout << "Memory tracker initialized with custom settings" << std::endl;
    std::cout << "  - Min allocation size: " << config.minAllocationSize
              << " bytes" << std::endl;
    std::cout << "  - Max stack frames: " << config.maxStackFrames << std::endl;
    std::cout << "  - Log file: " << config.logFilePath << std::endl;

    //--------------------------------------------------------------------------
    // 2. Manual Tracking of Memory Allocations
    //--------------------------------------------------------------------------
    printSection("2. Manual Tracking of Memory Allocations");

    // Manually track allocations
    std::cout << "Manually tracking memory allocations..." << std::endl;

    void* ptr1 = malloc(1024);
    ATOM_TRACK_ALLOC(ptr1, 1024);
    std::cout << "Allocated 1024 bytes at " << ptr1 << std::endl;

    void* ptr2 = malloc(2048);
    ATOM_TRACK_ALLOC(ptr2, 2048);
    std::cout << "Allocated 2048 bytes at " << ptr2 << std::endl;

    void* smallPtr = malloc(16);  // Smaller than minAllocationSize
    ATOM_TRACK_ALLOC(smallPtr, 16);
    std::cout << "Allocated 16 bytes at " << smallPtr
              << " (below minimum tracking size)" << std::endl;

    // Manually track deallocations
    std::cout << "\nManually tracking memory deallocations..." << std::endl;

    ATOM_TRACK_FREE(ptr1);
    free(ptr1);
    std::cout << "Deallocated memory at " << ptr1 << std::endl;

    // Intentionally don't free ptr2 to demonstrate leak detection

    ATOM_TRACK_FREE(smallPtr);
    free(smallPtr);
    std::cout << "Deallocated memory at " << smallPtr << std::endl;

    //--------------------------------------------------------------------------
    // 3. Automatic Tracking with Overloaded Operators
    //--------------------------------------------------------------------------
    printSection("3. Automatic Tracking with Overloaded Operators");

    // Reset the tracker to start with clean stats
    atom::memory::MemoryTracker::instance().reset();

    // Allocations using new (automatically tracked)
    std::cout << "Allocating memory using new operators..." << std::endl;

    int* intPtr = new int(42);
    std::cout << "Allocated int with value " << *intPtr << std::endl;

    int* intArrayPtr = new int[100];
    std::cout << "Allocated int array of 100 elements" << std::endl;

    char* charPtr = new (std::nothrow) char[1024];
    std::cout << "Allocated char array of 1024 elements using nothrow"
              << std::endl;

    // Deallocate using delete (automatically tracked)
    std::cout << "\nDeallocating memory using delete operators..." << std::endl;

    delete intPtr;
    std::cout << "Deallocated int pointer" << std::endl;

    delete[] intArrayPtr;
    std::cout << "Deallocated int array pointer" << std::endl;

    // Intentionally don't delete charPtr to demonstrate leak detection

    //--------------------------------------------------------------------------
    // 4. Testing Memory Leaks
    //--------------------------------------------------------------------------
    printSection("4. Testing Memory Leaks");

    {
        std::cout << "Creating objects that leak memory..." << std::endl;

        // Create objects that will leak memory
        MemoryLeakTest* leak1 = new MemoryLeakTest(1, 1000);
        MemoryLeakTest* leak2 = new MemoryLeakTest(2, 2000);

        // Initialize with some values
        leak1->setValue(0, 100);
        leak2->setValue(0, 200);

        std::cout << "leak1 value at index 0: " << leak1->getValue(0)
                  << std::endl;
        std::cout << "leak2 value at index 0: " << leak2->getValue(0)
                  << std::endl;

        // Properly delete one object but not the other
        std::cout << "\nDeleting one object but leaking the other..."
                  << std::endl;
        delete leak1;  // This will be deleted, but internal data_ will still
                       // leak
        // Intentionally don't delete leak2 to demonstrate object leak
    }

    {
        std::cout << "\nCreating objects that properly clean up memory..."
                  << std::endl;

        // Create objects that will properly clean up
        ProperCleanupTest* proper1 = new ProperCleanupTest(1, 1000);
        ProperCleanupTest* proper2 = new ProperCleanupTest(2, 2000);

        // Initialize with some values
        proper1->setValue(0, 300);
        proper2->setValue(0, 400);

        std::cout << "proper1 value at index 0: " << proper1->getValue(0)
                  << std::endl;
        std::cout << "proper2 value at index 0: " << proper2->getValue(0)
                  << std::endl;

        // Properly delete both objects
        std::cout << "\nProperly deleting all objects..." << std::endl;
        delete proper1;
        delete proper2;
    }

    //--------------------------------------------------------------------------
    // 5. Multi-threaded Memory Tracking
    //--------------------------------------------------------------------------
    printSection("5. Multi-threaded Memory Tracking");

    std::cout << "Testing memory tracking in multiple threads..." << std::endl;

    // Create threads to perform allocations
    const int numThreads = 4;
    const int allocsPerThread = 20;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadAllocationFunc, i, allocsPerThread);
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout
        << "\nAll threads completed. Some memory was intentionally leaked."
        << std::endl;

    //--------------------------------------------------------------------------
    // 6. Generating Memory Reports
    //--------------------------------------------------------------------------
    printSection("6. Generating Memory Reports");

    // Generate a memory leak report
    std::cout << "Generating memory leak report..." << std::endl;
    atom::memory::MemoryTracker::instance().reportLeaks();

    // Reset the tracker
    std::cout << "\nResetting memory tracker..." << std::endl;
    atom::memory::MemoryTracker::instance().reset();

    // Allocate some more memory to demonstrate reset worked
    std::cout << "\nAllocating memory after reset..." << std::endl;
    double* doublePtr = new double[50];
    std::cout << "Allocated double array of 50 elements" << std::endl;

    // Generate another report
    std::cout << "\nGenerating updated memory leak report..." << std::endl;
    atom::memory::MemoryTracker::instance().reportLeaks();

    // Clean up the remaining allocation
    delete[] doublePtr;

    //--------------------------------------------------------------------------
    // 7. Testing Edge Cases
    //--------------------------------------------------------------------------
    printSection("7. Testing Edge Cases");

    // Test with nullptr
    std::cout << "Testing tracking with nullptr..." << std::endl;
    ATOM_TRACK_ALLOC(nullptr, 100);  // Should be ignored
    ATOM_TRACK_FREE(nullptr);        // Should be ignored

    // Test double free
    std::cout << "\nTesting double free scenario..." << std::endl;
    void* testPtr = malloc(512);
    ATOM_TRACK_ALLOC(testPtr, 512);
    std::cout << "Allocated 512 bytes at " << testPtr << std::endl;

    ATOM_TRACK_FREE(testPtr);
    free(testPtr);
    std::cout << "Freed memory at " << testPtr << std::endl;

    ATOM_TRACK_FREE(testPtr);  // Double free - should log a warning
    std::cout << "Attempted to free memory at " << testPtr << " again"
              << std::endl;

    // Test freeing untracked memory
    std::cout << "\nTesting freeing untracked memory..." << std::endl;
    void* untrackedPtr = malloc(256);
    std::cout << "Allocated 256 bytes at " << untrackedPtr << " (untracked)"
              << std::endl;

    ATOM_TRACK_FREE(untrackedPtr);  // Untracked free - should log a warning
    free(untrackedPtr);
    std::cout << "Freed untracked memory at " << untrackedPtr << std::endl;

    //--------------------------------------------------------------------------
    // 8. Advanced Error Handling
    //--------------------------------------------------------------------------
    printSection("8. Advanced Error Handling");

    // Reconfigure with custom error handler
    atom::memory::MemoryTrackerConfig advancedConfig = config;
    advancedConfig.errorCallback = [](const std::string& error) {
        std::cerr << "LAMBDA ERROR HANDLER: " << error << std::endl;
        // Could also log to a different file, send an alert, etc.
    };

    atom::memory::MemoryTracker::instance().initialize(advancedConfig);
    std::cout << "Reconfigured memory tracker with lambda error handler"
              << std::endl;

    // Intentionally cause an error by providing invalid file path
    atom::memory::MemoryTrackerConfig invalidConfig = config;
    invalidConfig.logFilePath = "/invalid/path/that/does/not/exist/memory.log";

    std::cout << "\nIntentionally causing an error with invalid file path..."
              << std::endl;
    atom::memory::MemoryTracker::instance().initialize(invalidConfig);

    //--------------------------------------------------------------------------
    // 9. Performance Impact Assessment
    //--------------------------------------------------------------------------
    printSection("9. Performance Impact Assessment");

    // Reset tracker to start with clean stats
    atom::memory::MemoryTracker::instance().reset();

    // Measure performance with tracking enabled
    std::cout << "Measuring performance with memory tracking enabled..."
              << std::endl;

    const int iterations = 100000;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        void* perfPtr = malloc(64);
        ATOM_TRACK_ALLOC(perfPtr, 64);
        ATOM_TRACK_FREE(perfPtr);
        free(perfPtr);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> trackedDuration =
        endTime - startTime;

    std::cout << "Time with tracking: " << trackedDuration.count() << " ms"
              << std::endl;
    std::cout << "Average time per allocation+free: "
              << (trackedDuration.count() / iterations) << " ms" << std::endl;

    // Temporarily disable tracking for comparison
    atom::memory::MemoryTrackerConfig disabledConfig = config;
    disabledConfig.enabled = false;
    atom::memory::MemoryTracker::instance().initialize(disabledConfig);

    std::cout << "\nMeasuring performance with memory tracking disabled..."
              << std::endl;

    startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        void* perfPtr = malloc(64);
        ATOM_TRACK_ALLOC(perfPtr,
                         64);  // This will be ignored due to disabled config
        ATOM_TRACK_FREE(
            perfPtr);  // This will be ignored due to disabled config
        free(perfPtr);
    }

    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> untrackedDuration =
        endTime - startTime;

    std::cout << "Time without tracking: " << untrackedDuration.count() << " ms"
              << std::endl;
    std::cout << "Average time per allocation+free: "
              << (untrackedDuration.count() / iterations) << " ms" << std::endl;

    double overhead =
        (trackedDuration.count() / untrackedDuration.count()) - 1.0;
    std::cout << "Tracking overhead: " << (overhead * 100.0) << "%"
              << std::endl;

    // Re-enable tracking
    atom::memory::MemoryTracker::instance().initialize(config);

    //--------------------------------------------------------------------------
    // 10. Integration with Real-World Scenarios
    //--------------------------------------------------------------------------
    printSection("10. Integration with Real-World Scenarios");

    // Example of tracking custom STL allocator
    std::cout << "Testing with custom STL allocator..." << std::endl;

    // Use the tracked allocator with a vector
    {
        std::cout << "Creating vector with custom tracked allocator..."
                  << std::endl;
        std::vector<int, TrackedAllocator<int>> trackedVector;

        // Add some elements
        for (int i = 0; i < 1000; ++i) {
            trackedVector.push_back(i);
        }

        std::cout << "Vector size: " << trackedVector.size() << std::endl;

        // Vector will be automatically destroyed at the end of scope
        std::cout << "Letting vector go out of scope..." << std::endl;
    }

    // Generate a final leak report
    std::cout << "\nGenerating final memory leak report..." << std::endl;
    atom::memory::MemoryTracker::instance().reportLeaks();

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout << "This example demonstrated the following capabilities:"
              << std::endl;
    std::cout << "  1. Basic initialization and configuration" << std::endl;
    std::cout << "  2. Manual tracking of memory allocations" << std::endl;
    std::cout << "  3. Automatic tracking with overloaded operators"
              << std::endl;
    std::cout << "  4. Testing memory leaks" << std::endl;
    std::cout << "  5. Multi-threaded memory tracking" << std::endl;
    std::cout << "  6. Generating memory reports" << std::endl;
    std::cout << "  7. Testing edge cases" << std::endl;
    std::cout << "  8. Advanced error handling" << std::endl;
    std::cout << "  9. Performance impact assessment" << std::endl;
    std::cout << "  10. Integration with real-world scenarios" << std::endl;

    std::cout
        << "\nNote: Some memory leaks were intentionally created to demonstrate"
        << std::endl;
    std::cout << "the leak detection capabilities of the MemoryTracker."
              << std::endl;

    return 0;
}
