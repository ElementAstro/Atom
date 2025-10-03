/*
 * iteration_optimization_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Iteration and SIMD Optimization Example
Demonstrates cache-friendly iteration, SIMD processing, batch operations,
and performance optimization techniques for component systems.

**************************************************/

// Define feature flags before including headers
#ifndef ENABLE_FASTHASH
#define ENABLE_FASTHASH 0
#endif
#ifndef ENABLE_EVENT_SYSTEM
#define ENABLE_EVENT_SYSTEM 0
#endif
#ifndef ENABLE_HOT_RELOAD
#define ENABLE_HOT_RELOAD 0
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/iteration.hpp"
#include "atom/components/core/registry.hpp"

// Note: Registry and Component are in the global namespace, not atom::components
// ComponentIterator is not available in the current implementation

/**
 * @brief Transform component with SIMD-friendly data layout
 */
class TransformComponent : public Component {
public:
    struct alignas(16) Transform {
        float position[3] = {0.0f, 0.0f, 0.0f};
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        float scale[3] = {1.0f, 1.0f, 1.0f};
        float padding = 0.0f;  // SIMD alignment
    };

    explicit TransformComponent(const std::string& name) : Component(name) {
        // Initialize with random values
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-10.0f, 10.0f);

        for (int i = 0; i < 3; ++i) {
            transform_.position[i] = dis(gen);
            transform_.rotation[i] =
                dis(gen) * 0.1f;  // Smaller rotation values
        }

        addVariable<float>("pos_x", transform_.position[0]);
        addVariable<float>("pos_y", transform_.position[1]);
        addVariable<float>("pos_z", transform_.position[2]);

        def("getPosition", [this]() -> std::vector<float> {
            return {transform_.position[0], transform_.position[1],
                    transform_.position[2]};
        });

        def("setPosition", [this](float x, float y, float z) {
            transform_.position[0] = x;
            transform_.position[1] = y;
            transform_.position[2] = z;
            setValue("pos_x", x);
            setValue("pos_y", y);
            setValue("pos_z", z);
        });

        def("update", [this]() {
            // Simple physics update
            for (int i = 0; i < 3; ++i) {
                transform_.position[i] +=
                    transform_.rotation[i] * 0.016f;  // 60 FPS
            }
            setValue("pos_x", transform_.position[0]);
            setValue("pos_y", transform_.position[1]);
            setValue("pos_z", transform_.position[2]);
        });
    }

    Transform& getTransform() { return transform_; }
    const Transform& getTransform() const { return transform_; }

    // Batch processing interface
    void batchUpdate() { [[maybe_unused]] auto result = runCommand("update", {}); }

    float* getUpdateData() { return transform_.position; }
    size_t getUpdateDataSize() const { return 3; }

private:
    alignas(16) Transform transform_;
};

/**
 * @brief Physics component with batch processing capabilities
 */
class PhysicsComponent : public Component {
public:
    struct alignas(16) PhysicsData {
        float velocity[3] = {0.0f, 0.0f, 0.0f};
        float acceleration[3] = {0.0f, 0.0f, 0.0f};
        float mass = 1.0f;
        float damping = 0.99f;
        float padding[2] = {0.0f, 0.0f};  // Cache line alignment
    };

    explicit PhysicsComponent(const std::string& name) : Component(name) {
        // Initialize with random physics values
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> vel_dis(-5.0f, 5.0f);
        std::uniform_real_distribution<float> mass_dis(0.5f, 2.0f);

        for (int i = 0; i < 3; ++i) {
            physics_.velocity[i] = vel_dis(gen);
        }
        physics_.mass = mass_dis(gen);

        addVariable<float>("mass", physics_.mass);
        addVariable<float>("damping", physics_.damping);

        def("applyForce", [this](float fx, float fy, float fz) {
            physics_.acceleration[0] += fx / physics_.mass;
            physics_.acceleration[1] += fy / physics_.mass;
            physics_.acceleration[2] += fz / physics_.mass;
        });

        def("getVelocity", [this]() -> std::vector<float> {
            return {physics_.velocity[0], physics_.velocity[1],
                    physics_.velocity[2]};
        });

        def("updatePhysics", [this]() {
            // Update velocity with acceleration
            for (int i = 0; i < 3; ++i) {
                physics_.velocity[i] += physics_.acceleration[i] * 0.016f;
                physics_.velocity[i] *= physics_.damping;
                physics_.acceleration[i] = 0.0f;  // Reset acceleration
            }
        });
    }

    PhysicsData& getPhysicsData() { return physics_; }
    const PhysicsData& getPhysicsData() const { return physics_; }

    // Batch processing interface
    void batchUpdate() { [[maybe_unused]] auto result = runCommand("updatePhysics", {}); }

    float* getUpdateData() { return physics_.velocity; }
    size_t getUpdateDataSize() const { return 3; }

private:
    alignas(16) PhysicsData physics_;
};

void demonstrateBasicIteration() {
    std::cout << "\n=== Basic Component Iteration Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n1. Creating components for iteration..." << std::endl;

    const int NUM_COMPONENTS = 1000;
    std::vector<std::shared_ptr<TransformComponent>> transforms;
    std::vector<std::shared_ptr<PhysicsComponent>> physics;

    // Create transform components
    for (int i = 0; i < NUM_COMPONENTS; ++i) {
        auto transform = registry.createComponent<TransformComponent>(
            "Transform_" + std::to_string(i));
        transforms.push_back(transform);
    }

    // Create physics components
    for (int i = 0; i < NUM_COMPONENTS; ++i) {
        auto physicsComp = registry.createComponent<PhysicsComponent>(
            "Physics_" + std::to_string(i));
        physics.push_back(physicsComp);
    }

    std::cout << "Created " << transforms.size() << " transform components"
              << std::endl;
    std::cout << "Created " << physics.size() << " physics components"
              << std::endl;

    std::cout << "\n2. Basic iteration performance test..." << std::endl;

    const int ITERATIONS = 100;

    // Test basic iteration
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        // Update all transform components
        for (auto& transform : transforms) {
            [[maybe_unused]] auto result = transform->runCommand("update", {});
        }

        // Update all physics components
        for (auto& physicsComp : physics) {
            [[maybe_unused]] auto result = physicsComp->runCommand("updatePhysics", {});
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Basic iteration (" << ITERATIONS
              << " iterations): " << duration.count() << " μs" << std::endl;
    std::cout << "Average per iteration: " << (duration.count() / ITERATIONS)
              << " μs" << std::endl;
    std::cout << "Components per second: "
              << (NUM_COMPONENTS * 2 * ITERATIONS * 1000000LL /
                  duration.count())
              << std::endl;
}

void demonstrateBatchProcessing() {
    std::cout << "\n=== Batch Processing Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n3. Batch processing performance test..." << std::endl;

    // Get existing components
    std::vector<std::shared_ptr<TransformComponent>> transforms;
    std::vector<std::shared_ptr<PhysicsComponent>> physics;

    auto componentNames = registry.getAllComponentNames();
    for (const auto& name : componentNames) {
        if (name.find("Transform_") == 0) {
            auto comp = std::dynamic_pointer_cast<TransformComponent>(
                registry.getComponent(name));
            if (comp)
                transforms.push_back(comp);
        } else if (name.find("Physics_") == 0) {
            auto comp = std::dynamic_pointer_cast<PhysicsComponent>(
                registry.getComponent(name));
            if (comp)
                physics.push_back(comp);
        }
    }

    const int ITERATIONS = 100;

    // Test batch processing
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        // Batch update transforms
        for (auto& transform : transforms) {
            transform->batchUpdate();
        }

        // Batch update physics
        for (auto& physicsComp : physics) {
            physicsComp->batchUpdate();
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Batch processing (" << ITERATIONS
              << " iterations): " << duration.count() << " μs" << std::endl;
    std::cout << "Average per iteration: " << (duration.count() / ITERATIONS)
              << " μs" << std::endl;
    std::cout << "Components per second: "
              << ((transforms.size() + physics.size()) * ITERATIONS *
                  1000000LL / duration.count())
              << std::endl;
}

void demonstrateCacheFriendlyIteration() {
    std::cout << "\n=== Cache-Friendly Iteration Demo ===" << std::endl;

    auto& registry = Registry::instance();

    std::cout << "\n4. Cache-friendly data access patterns..." << std::endl;

    // Get existing components
    std::vector<std::shared_ptr<TransformComponent>> transforms;
    auto componentNames = registry.getAllComponentNames();
    for (const auto& name : componentNames) {
        if (name.find("Transform_") == 0) {
            auto comp = std::dynamic_pointer_cast<TransformComponent>(
                registry.getComponent(name));
            if (comp)
                transforms.push_back(comp);
        }
    }

    if (transforms.empty()) {
        std::cout << "No transform components found for cache test"
                  << std::endl;
        return;
    }

    const int ITERATIONS = 1000;

    // Test Structure of Arrays (SoA) pattern
    std::cout << "\n   Testing SoA pattern..." << std::endl;

    // Extract position data into contiguous arrays
    std::vector<float> positions_x, positions_y, positions_z;
    positions_x.reserve(transforms.size());
    positions_y.reserve(transforms.size());
    positions_z.reserve(transforms.size());

    for (const auto& transform : transforms) {
        const auto& t = transform->getTransform();
        positions_x.push_back(t.position[0]);
        positions_y.push_back(t.position[1]);
        positions_z.push_back(t.position[2]);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        // Process X coordinates
        for (size_t i = 0; i < positions_x.size(); ++i) {
            positions_x[i] += 0.01f;
        }

        // Process Y coordinates
        for (size_t i = 0; i < positions_y.size(); ++i) {
            positions_y[i] += 0.01f;
        }

        // Process Z coordinates
        for (size_t i = 0; i < positions_z.size(); ++i) {
            positions_z[i] += 0.01f;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto soa_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "   SoA pattern (" << ITERATIONS
              << " iterations): " << soa_duration.count() << " μs" << std::endl;

    // Test Array of Structures (AoS) pattern
    std::cout << "\n   Testing AoS pattern..." << std::endl;

    start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        for (auto& transform : transforms) {
            auto& t = transform->getTransform();
            t.position[0] += 0.01f;
            t.position[1] += 0.01f;
            t.position[2] += 0.01f;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto aos_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "   AoS pattern (" << ITERATIONS
              << " iterations): " << aos_duration.count() << " μs" << std::endl;

    // Compare performance
    std::cout << "\n   Performance comparison:" << std::endl;
    std::cout << "   SoA: " << soa_duration.count() << " μs" << std::endl;
    std::cout << "   AoS: " << aos_duration.count() << " μs" << std::endl;

    if (soa_duration < aos_duration) {
        double improvement =
            (double)(aos_duration.count() - soa_duration.count()) /
            aos_duration.count() * 100.0;
        std::cout << "   SoA is " << improvement << "% faster" << std::endl;
    } else {
        double overhead =
            (double)(soa_duration.count() - aos_duration.count()) /
            aos_duration.count() * 100.0;
        std::cout << "   AoS is " << overhead << "% faster" << std::endl;
    }
}

void demonstrateSIMDOptimization() {
    std::cout << "\n=== SIMD Optimization Demo ===" << std::endl;

    std::cout << "\n5. SIMD-friendly operations..." << std::endl;

    const size_t ARRAY_SIZE = 10000;
    const int ITERATIONS = 1000;

    // Create aligned arrays for SIMD operations
    std::vector<float> input1(ARRAY_SIZE);
    std::vector<float> input2(ARRAY_SIZE);
    std::vector<float> output(ARRAY_SIZE);

    // Initialize with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        input1[i] = dis(gen);
        input2[i] = dis(gen);
    }

    std::cout << "   Testing scalar operations..." << std::endl;

    // Scalar version
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            output[i] = input1[i] * input2[i] + 0.5f;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto scalar_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "   Scalar operations (" << ITERATIONS
              << " iterations): " << scalar_duration.count() << " μs"
              << std::endl;

    // Note: Actual SIMD implementation would require compiler intrinsics
    // This is a demonstration of the concept
    std::cout
        << "   Note: Actual SIMD implementation requires compiler intrinsics"
        << std::endl;
    std::cout
        << "   Expected SIMD improvement: 2-4x faster for float operations"
        << std::endl;

    // Demonstrate vectorized operations using standard algorithms
    std::cout << "\n   Testing vectorized operations..." << std::endl;

    start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        std::transform(input1.begin(), input1.end(), input2.begin(),
                       output.begin(),
                       [](float a, float b) { return a * b + 0.5f; });
    }

    end = std::chrono::high_resolution_clock::now();
    auto vectorized_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "   Vectorized operations (" << ITERATIONS
              << " iterations): " << vectorized_duration.count() << " μs"
              << std::endl;

    // Compare performance
    std::cout << "\n   Performance comparison:" << std::endl;
    std::cout << "   Scalar: " << scalar_duration.count() << " μs" << std::endl;
    std::cout << "   Vectorized: " << vectorized_duration.count() << " μs"
              << std::endl;

    if (vectorized_duration < scalar_duration) {
        double improvement =
            (double)(scalar_duration.count() - vectorized_duration.count()) /
            scalar_duration.count() * 100.0;
        std::cout << "   Vectorized is " << improvement << "% faster"
                  << std::endl;
    }
}

void demonstrateMemoryPrefetching() {
    std::cout << "\n=== Memory Prefetching Demo ===" << std::endl;

    std::cout << "\n6. Memory access patterns and prefetching..." << std::endl;

    const size_t ARRAY_SIZE = 100000;
    std::vector<int> data(ARRAY_SIZE);
    std::iota(data.begin(), data.end(), 0);

    // Random access pattern (cache-unfriendly)
    std::vector<size_t> random_indices(ARRAY_SIZE);
    std::iota(random_indices.begin(), random_indices.end(), 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(random_indices.begin(), random_indices.end(), gen);

    std::cout << "   Testing sequential access..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    long long sum = 0;
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        sum += data[i];
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto sequential_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "   Sequential access: " << sequential_duration.count()
              << " μs (sum: " << sum << ")" << std::endl;

    std::cout << "   Testing random access..." << std::endl;

    start = std::chrono::high_resolution_clock::now();
    sum = 0;
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        sum += data[random_indices[i]];
    }
    end = std::chrono::high_resolution_clock::now();
    auto random_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "   Random access: " << random_duration.count()
              << " μs (sum: " << sum << ")" << std::endl;

    // Performance comparison
    std::cout << "\n   Memory access performance:" << std::endl;
    std::cout << "   Sequential: " << sequential_duration.count() << " μs"
              << std::endl;
    std::cout << "   Random: " << random_duration.count() << " μs" << std::endl;

    if (random_duration > sequential_duration) {
        double slowdown =
            (double)random_duration.count() / sequential_duration.count();
        std::cout << "   Random access is " << slowdown
                  << "x slower than sequential" << std::endl;
    }

    std::cout << "   Recommendation: Use cache-friendly data layouts and "
                 "access patterns"
              << std::endl;
}

int main() {
    std::cout
        << "=== Atom Component Iteration and SIMD Optimization Examples ==="
        << std::endl;

    try {
        demonstrateBasicIteration();
        demonstrateBatchProcessing();
        demonstrateCacheFriendlyIteration();
        demonstrateSIMDOptimization();
        demonstrateMemoryPrefetching();

        std::cout << "\n=== All Iteration Optimization Examples Completed "
                     "Successfully! ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in iteration optimization examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
