/*
 * optimized_usage_examples.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Examples for Optimized Component System Usage
Demonstrates memory pool integration, SIMD optimizations, and performance
best practices with real-world scenarios.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "../component.hpp"
#include "../component_pool.hpp"
#include "../iteration.hpp"
#include "../registry.hpp"

using namespace atom::components;

//==============================================================================
// Example 1: High-Performance Game Entity System
//==============================================================================

/**
 * @brief Transform component for position, rotation, scale
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
        // Register optimized update functions
        def("setPosition", [this](float x, float y, float z) {
            transform_.position[0] = x;
            transform_.position[1] = y;
            transform_.position[2] = z;
        });

        def("getPosition", [this]() -> std::vector<float> {
            return {transform_.position[0], transform_.position[1],
                    transform_.position[2]};
        });

        // Add variables for external access
        addVariable<float>("x", transform_.position[0]);
        addVariable<float>("y", transform_.position[1]);
        addVariable<float>("z", transform_.position[2]);
    }

    Transform& getTransform() { return transform_; }
    const Transform& getTransform() const { return transform_; }

    // Batch processing interface
    void batchUpdate() {
        // Apply physics or animation updates
        transform_.position[1] += velocity_;
    }

    float* getUpdateData() { return transform_.position; }
    size_t getUpdateDataSize() const { return 3; }

private:
    alignas(16) Transform transform_;
    float velocity_ = 0.0f;
};

/**
 * @brief Render component with optimized data layout
 */
class RenderComponent : public Component {
public:
    struct alignas(64) RenderData {
        uint32_t meshId = 0;
        uint32_t materialId = 0;
        uint32_t textureId = 0;
        uint32_t shaderProgram = 0;
        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        bool visible = true;
        bool castShadows = true;
        bool receiveShadows = true;
        uint8_t padding[45] = {0};  // Cache line padding
    };

    explicit RenderComponent(const std::string& name) : Component(name) {
        def("setVisible",
            [this](bool visible) { renderData_.visible = visible; });
        def("setColor", [this](float r, float g, float b, float a) {
            renderData_.color[0] = r;
            renderData_.color[1] = g;
            renderData_.color[2] = b;
            renderData_.color[3] = a;
        });

        addVariable<bool>("visible", renderData_.visible);
        addVariable<uint32_t>("meshId", renderData_.meshId);
    }

    RenderData& getRenderData() { return renderData_; }
    const RenderData& getRenderData() const { return renderData_; }

private:
    alignas(64) RenderData renderData_;
};

/**
 * @brief Game entity manager with optimized component handling
 */
class GameEntityManager {
public:
    GameEntityManager() {
        // Configure memory pools for optimal performance
        PoolConfig transformPoolConfig;
        transformPoolConfig.initialPoolSize = 1000;
        transformPoolConfig.maxPoolSize = 10000;
        transformPoolConfig.chunkSize = 64;  // Cache-friendly chunks
        transformPoolConfig.enableStatistics = true;

        PoolConfig renderPoolConfig;
        renderPoolConfig.initialPoolSize = 1000;
        renderPoolConfig.maxPoolSize = 10000;
        renderPoolConfig.chunkSize = 32;
        renderPoolConfig.enableStatistics = true;

        // Create specialized pools
        transformPool_ = std::make_unique<ComponentPool<TransformComponent>>(
            transformPoolConfig);
        renderPool_ =
            std::make_unique<ComponentPool<RenderComponent>>(renderPoolConfig);

        // Configure SIMD container
        SIMDComponentContainer<TransformComponent>::SIMDConfig simdConfig;
        simdConfig.batchSize = 64;
        simdConfig.enableSIMD = true;
        simdConfig.enablePrefetch = true;
        transformContainer_ =
            std::make_unique<SIMDComponentContainer<TransformComponent>>(
                simdConfig);
    }

    /**
     * @brief Creates a game entity with transform and render components
     */
    size_t createEntity(const std::string& name) {
        size_t entityId = nextEntityId_++;

        // Use memory pools for allocation
        auto transform =
            transformPool_->allocate("transform_" + std::to_string(entityId));
        auto render =
            renderPool_->allocate("render_" + std::to_string(entityId));

        // Store components
        entities_[entityId] = {transform, render};

        // Add to SIMD container for batch processing
        transformContainer_->add(transform);

        return entityId;
    }

    /**
     * @brief Updates all entities using SIMD optimization
     */
    void updateEntities() {
        // Batch update transforms using SIMD
        transformContainer_->forEachBatch([](auto& batch) {
            // SIMD-optimized batch processing
            for (auto& transform : batch) {
                transform->batchUpdate();
            }
        });
    }

    /**
     * @brief Gets performance statistics
     */
    void printStatistics() const {
        const auto& transformStats = transformPool_->getStatistics();
        const auto& renderStats = renderPool_->getStatistics();

        std::cout << "Transform Pool Statistics:\n";
        std::cout << "  Hit Ratio: " << transformStats.getHitRatio() * 100
                  << "%\n";
        std::cout << "  Memory Usage: "
                  << transformPool_->getMemoryUsage() / 1024 << " KB\n";

        std::cout << "Render Pool Statistics:\n";
        std::cout << "  Hit Ratio: " << renderStats.getHitRatio() * 100
                  << "%\n";
        std::cout << "  Memory Usage: " << renderPool_->getMemoryUsage() / 1024
                  << " KB\n";
    }

private:
    struct Entity {
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<RenderComponent> render;
    };

    std::unordered_map<size_t, Entity> entities_;
    size_t nextEntityId_ = 1;

    std::unique_ptr<ComponentPool<TransformComponent>> transformPool_;
    std::unique_ptr<ComponentPool<RenderComponent>> renderPool_;
    std::unique_ptr<SIMDComponentContainer<TransformComponent>>
        transformContainer_;
};

//==============================================================================
// Example 2: Scientific Computing with Component Archetype System
//==============================================================================

/**
 * @brief Particle component for physics simulation
 */
class ParticleComponent : public Component {
public:
    struct alignas(32) ParticleData {
        float position[3];
        float velocity[3];
        float mass;
        float charge;
    };

    explicit ParticleComponent(const std::string& name) : Component(name) {
        def("setMass", [this](float mass) { data_.mass = mass; });
        def("setCharge", [this](float charge) { data_.charge = charge; });
        def("getMass", [this]() { return data_.mass; });
        def("getCharge", [this]() { return data_.charge; });
    }

    ParticleData& getData() { return data_; }
    const ParticleData& getData() const { return data_; }

private:
    alignas(32) ParticleData data_{};
};

/**
 * @brief Force component for physics calculations
 */
class ForceComponent : public Component {
public:
    explicit ForceComponent(const std::string& name) : Component(name) {
        def("addForce", [this](float fx, float fy, float fz) {
            force_[0] += fx;
            force_[1] += fy;
            force_[2] += fz;
        });

        def("clearForce",
            [this]() { force_[0] = force_[1] = force_[2] = 0.0f; });
    }

    float* getForce() { return force_; }
    const float* getForce() const { return force_; }

private:
    alignas(16) float force_[3] = {0.0f, 0.0f, 0.0f};
};

/**
 * @brief Physics simulation system using component archetypes
 */
class PhysicsSimulation {
public:
    PhysicsSimulation() {
        // Create archetype for particles with physics properties
        particleArchetype_ = std::make_shared<
            ComponentArchetype<ParticleComponent, ForceComponent>>();

        // Configure query engine
        queryEngine_.registerArchetype(particleArchetype_);
    }

    /**
     * @brief Adds a particle to the simulation
     */
    size_t addParticle(float mass, float charge, float x, float y, float z) {
        auto particle = std::make_shared<ParticleComponent>(
            "particle_" + std::to_string(nextParticleId_));
        auto force = std::make_shared<ForceComponent>(
            "force_" + std::to_string(nextParticleId_));

        // Set initial properties
        auto& data = particle->getData();
        data.mass = mass;
        data.charge = charge;
        data.position[0] = x;
        data.position[1] = y;
        data.position[2] = z;

        // Add to archetype
        size_t entityId = particleArchetype_->addEntity(*particle, *force);

        return nextParticleId_++;
    }

    /**
     * @brief Performs physics simulation step
     */
    void simulateStep(float deltaTime) {
        // Query all particles and apply forces
        queryEngine_.query()
            .with<ParticleComponent>()
            .with<ForceComponent>()
            .execute([deltaTime](auto& entities) {
                // SIMD-optimized physics calculations
                for (auto& entity : entities) {
                    auto& particle = std::get<ParticleComponent>(entity);
                    auto& force = std::get<ForceComponent>(entity);

                    auto& data = particle.getData();
                    const float* f = force.getForce();

                    // Apply forces (F = ma)
                    float acceleration[3] = {f[0] / data.mass, f[1] / data.mass,
                                             f[2] / data.mass};

                    // Update velocity and position
                    for (int i = 0; i < 3; ++i) {
                        data.velocity[i] += acceleration[i] * deltaTime;
                        data.position[i] += data.velocity[i] * deltaTime;
                    }
                }
            });
    }

    /**
     * @brief Gets simulation statistics
     */
    void printStatistics() const {
        std::cout << "Physics Simulation Statistics:\n";
        std::cout << "  Particles: " << particleArchetype_->size() << "\n";
        std::cout << "  Memory Layout: Structure of Arrays (SoA)\n";
        std::cout << "  SIMD Optimization: Enabled\n";
    }

private:
    std::shared_ptr<ComponentArchetype<ParticleComponent, ForceComponent>>
        particleArchetype_;
    ComponentQueryEngine queryEngine_;
    size_t nextParticleId_ = 1;
};

//==============================================================================
// Example Usage and Demonstration
//==============================================================================

void demonstrateGameEntitySystem() {
    std::cout << "=== Game Entity System Example ===\n";

    GameEntityManager entityManager;

    // Create entities
    std::vector<size_t> entities;
    for (int i = 0; i < 1000; ++i) {
        entities.push_back(
            entityManager.createEntity("entity_" + std::to_string(i)));
    }

    // Benchmark update performance
    auto start = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < 100; ++frame) {
        entityManager.updateEntities();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Updated 1000 entities for 100 frames in " << duration.count()
              << " microseconds\n";
    std::cout << "Average: " << duration.count() / 100.0
              << " microseconds per frame\n";

    entityManager.printStatistics();
    std::cout << "\n";
}

void demonstratePhysicsSimulation() {
    std::cout << "=== Physics Simulation Example ===\n";

    PhysicsSimulation simulation;

    // Add particles
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> massDist(0.1f, 2.0f);
    std::uniform_real_distribution<float> chargeDist(-1.0f, 1.0f);

    for (int i = 0; i < 500; ++i) {
        simulation.addParticle(massDist(gen), chargeDist(gen), posDist(gen),
                               posDist(gen), posDist(gen));
    }

    // Run simulation
    auto start = std::chrono::high_resolution_clock::now();

    for (int step = 0; step < 1000; ++step) {
        simulation.simulateStep(0.016f);  // 60 FPS
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Simulated 500 particles for 1000 steps in "
              << duration.count() << " microseconds\n";
    std::cout << "Average: " << duration.count() / 1000.0
              << " microseconds per step\n";

    simulation.printStatistics();
    std::cout << "\n";
}

//==============================================================================
// Example 3: Memory Pool Integration Best Practices
//==============================================================================

void demonstrateMemoryPoolIntegration() {
    std::cout << "=== Memory Pool Integration Example ===\n";

    // Configure different pool strategies for different component types
    PoolConfig frequentAllocConfig;
    frequentAllocConfig.initialPoolSize = 512;
    frequentAllocConfig.maxPoolSize = 2048;
    frequentAllocConfig.chunkSize = 64;
    frequentAllocConfig.enableCacheOptimization = true;

    PoolConfig infrequentAllocConfig;
    infrequentAllocConfig.initialPoolSize = 32;
    infrequentAllocConfig.maxPoolSize = 128;
    frequentAllocConfig.chunkSize = 8;

    // Create pools
    ComponentPool<TransformComponent> transformPool(frequentAllocConfig);
    ComponentPool<RenderComponent> renderPool(infrequentAllocConfig);

    // Demonstrate allocation patterns
    std::vector<std::shared_ptr<TransformComponent>> transforms;

    auto start = std::chrono::high_resolution_clock::now();

    // Allocate many components rapidly
    for (int i = 0; i < 1000; ++i) {
        transforms.push_back(
            transformPool.allocate("transform_" + std::to_string(i)));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Allocated 1000 components in " << duration.count()
              << " microseconds\n";

    // Show pool statistics
    const auto& stats = transformPool.getStatistics();
    std::cout << "Pool hit ratio: " << stats.getHitRatio() * 100 << "%\n";
    std::cout << "Memory usage: " << transformPool.getMemoryUsage() / 1024
              << " KB\n";
    std::cout << "Fragmentation: "
              << transformPool.getFragmentationRatio() * 100 << "%\n\n";
}

//==============================================================================
// Example 4: SIMD Optimization Patterns
//==============================================================================

void demonstrateSIMDOptimizations() {
    std::cout << "=== SIMD Optimization Example ===\n";

    // Create SIMD-friendly component container
    SIMDComponentContainer<TransformComponent>::SIMDConfig config;
    config.batchSize = 64;  // Process 64 components at once
    config.enableSIMD = true;
    config.enablePrefetch = true;

    SIMDComponentContainer<TransformComponent> container(config);

    // Add components
    for (int i = 0; i < 1000; ++i) {
        auto transform = std::make_shared<TransformComponent>(
            "simd_transform_" + std::to_string(i));
        container.add(transform);
    }

    // Benchmark SIMD vs standard processing
    auto start = std::chrono::high_resolution_clock::now();

    container.forEachBatch([](auto& batch) {
        // SIMD-optimized batch processing
        for (auto& transform : batch) {
            transform->batchUpdate();
        }
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto simdDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "SIMD batch processing: " << simdDuration.count()
              << " microseconds\n";
    std::cout << "Components processed: " << container.size() << "\n";
    std::cout << "Throughput: "
              << (container.size() * 1e6) / simdDuration.count()
              << " components/second\n\n";
}

int main() {
    std::cout << "Optimized Component System Usage Examples\n";
    std::cout << "==========================================\n\n";

    demonstrateGameEntitySystem();
    demonstratePhysicsSimulation();
    demonstrateMemoryPoolIntegration();
    demonstrateSIMDOptimizations();

    std::cout << "All examples completed successfully!\n";
    std::cout << "Key optimizations demonstrated:\n";
    std::cout << "  - Cache-aligned component layouts\n";
    std::cout << "  - Memory pool integration\n";
    std::cout << "  - SIMD-friendly data structures\n";
    std::cout << "  - Batch processing patterns\n";
    std::cout << "  - Component archetype systems\n";

    return 0;
}
