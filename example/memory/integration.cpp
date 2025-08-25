/**
 * @file integration_example.cpp
 * @brief Integration examples showing how different memory components work
 * together
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/memory.hpp"
#include "atom/memory/memory_pool.hpp"
#include "atom/memory/object.hpp"
#include "atom/memory/ring.hpp"
#include "atom/memory/shared.hpp"
#include "atom/memory/tracker.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Example data structures for integration
struct NetworkPacket {
    uint32_t id;
    uint32_t size;
    std::chrono::steady_clock::time_point timestamp;
    std::vector<uint8_t> data;

    NetworkPacket()
        : id(0), size(0), timestamp(std::chrono::steady_clock::now()) {}

    NetworkPacket(uint32_t packet_id, const std::vector<uint8_t>& packet_data)
        : id(packet_id),
          size(packet_data.size()),
          timestamp(std::chrono::steady_clock::now()),
          data(packet_data) {}

    void reset() {
        id = 0;
        size = 0;
        timestamp = std::chrono::steady_clock::now();
        data.clear();
    }

    bool isValid() const { return id > 0 && size > 0 && !data.empty(); }
};

// Custom allocator using MemoryPool
template <typename T>
class PoolAllocator {
private:
    static MemoryPool<T>* pool_;

public:
    using value_type = T;

    PoolAllocator() = default;
    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) {}

    T* allocate(std::size_t n) {
        if (!pool_) {
            pool_ = new MemoryPool<T>();
        }
        return pool_->allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        if (pool_) {
            pool_->deallocate(p, n);
        }
    }

    template <typename U>
    bool operator==(const PoolAllocator<U>&) const {
        return true;
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U>&) const {
        return false;
    }
};

template <typename T>
MemoryPool<T>* PoolAllocator<T>::pool_ = nullptr;

int main() {
    std::cout << "MEMORY COMPONENTS INTEGRATION EXAMPLES\n";
    std::cout << "=====================================\n";

    //--------------------------------------------------------------------------
    // 1. High-Performance Network Packet Processing System
    //--------------------------------------------------------------------------
    printSection("1. High-Performance Network Packet Processing System");

    std::cout << "Creating a network packet processing system using multiple "
                 "memory components..."
              << std::endl;

    // Initialize memory tracking
    atom::memory::MemoryTracker::instance().initialize();

    // Create object pool for packet reuse
    atom::memory::ObjectPool<NetworkPacket> packetPool(100, 10);

    // Create ring buffer for packet queue
    atom::memory::RingBuffer<NetworkPacket> packetQueue(1000);

    // Create fixed-size memory pool for small allocations
    constexpr size_t SmallBlockSize = 256;
    constexpr size_t BlocksPerChunk = 1000;
    atom::memory::MemoryPool<SmallBlockSize, BlocksPerChunk> smallPool;

    std::cout << "System components initialized:" << std::endl;
    std::cout << "  - Object pool: 100 packets (configured)" << std::endl;
    std::cout << "  - Ring buffer: " << packetQueue.capacity() << " queue slots"
              << std::endl;
    std::cout << "  - Memory pool: " << BlocksPerChunk << " blocks of "
              << SmallBlockSize << " bytes" << std::endl;

    // Simulate packet processing
    std::cout << "\nSimulating packet processing..." << std::endl;

    for (int i = 0; i < 50; ++i) {
        // Acquire packet from pool
        auto packet = packetPool.acquire();

        // Initialize packet with simulated data
        std::vector<uint8_t> data(100 + (i % 200),
                                  static_cast<uint8_t>(i % 256));
        *packet = NetworkPacket(i + 1, data);

        // Add to processing queue
        packetQueue.push(*packet);

        if (i % 10 == 0) {
            std::cout << "  Processed " << (i + 1) << " packets" << std::endl;
        }
    }

    std::cout << "Queue size after processing: " << packetQueue.size()
              << std::endl;

    // Process packets from queue
    std::cout << "\nProcessing packets from queue..." << std::endl;
    int processed = 0;
    while (!packetQueue.empty()) {
        auto packet_opt = packetQueue.front();
        packetQueue.pop();

        // Simulate processing
        if (packet_opt && packet_opt->id > 0) {
            processed++;
        }
    }

    std::cout << "Successfully processed " << processed << " packets"
              << std::endl;

    //--------------------------------------------------------------------------
    // 2. Memory-Efficient Data Cache with Tracking
    //--------------------------------------------------------------------------
    printSection("2. Memory-Efficient Data Cache with Tracking");

    std::cout << "Creating a data cache using memory pool and tracking..."
              << std::endl;

    // Reset memory tracker for this example
    atom::memory::MemoryTracker::instance().reset();

    // Create a cache using ring buffer and memory pool
    struct CacheEntry {
        std::string key;
        std::vector<uint8_t> value;
        std::chrono::steady_clock::time_point access_time;

        CacheEntry() : access_time(std::chrono::steady_clock::now()) {}

        CacheEntry(const std::string& k, const std::vector<uint8_t>& v)
            : key(k), value(v), access_time(std::chrono::steady_clock::now()) {}
    };

    // Create cache using ring buffer
    atom::memory::RingBuffer<CacheEntry> cache(100);

    std::cout << "Cache initialized with capacity: " << cache.capacity()
              << std::endl;

    // Add entries to cache
    std::cout << "\nAdding entries to cache..." << std::endl;
    for (int i = 0; i < 150; ++i) {  // More than capacity to test overwrite
        std::string key = "key_" + std::to_string(i);
        std::vector<uint8_t> value(50, static_cast<uint8_t>(i % 256));

        cache.push(CacheEntry(key, value));

        if (i % 25 == 0) {
            std::cout << "  Added " << (i + 1) << " entries" << std::endl;
        }
    }

    std::cout << "Final cache size: " << cache.size() << std::endl;

    // Search for specific entries
    std::cout << "\nSearching for specific entries..." << std::endl;
    std::vector<std::string> searchKeys = {"key_140", "key_50", "key_149"};

    for (const auto& searchKey : searchKeys) {
        bool found = false;
        for (const auto& entry : cache) {
            if (entry.key == searchKey) {
                std::cout << "  Found " << searchKey
                          << " with value size: " << entry.value.size()
                          << std::endl;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "  " << searchKey
                      << " not found (may have been overwritten)" << std::endl;
        }
    }

    // Generate memory report
    std::cout << "\nMemory usage report:" << std::endl;
    atom::memory::MemoryTracker::instance().reportLeaks();

    //--------------------------------------------------------------------------
    // 3. Shared Memory Communication with Object Pool
    //--------------------------------------------------------------------------
    printSection("3. Shared Memory Communication with Object Pool");

    std::cout << "Demonstrating shared memory with object pool for IPC..."
              << std::endl;

    // Define shared data structure
    struct SharedData {
        int message_id;
        char message[256];
        bool ready;

        SharedData() : message_id(0), ready(false) {
            std::memset(message, 0, sizeof(message));
        }
    };

    try {
        // Create shared memory
        const std::string shm_name = "integration_example";
        atom::connection::SharedMemory<SharedData> sharedMem(shm_name, true);

        // Create a simple message class that implements reset
        struct Message {
            std::string content;
            void reset() { content.clear(); }
        };

        // Create object pool for message processing
        atom::memory::ObjectPool<Message> messagePool(50, 10);

        std::cout << "Shared memory and message pool created" << std::endl;

        // Simulate message exchange
        std::cout << "\nSimulating message exchange..." << std::endl;

        for (int i = 0; i < 10; ++i) {
            // Acquire message object from pool
            auto message = messagePool.acquire();
            message->content = "Message " + std::to_string(i + 1) +
                               " from integration example";

            // Write to shared memory
            SharedData data;
            data.message_id = i + 1;
            std::strncpy(data.message, message->content.c_str(),
                         sizeof(data.message) - 1);
            data.ready = true;

            sharedMem.write(data);
            std::cout << "  Sent: " << message->content << std::endl;

            // Simulate processing delay
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Read back from shared memory
            try {
                SharedData readData =
                    sharedMem.read(std::chrono::milliseconds(100));
                if (readData.ready) {
                    std::cout << "  Received: ID=" << readData.message_id
                              << ", Message=" << readData.message << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "  Failed to read from shared memory: " << e.what()
                          << std::endl;
            }

            // Message automatically returns to pool when shared_ptr is
            // destroyed
        }

        std::cout << "Message exchange completed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Shared memory example failed: " << e.what() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 4. Memory Tracking Integration
    //--------------------------------------------------------------------------
    printSection("4. Memory Tracking Integration");

    std::cout << "Demonstrating memory tracking across all components..."
              << std::endl;

    // Generate final memory report
    std::cout << "\nFinal memory usage report:" << std::endl;
    atom::memory::MemoryTracker::instance().reportLeaks();

    std::cout << "\nMemory tracking has been monitoring allocations throughout"
              << std::endl;
    std::cout << "the execution of all integration examples." << std::endl;

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout << "This integration example demonstrated:" << std::endl;
    std::cout << "  1. High-performance network packet processing using "
                 "ObjectPool and RingBuffer"
              << std::endl;
    std::cout << "  2. Memory-efficient data cache with tracking and custom "
                 "allocators"
              << std::endl;
    std::cout << "  3. Shared memory communication combined with object pooling"
              << std::endl;
    std::cout << "  4. Comprehensive memory tracking across all components"
              << std::endl;
    std::cout
        << "\nThese examples show how the memory components can be combined"
        << std::endl;
    std::cout
        << "to create efficient, high-performance memory management solutions"
        << std::endl;
    std::cout << "for real-world applications." << std::endl;

    return 0;
}
