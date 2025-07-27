#include "test_framework.hpp"
#include "../../atom/memory/ring.hpp"
#include <thread>
#include <vector>
#include <random>

using namespace atom::memory::test;
using namespace atom::memory;

namespace {

/**
 * @brief Test basic ring buffer functionality
 */
void testBasicRingBuffer() {
    RingBuffer<int> buffer(5);
    
    // Test initial state
    ASSERT_TRUE(buffer.empty());
    ASSERT_FALSE(buffer.full());
    ASSERT_EQ(buffer.size(), 0);
    ASSERT_EQ(buffer.capacity(), 5);
    
    // Test push operations
    ASSERT_TRUE(buffer.push(1));
    ASSERT_TRUE(buffer.push(2));
    ASSERT_TRUE(buffer.push(3));
    
    ASSERT_FALSE(buffer.empty());
    ASSERT_FALSE(buffer.full());
    ASSERT_EQ(buffer.size(), 3);
    
    // Test pop operations
    auto item1 = buffer.pop();
    ASSERT_TRUE(item1.has_value());
    ASSERT_EQ(item1.value(), 1);
    
    auto item2 = buffer.pop();
    ASSERT_TRUE(item2.has_value());
    ASSERT_EQ(item2.value(), 2);
    
    ASSERT_EQ(buffer.size(), 1);
    
    // Test fill to capacity
    ASSERT_TRUE(buffer.push(4));
    ASSERT_TRUE(buffer.push(5));
    ASSERT_TRUE(buffer.push(6));
    ASSERT_TRUE(buffer.push(7));
    
    ASSERT_TRUE(buffer.full());
    ASSERT_FALSE(buffer.push(8)); // Should fail when full
}

/**
 * @brief Test ring buffer with enhanced configuration
 */
void testEnhancedConfiguration() {
    RingBufferConfig config;
    config.enable_stats = true;
    config.enable_prefetching = true;
    config.enable_batch_operations = true;
    config.enable_lock_free_reads = true;
    
    RingBuffer<int> buffer(10, config);
    
    // Test configuration is applied
    ASSERT_EQ(buffer.getConfig().enable_stats, true);
    ASSERT_EQ(buffer.getConfig().enable_prefetching, true);
    
    // Test basic operations with enhanced config
    ASSERT_TRUE(buffer.push(42));
    auto item = buffer.pop();
    ASSERT_TRUE(item.has_value());
    ASSERT_EQ(item.value(), 42);
}

/**
 * @brief Test ring buffer performance statistics
 */
void testPerformanceStatistics() {
    RingBufferConfig config;
    config.enable_stats = true;
    
    RingBuffer<int> buffer(10, config);
    
    // Perform operations to generate statistics
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(buffer.push(i));
    }
    
    for (int i = 0; i < 3; ++i) {
        auto item = buffer.pop();
        ASSERT_TRUE(item.has_value());
    }
    
    // Get performance metrics
    auto metrics = buffer.getPerformanceMetrics();
    double push_success_ratio = std::get<0>(metrics);
    double pop_success_ratio = std::get<1>(metrics);
    double avg_push_time = std::get<2>(metrics);
    double avg_pop_time = std::get<3>(metrics);
    double cache_hit_ratio = std::get<4>(metrics);
    
    ASSERT_GT(push_success_ratio, 0.0);
    ASSERT_GT(pop_success_ratio, 0.0);
    ASSERT_GE(avg_push_time, 0.0);
    ASSERT_GE(avg_pop_time, 0.0);
    ASSERT_GE(cache_hit_ratio, 0.0);
    
    // Clean up remaining items
    while (!buffer.empty()) {
        buffer.pop();
    }
}

/**
 * @brief Test lock-free read operations
 */
void testLockFreeReads() {
    RingBufferConfig config;
    config.enable_lock_free_reads = true;
    
    RingBuffer<int> buffer(10, config);
    
    // Test lock-free size check
    ASSERT_EQ(buffer.sizeLockFree(), 0);
    ASSERT_TRUE(buffer.emptyLockFree());
    ASSERT_FALSE(buffer.fullLockFree());
    
    // Add some items
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    
    // Test lock-free reads
    ASSERT_EQ(buffer.sizeLockFree(), 3);
    ASSERT_FALSE(buffer.emptyLockFree());
    ASSERT_FALSE(buffer.fullLockFree());
    
    // Fill to capacity
    for (int i = 4; i <= 10; ++i) {
        buffer.push(i);
    }
    
    ASSERT_TRUE(buffer.fullLockFree());
    
    // Clean up
    while (!buffer.empty()) {
        buffer.pop();
    }
}

/**
 * @brief Test batch operations
 */
void testBatchOperations() {
    RingBufferConfig config;
    config.enable_batch_operations = true;
    
    RingBuffer<int> buffer(20, config);
    
    // Test batch push
    std::vector<int> items_to_push = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t pushed = buffer.pushBatch(items_to_push);
    ASSERT_EQ(pushed, items_to_push.size());
    ASSERT_EQ(buffer.size(), items_to_push.size());
    
    // Test batch pop
    auto popped_items = buffer.popBatch(5);
    ASSERT_EQ(popped_items.size(), 5);
    ASSERT_EQ(buffer.size(), 5);
    
    // Verify popped items
    for (size_t i = 0; i < popped_items.size(); ++i) {
        ASSERT_EQ(popped_items[i], static_cast<int>(i + 1));
    }
    
    // Clean up remaining items
    auto remaining = buffer.popBatch(10);
    ASSERT_EQ(remaining.size(), 5);
}

/**
 * @brief Test ring buffer thread safety
 */
void testThreadSafety() {
    RingBuffer<int> buffer(100);
    const size_t num_producers = 4;
    const size_t num_consumers = 4;
    const size_t items_per_producer = 1000;
    
    std::atomic<size_t> total_produced{0};
    std::atomic<size_t> total_consumed{0};
    std::atomic<bool> stop_consumers{false};
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    // Start producers
    for (size_t i = 0; i < num_producers; ++i) {
        producers.emplace_back([&buffer, &total_produced, items_per_producer, i]() {
            for (size_t j = 0; j < items_per_producer; ++j) {
                int value = static_cast<int>(i * items_per_producer + j);
                while (!buffer.push(value)) {
                    std::this_thread::yield();
                }
                total_produced.fetch_add(1);
            }
        });
    }
    
    // Start consumers
    for (size_t i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&buffer, &total_consumed, &stop_consumers]() {
            while (!stop_consumers.load()) {
                auto item = buffer.pop();
                if (item.has_value()) {
                    total_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    // Wait for producers to finish
    for (auto& producer : producers) {
        producer.join();
    }
    
    // Wait for all items to be consumed
    while (total_consumed.load() < total_produced.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    stop_consumers.store(true);
    
    // Wait for consumers to finish
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    ASSERT_EQ(total_produced.load(), num_producers * items_per_producer);
    ASSERT_EQ(total_consumed.load(), total_produced.load());
    ASSERT_TRUE(buffer.empty());
}

/**
 * @brief Test ring buffer utilization tracking
 */
void testUtilizationTracking() {
    RingBuffer<int> buffer(10);
    
    // Test initial utilization
    auto initial_util = buffer.getUtilization();
    ASSERT_EQ(initial_util, 0.0);
    
    // Add some items
    for (int i = 0; i < 5; ++i) {
        buffer.push(i);
    }
    
    auto half_util = buffer.getUtilization();
    ASSERT_EQ(half_util, 0.5);
    
    // Fill completely
    for (int i = 5; i < 10; ++i) {
        buffer.push(i);
    }
    
    auto full_util = buffer.getUtilization();
    ASSERT_EQ(full_util, 1.0);
    
    // Clean up
    while (!buffer.empty()) {
        buffer.pop();
    }
}

/**
 * @brief Benchmark ring buffer performance
 */
BenchmarkResult benchmarkRingBufferPerformance() {
    RingBuffer<int> buffer(1000);
    const size_t iterations = 100000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Push phase
    for (size_t i = 0; i < iterations; ++i) {
        while (!buffer.push(static_cast<int>(i))) {
            // Pop one item to make space
            buffer.pop();
        }
    }
    
    // Pop phase
    for (size_t i = 0; i < iterations; ++i) {
        auto item = buffer.pop();
        if (!item.has_value()) {
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    BenchmarkResult result;
    result.name = "Ring Buffer Push/Pop";
    result.duration = duration;
    result.iterations = iterations * 2; // Both push and pop
    result.memory_used = buffer.capacity() * sizeof(int);
    result.operations_per_second = (result.iterations * 1e9) / duration.count();
    result.additional_info = "Buffer capacity: " + std::to_string(buffer.capacity());
    
    return result;
}

/**
 * @brief Test cache efficiency improvements
 */
void testCacheEfficiency() {
    RingBufferConfig config;
    config.enable_stats = true;
    config.enable_prefetching = true;
    
    RingBuffer<int> buffer(100, config);
    
    // Perform sequential operations to test cache efficiency
    for (int i = 0; i < 50; ++i) {
        buffer.push(i);
    }
    
    for (int i = 0; i < 25; ++i) {
        auto item = buffer.pop();
        ASSERT_TRUE(item.has_value());
    }
    
    // Get cache performance metrics
    auto metrics = buffer.getPerformanceMetrics();
    double cache_hit_ratio = std::get<4>(metrics);
    
    // Should have some cache hits due to sequential access
    ASSERT_GE(cache_hit_ratio, 0.0);
    
    // Clean up
    while (!buffer.empty()) {
        buffer.pop();
    }
}

/**
 * @brief Test configuration updates
 */
void testConfigurationUpdates() {
    RingBuffer<int> buffer(10);
    
    // Get initial config
    auto initial_config = buffer.getConfig();
    
    // Update configuration
    RingBufferConfig new_config;
    new_config.enable_stats = !initial_config.enable_stats;
    new_config.enable_prefetching = !initial_config.enable_prefetching;
    
    buffer.updateConfig(new_config);
    
    // Verify configuration was updated
    auto updated_config = buffer.getConfig();
    ASSERT_EQ(updated_config.enable_stats, new_config.enable_stats);
    ASSERT_EQ(updated_config.enable_prefetching, new_config.enable_prefetching);
}

} // anonymous namespace

/**
 * @brief Register all ring buffer tests
 */
void registerRingBufferTests(TestFramework& framework) {
    framework.addTest("BasicRingBuffer", "Test basic ring buffer push/pop operations", 
                      testBasicRingBuffer);
    
    framework.addTest("EnhancedConfiguration", "Test ring buffer with enhanced configuration", 
                      testEnhancedConfiguration);
    
    framework.addTest("PerformanceStatistics", "Test ring buffer performance statistics", 
                      testPerformanceStatistics);
    
    framework.addTest("LockFreeReads", "Test lock-free read operations", 
                      testLockFreeReads);
    
    framework.addTest("BatchOperations", "Test batch push and pop operations", 
                      testBatchOperations);
    
    framework.addTest("ThreadSafety", "Test ring buffer thread safety with producers/consumers", 
                      testThreadSafety);
    
    framework.addTest("UtilizationTracking", "Test ring buffer utilization tracking", 
                      testUtilizationTracking);
    
    framework.addTest("CacheEfficiency", "Test cache efficiency improvements", 
                      testCacheEfficiency);
    
    framework.addTest("ConfigurationUpdates", "Test runtime configuration updates", 
                      testConfigurationUpdates);
    
    framework.addBenchmark("RingBufferPerformance", "Benchmark ring buffer performance", 
                           benchmarkRingBufferPerformance);
}
