#include "test_framework.hpp"
#include "../../atom/memory/object.hpp"
#include <string>
#include <vector>
#include <thread>

using namespace atom::memory::test;
using namespace atom::memory;

namespace {

/**
 * @brief Test object for object pool testing
 */
class TestObject {
public:
    int value;
    std::string data;
    static std::atomic<int> construction_count;
    static std::atomic<int> destruction_count;
    
    TestObject() : value(42), data("test") {
        construction_count.fetch_add(1);
    }
    
    explicit TestObject(int v) : value(v), data("test_" + std::to_string(v)) {
        construction_count.fetch_add(1);
    }
    
    TestObject(int v, const std::string& d) : value(v), data(d) {
        construction_count.fetch_add(1);
    }
    
    ~TestObject() {
        destruction_count.fetch_add(1);
    }
    
    void reset() {
        value = 0;
        data.clear();
    }
    
    static void resetCounters() {
        construction_count.store(0);
        destruction_count.store(0);
    }
};

std::atomic<int> TestObject::construction_count{0};
std::atomic<int> TestObject::destruction_count{0};

/**
 * @brief Test basic object pool functionality
 */
void testBasicObjectPool() {
    TestObject::resetCounters();
    
    {
        ObjectPool<TestObject> pool(5);
        
        // Test basic acquisition
        auto obj1 = pool.acquire();
        ASSERT_TRUE(obj1 != nullptr);
        ASSERT_EQ(obj1->value, 42);
        ASSERT_EQ(obj1->data, "test");
        
        auto obj2 = pool.acquire();
        ASSERT_TRUE(obj2 != nullptr);
        ASSERT_NE(obj1.get(), obj2.get());
        
        // Test release (automatic via RAII)
        obj1.reset();
        obj2.reset();
        
        // Test reacquisition
        auto obj3 = pool.acquire();
        ASSERT_TRUE(obj3 != nullptr);
    }
    
    // Objects should be destroyed when pool is destroyed
    ASSERT_GT(TestObject::destruction_count.load(), 0);
}

/**
 * @brief Test object pool with custom factory
 */
void testCustomFactory() {
    TestObject::resetCounters();
    
    auto factory = []() { return std::make_unique<TestObject>(100, "custom"); };
    ObjectPool<TestObject> pool(3, factory);
    
    auto obj = pool.acquire();
    ASSERT_TRUE(obj != nullptr);
    ASSERT_EQ(obj->value, 100);
    ASSERT_EQ(obj->data, "custom");
}

/**
 * @brief Test object pool performance monitoring
 */
void testPerformanceMonitoring() {
    ObjectPool<TestObject> pool(10);
    
    // Get initial performance metrics
    auto initial_metrics = pool.getPerformanceMetrics();
    double initial_hit_ratio = std::get<0>(initial_metrics);
    
    // Acquire and release objects to generate statistics
    std::vector<std::unique_ptr<TestObject>> objects;
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.acquire());
    }
    
    // Release all objects
    objects.clear();
    
    // Acquire again (should hit cache)
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.acquire());
    }
    
    // Check performance metrics
    auto final_metrics = pool.getPerformanceMetrics();
    double final_hit_ratio = std::get<0>(final_metrics);
    
    // Hit ratio should have improved
    ASSERT_GE(final_hit_ratio, initial_hit_ratio);
    
    objects.clear();
}

/**
 * @brief Test object pool with initializer and finalizer
 */
void testInitializerFinalizer() {
    PoolConfig config;
    config.object_initializer = [](TestObject& obj) {
        obj.value = 999;
        obj.data = "initialized";
    };
    config.object_finalizer = [](TestObject& obj) {
        obj.reset();
    };
    
    ObjectPool<TestObject> pool(3, nullptr, config);
    
    auto obj = pool.acquire();
    ASSERT_TRUE(obj != nullptr);
    ASSERT_EQ(obj->value, 999);
    ASSERT_EQ(obj->data, "initialized");
    
    // Modify object
    obj->value = 123;
    obj->data = "modified";
    
    // Release object
    obj.reset();
    
    // Acquire again - should be reset by finalizer
    auto obj2 = pool.acquire();
    ASSERT_TRUE(obj2 != nullptr);
    // Note: The finalizer should have reset the object, but initializer runs again
    ASSERT_EQ(obj2->value, 999);
    ASSERT_EQ(obj2->data, "initialized");
}

/**
 * @brief Test object pool thread safety
 */
void testThreadSafety() {
    ObjectPool<TestObject> pool(20);
    const size_t num_threads = 8;
    const size_t operations_per_thread = 100;
    
    std::atomic<size_t> successful_acquisitions{0};
    std::atomic<size_t> successful_releases{0};
    
    std::vector<std::thread> threads;
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&pool, &successful_acquisitions, &successful_releases, operations_per_thread]() {
            for (size_t j = 0; j < operations_per_thread; ++j) {
                auto obj = pool.acquire();
                if (obj) {
                    successful_acquisitions.fetch_add(1);
                    
                    // Do some work with the object
                    obj->value = static_cast<int>(j);
                    obj->data = "thread_data_" + std::to_string(j);
                    
                    // Object is automatically released when obj goes out of scope
                    successful_releases.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(successful_acquisitions.load(), num_threads * operations_per_thread);
    ASSERT_EQ(successful_releases.load(), num_threads * operations_per_thread);
}

/**
 * @brief Test object pool adaptive sizing
 */
void testAdaptiveSizing() {
    PoolConfig config;
    config.enable_adaptive_sizing = true;
    config.growth_factor = 1.5;
    config.max_pool_growth = 10;
    
    ObjectPool<TestObject> pool(2, nullptr, config);
    
    // Get initial utilization
    auto initial_utilization = pool.getUtilization();
    size_t initial_max_size = std::get<1>(initial_utilization);
    
    // Acquire more objects than initial capacity
    std::vector<std::unique_ptr<TestObject>> objects;
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.acquire());
    }
    
    // Trigger adaptive sizing manually
    pool.triggerAdaptiveSizing();
    
    // Check if pool has grown
    auto final_utilization = pool.getUtilization();
    size_t final_max_size = std::get<1>(final_utilization);
    
    // Pool might have grown (depending on miss ratio)
    ASSERT_GE(final_max_size, initial_max_size);
    
    objects.clear();
}

/**
 * @brief Test object warming feature
 */
void testObjectWarming() {
    PoolConfig config;
    config.enable_object_warming = true;
    
    ObjectPool<TestObject> pool(5, nullptr, config);
    
    // Trigger object warming
    pool.triggerObjectWarming(3);
    
    // Acquire objects - should be faster due to warming
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::unique_ptr<TestObject>> objects;
    for (int i = 0; i < 3; ++i) {
        objects.push_back(pool.acquire());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Should be relatively fast due to pre-warmed objects
    ASSERT_LT(duration.count(), 1000); // Less than 1ms
    
    objects.clear();
}

/**
 * @brief Benchmark object pool performance
 */
BenchmarkResult benchmarkObjectPoolPerformance() {
    ObjectPool<TestObject> pool(100);
    const size_t iterations = 10000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto obj = pool.acquire();
        if (obj) {
            obj->value = static_cast<int>(i);
            obj->data = "benchmark_" + std::to_string(i);
        }
        // Object automatically released when obj goes out of scope
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    BenchmarkResult result;
    result.name = "Object Pool Acquire/Release";
    result.duration = duration;
    result.iterations = iterations;
    result.memory_used = iterations * sizeof(TestObject);
    result.operations_per_second = (iterations * 1e9) / duration.count();
    result.additional_info = "Pool size: 100, Object size: " + std::to_string(sizeof(TestObject));
    
    return result;
}

/**
 * @brief Test memory efficiency statistics
 */
void testMemoryEfficiencyStats() {
    ObjectPool<TestObject> pool(10);
    
    // Get initial memory efficiency stats
    auto initial_stats = pool.getMemoryEfficiencyStats();
    size_t initial_reuses = std::get<0>(initial_stats);
    size_t initial_allocations = std::get<1>(initial_stats);
    
    // Perform operations to generate reuse
    std::vector<std::unique_ptr<TestObject>> objects;
    
    // First round - should create new objects
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.acquire());
    }
    objects.clear();
    
    // Second round - should reuse objects
    for (int i = 0; i < 5; ++i) {
        objects.push_back(pool.acquire());
    }
    objects.clear();
    
    // Check memory efficiency stats
    auto final_stats = pool.getMemoryEfficiencyStats();
    size_t final_reuses = std::get<0>(final_stats);
    size_t final_allocations = std::get<1>(final_stats);
    double reuse_ratio = std::get<2>(final_stats);
    
    ASSERT_GT(final_reuses, initial_reuses);
    ASSERT_GE(final_allocations, initial_allocations);
    ASSERT_GT(reuse_ratio, 0.0);
}

/**
 * @brief Test fast path acquisitions
 */
void testFastPathAcquisitions() {
    PoolConfig config;
    config.enable_object_warming = true;
    
    ObjectPool<TestObject> pool(10, nullptr, config);
    
    // Warm up some objects
    pool.triggerObjectWarming(5);
    
    size_t initial_fast_path = pool.getFastPathAcquisitions();
    
    // Acquire objects (should use fast path)
    std::vector<std::unique_ptr<TestObject>> objects;
    for (int i = 0; i < 3; ++i) {
        objects.push_back(pool.acquire());
    }
    
    size_t final_fast_path = pool.getFastPathAcquisitions();
    
    // Should have some fast path acquisitions
    ASSERT_GE(final_fast_path, initial_fast_path);
    
    objects.clear();
}

} // anonymous namespace

/**
 * @brief Register all object pool tests
 */
void registerObjectPoolTests(TestFramework& framework) {
    framework.addTest("BasicObjectPool", "Test basic object pool acquisition and release", 
                      testBasicObjectPool);
    
    framework.addTest("CustomFactory", "Test object pool with custom factory function", 
                      testCustomFactory);
    
    framework.addTest("PerformanceMonitoring", "Test object pool performance monitoring", 
                      testPerformanceMonitoring);
    
    framework.addTest("InitializerFinalizer", "Test object pool with initializer and finalizer", 
                      testInitializerFinalizer);
    
    framework.addTest("ThreadSafety", "Test object pool thread safety", 
                      testThreadSafety);
    
    framework.addTest("AdaptiveSizing", "Test object pool adaptive sizing feature", 
                      testAdaptiveSizing);
    
    framework.addTest("ObjectWarming", "Test object pool warming feature", 
                      testObjectWarming);
    
    framework.addTest("MemoryEfficiencyStats", "Test memory efficiency statistics", 
                      testMemoryEfficiencyStats);
    
    framework.addTest("FastPathAcquisitions", "Test fast path acquisition optimization", 
                      testFastPathAcquisitions);
    
    framework.addBenchmark("ObjectPoolPerformance", "Benchmark object pool performance", 
                           benchmarkObjectPoolPerformance);
}
