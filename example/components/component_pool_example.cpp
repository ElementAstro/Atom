/*
 * component_pool_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Component Pool and Memory Management ExampleDemonstrates memory
pools, cache optimization, performance monitoring, and efficient component
allocation/deallocation patterns.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/component_pool.hpp"
#include "atom/components/core/registry.hpp"

// Note: Registry and Component are in the global namespace, not
// atom::componentsusing atom::components::ComponentPool;

/**
 * @brief Lightweight component for pool testing
 */
class PoolTestComponent : public Component {
public:
    explicit PoolTestComponent(const std::string& name) : Component(name) {
        // Add some basic variables
        addVariable<int>("id", generateId());
        addVariable<double>("value", generateRandomValue());
        addVariable<std::string>("status", "active");

        // Add simple commands
        def("getId", [this]() -> int {
            auto id = getVariable<int>("id");
            return id ? id->get() : -1;
        });

        def("getValue", [this]() -> double {
            auto value = getVariable<double>("value");
            return value ? value->get() : 0.0;
        });

        def("updateValue",
            [this]() { setValue("value", generateRandomValue()); });

        def("process", [this]() {
            // Simulate some processing work
            auto value = getVariable<double>("value");
            if (value) {
                setValue("value", value->get() * 1.1);
            }
        });
    }

private:
    static int generateId() {
        static int counter = 0;
        return ++counter;
    }

    static double generateRandomValue() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 100.0);
        return dis(gen);
    }
};

/**
 * @brief Heavy component for performance testing
 */
class HeavyComponent : public Component {
public:
    explicit HeavyComponent(const std::string& name) : Component(name) {
        // Allocate some memory to simulate heavy components
        data_.resize(1000);
        std::iota(data_.begin(), data_.end(), 0);

        addVariable<size_t>("data_size", data_.size());
        addVariable<int>("computation_count", 0);

        def("heavyComputation", [this]() -> double {
            // Simulate heavy computation
            double result = 0.0;
            for (size_t i = 0; i < data_.size(); ++i) {
                result += std::sin(data_[i]) * std::cos(data_[i]);
            }

            auto count = getVariable<int>("computation_count");
            if (count) {
                setValue("computation_count", count->get() + 1);
            }

            return result;
        });

        def("getComputationCount", [this]() -> int {
            auto count = getVariable<int>("computation_count");
            return count ? count->get() : 0;
        });
    }

private:
    std::vector<double> data_;
};

void demonstrateBasicPoolOperations() {
    std::cout << "\n=== Basic Pool Operations Demo ===" << std::endl;

    // Create a component pool with explicit template parameter
    ComponentPool<PoolTestComponent> pool;

    std::cout << "\n1. Creating component pool..." << std::endl;
    std::cout << "   Pool created successfully" << std::endl;

    std::cout << "\n2. Allocating components from pool..." << std::endl;

    std::vector<std::shared_ptr<PoolTestComponent>> components;

    // Allocate multiple components
    for (int i = 0; i < 10; ++i) {
        auto component = pool.allocate("PoolComponent_" + std::to_string(i));
        if (component) {
            components.push_back(component);
            std::cout << "   Allocated component " << i << " with ID: "
                      << std::any_cast<int>(component->runCommand("getId", {}))
                      << std::endl;
        }
    }

    std::cout << "\n3. Pool statistics after allocation:" << std::endl;
    const auto& stats = pool.getStatistics();
    std::cout << "   Total allocations: " << stats.totalAllocations.load()
              << std::endl;
    std::cout << "   Current allocations: " << stats.currentAllocations.load()
              << std::endl;
    std::cout << "   Pool hits: " << stats.poolHits.load() << std::endl;
    std::cout << "   Pool misses: " << stats.poolMisses.load() << std::endl;
    std::cout << "   Hit ratio: " << stats.getHitRatio() << std::endl;

    std::cout << "\n4. Using allocated components..." << std::endl;
    for (auto& component : components) {
        [[maybe_unused]] auto result = component->runCommand("process", {});
        std::cout << "   Component "
                  << std::any_cast<int>(component->runCommand("getId", {}))
                  << " processed, new value: "
                  << std::any_cast<double>(
                         component->runCommand("getValue", {}))
                  << std::endl;
    }

    std::cout << "\n5. Deallocating components..." << std::endl;
    for (auto& component : components) {
        int id = std::any_cast<int>(component->runCommand("getId", {}));
        pool.deallocate(component);
        std::cout << "   Deallocated component " << id << std::endl;
    }
    components.clear();

    std::cout << "\n6. Pool statistics after deallocation:" << std::endl;
    std::cout << "   Total deallocations: " << stats.totalDeallocations.load()
              << std::endl;
    std::cout << "   Current allocations: " << stats.currentAllocations.load()
              << std::endl;
}

void demonstratePerformanceComparison() {
    std::cout << "\n=== Performance Comparison Demo ===" << std::endl;

    const int NUM_COMPONENTS = 1000;
    const int NUM_ITERATIONS = 5;

    std::cout << "\n7. Comparing pool vs direct allocation performance..."
              << std::endl;
    std::cout << "   Testing with " << NUM_COMPONENTS << " components, "
              << NUM_ITERATIONS << " iterations" << std::endl;

    // Test direct allocation
    std::cout << "\n   Direct allocation test:" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        std::vector<std::shared_ptr<PoolTestComponent>> directComponents;

        // Allocate
        for (int i = 0; i < NUM_COMPONENTS; ++i) {
            auto component = std::make_shared<PoolTestComponent>(
                "Direct_" + std::to_string(i));
            directComponents.push_back(component);
        }

        // Use components
        for (auto& component : directComponents) {
            [[maybe_unused]] auto result = component->runCommand("process", {});
        }

        // Deallocate (automatic with shared_ptr)
        directComponents.clear();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto directTime =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "     Direct allocation time: " << directTime.count()
              << " microseconds" << std::endl;

    // Test pool allocation
    std::cout << "\n   Pool allocation test:" << std::endl;
    ComponentPool<PoolTestComponent> pool;

    start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        std::vector<std::shared_ptr<PoolTestComponent>> poolComponents;

        // Allocate from pool
        for (int i = 0; i < NUM_COMPONENTS; ++i) {
            auto component = pool.allocate("Pool_" + std::to_string(i));
            poolComponents.push_back(component);
        }

        // Use components
        for (auto& component : poolComponents) {
            [[maybe_unused]] auto result = component->runCommand("process", {});
        }

        // Deallocate to pool
        for (auto& component : poolComponents) {
            pool.deallocate(component);
        }
        poolComponents.clear();
    }

    end = std::chrono::high_resolution_clock::now();
    auto poolTime =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "     Pool allocation time: " << poolTime.count()
              << " microseconds" << std::endl;

    // Performance comparison
    std::cout << "\n   Performance comparison:" << std::endl;
    std::cout << "     Direct allocation: " << directTime.count() << " μs"
              << std::endl;
    std::cout << "     Pool allocation: " << poolTime.count() << " μs"
              << std::endl;

    if (poolTime < directTime) {
        double improvement = (double)(directTime.count() - poolTime.count()) /
                             directTime.count() * 100.0;
        std::cout << "     Pool is " << improvement << "% faster" << std::endl;
    } else {
        double overhead = (double)(poolTime.count() - directTime.count()) /
                          directTime.count() * 100.0;
        std::cout << "     Pool has " << overhead << "% overhead" << std::endl;
    }

    // Final pool statistics
    std::cout << "\n   Final pool statistics:" << std::endl;
    const auto& stats = pool.getStatistics();
    std::cout << "     Total allocations: " << stats.totalAllocations.load()
              << std::endl;
    std::cout << "     Total deallocations: " << stats.totalDeallocations.load()
              << std::endl;
    std::cout << "     Pool hits: " << stats.poolHits.load() << std::endl;
    std::cout << "     Pool misses: " << stats.poolMisses.load() << std::endl;
    std::cout << "     Hit ratio: " << stats.getHitRatio() << std::endl;
}

void demonstrateMemoryUsage() {
    std::cout << "\n=== Memory Usage Demo ===" << std::endl;

    ComponentPool<HeavyComponent> pool;

    std::cout << "\n8. Testing memory usage with heavy components..."
              << std::endl;

    std::vector<std::shared_ptr<HeavyComponent>> heavyComponents;

    // Allocate heavy components
    std::cout << "   Allocating 50 heavy components..." << std::endl;
    for (int i = 0; i < 50; ++i) {
        auto component = pool.allocate("Heavy_" + std::to_string(i));
        if (component) {
            heavyComponents.push_back(component);
        }
    }

    std::cout << "   Heavy components allocated: " << heavyComponents.size()
              << std::endl;

    // Use heavy components
    std::cout << "   Performing heavy computations..." << std::endl;
    double totalResult = 0.0;
    for (auto& component : heavyComponents) {
        auto result = component->runCommand("heavyComputation", {});
        totalResult += std::any_cast<double>(result);
    }

    std::cout << "   Total computation result: " << totalResult << std::endl;

    // Check computation counts
    std::cout << "   Computation counts:" << std::endl;
    for (size_t i = 0; i < std::min(size_t(5), heavyComponents.size()); ++i) {
        auto count = heavyComponents[i]->runCommand("getComputationCount", {});
        std::cout << "     Component " << i << ": " << std::any_cast<int>(count)
                  << " computations" << std::endl;
    }

    // Deallocate heavy components
    std::cout << "   Deallocating heavy components..." << std::endl;
    for (auto& component : heavyComponents) {
        pool.deallocate(component);
    }
    heavyComponents.clear();

    std::cout << "   Heavy components deallocated" << std::endl;

    // Pool statistics
    const auto& stats = pool.getStatistics();
    std::cout << "   Memory pool statistics:" << std::endl;
    std::cout << "     Current allocations: " << stats.currentAllocations.load()
              << std::endl;
    std::cout << "     Peak allocations: " << stats.peakAllocations.load()
              << std::endl;
}

void demonstrateConcurrentAccess() {
    std::cout << "\n=== Concurrent Access Demo ===" << std::endl;

    ComponentPool<PoolTestComponent> pool;

    std::cout << "\n9. Testing concurrent pool access..." << std::endl;

    const int NUM_THREADS = 4;
    const int COMPONENTS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<int> totalAllocated{0};
    std::atomic<int> totalProcessed{0};

    auto workerFunction = [&](int threadId) {
        std::vector<std::shared_ptr<PoolTestComponent>> localComponents;

        // Allocate components
        for (int i = 0; i < COMPONENTS_PER_THREAD; ++i) {
            auto component = pool.allocate("Thread" + std::to_string(threadId) +
                                           "_" + std::to_string(i));
            if (component) {
                localComponents.push_back(component);
                totalAllocated.fetch_add(1);
            }
        }

        // Process components
        for (auto& component : localComponents) {
            [[maybe_unused]] auto result = component->runCommand("process", {});
            totalProcessed.fetch_add(1);
        }

        // Small delay to simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Deallocate components
        for (auto& component : localComponents) {
            pool.deallocate(component);
        }
        localComponents.clear();
    };

    // Start threads
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(workerFunction, i);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "   Concurrent test completed in: " << duration.count()
              << " ms" << std::endl;
    std::cout << "   Total components allocated: " << totalAllocated.load()
              << std::endl;
    std::cout << "   Total components processed: " << totalProcessed.load()
              << std::endl;

    // Final pool statistics
    const auto& stats = pool.getStatistics();
    std::cout << "   Final pool statistics:" << std::endl;
    std::cout << "     Total allocations: " << stats.totalAllocations.load()
              << std::endl;
    std::cout << "     Total deallocations: " << stats.totalDeallocations.load()
              << std::endl;
    std::cout << "     Current allocations: " << stats.currentAllocations.load()
              << std::endl;
    std::cout << "     Hit ratio: " << stats.getHitRatio() << std::endl;
}

int main() {
    std::cout << "=== Atom Component Pool and Memory Management Examples ==="
              << std::endl;

    try {
        demonstrateBasicPoolOperations();
        demonstratePerformanceComparison();
        demonstrateMemoryUsage();
        demonstrateConcurrentAccess();

        std::cout
            << "\n=== All Component Pool Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in component pool examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
