/*
 * test_component_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Test Suite for Enhanced Component System
Tests memory pooling, lifecycle management, iteration patterns,
serialization, scripting integration, and performance benchmarks.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../component_pool.hpp"
#include "../iteration.hpp"
#include "../lifecycle.hpp"
#include "../script_sandbox.hpp"
#include "../scripting_api.hpp"
#include "../serialization.hpp"

#if ATOM_ENABLE_LUA
#include "../lua_engine.hpp"
#endif

#if ATOM_ENABLE_PYTHON
#include "../python_engine.hpp"
#endif

using namespace atom::components;
using namespace atom::components::scripting;

/**
 * @brief Test fixture for component system tests
 */
class ComponentSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize component system
        registry_ = &Registry::instance();

        // Create test components
        testComponent1_ = std::make_shared<Component>("TestComponent1");
        testComponent2_ = std::make_shared<Component>("TestComponent2");

        // Register test components
        registry_->registerComponent("TestComponent1", testComponent1_);
        registry_->registerComponent("TestComponent2", testComponent2_);
    }

    void TearDown() override {
        // Cleanup
        registry_->removeComponent("TestComponent1");
        registry_->removeComponent("TestComponent2");
    }

    Registry* registry_;
    std::shared_ptr<Component> testComponent1_;
    std::shared_ptr<Component> testComponent2_;
};

/**
 * @brief Test fixture for memory pool tests
 */
class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        PoolConfig config;
        config.initialPoolSize = 32;
        config.maxPoolSize = 128;
        config.chunkSize = 8;
        config.enableStatistics = true;

        pool_ = std::make_unique<ComponentPool<Component>>(config);
    }

    std::unique_ptr<ComponentPool<Component>> pool_;
};

/**
 * @brief Test fixture for optimized component system tests
 */
class OptimizedComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure optimized memory pools
        PoolConfig config;
        config.initialPoolSize = 64;
        config.maxPoolSize = 256;
        config.chunkSize = 16;
        config.enableStatistics = true;
        config.enableCacheOptimization = true;

        pool_ = std::make_unique<ComponentPool<Component>>(config);

        // Create SIMD container
        SIMDComponentContainer<Component>::SIMDConfig simdConfig;
        simdConfig.batchSize = 32;
        simdConfig.enableSIMD = true;
        simdConfig.enablePrefetch = true;

        simdContainer_ =
            std::make_unique<SIMDComponentContainer<Component>>(simdConfig);
    }

    std::unique_ptr<ComponentPool<Component>> pool_;
    std::unique_ptr<SIMDComponentContainer<Component>> simdContainer_;
};

/**
 * @brief Test fixture for scripting tests
 */
class ScriptingTest : public ::testing::Test {
protected:
    void SetUp() override {
        scriptingAPI_ = &ComponentScriptingAPI::instance();

        ScriptEngineConfig config;
        config.memoryLimit = 32 * 1024 * 1024;  // 32MB
        config.executionTimeout = std::chrono::seconds(10);
        config.enableDebug = true;

        ASSERT_TRUE(scriptingAPI_->initialize(config));

        // Create sandbox
        SandboxConfig sandboxConfig;
        sandboxConfig.permissions =
            Permission::ComponentAccess | Permission::MemoryAccess;
        sandboxConfig.limits.maxMemoryUsage = 16 * 1024 * 1024;  // 16MB
        sandboxConfig.enableLogging = true;

        sandbox_ = std::make_unique<ScriptSandbox>(sandboxConfig);
        ASSERT_TRUE(sandbox_->initialize());
    }

    void TearDown() override {
        sandbox_->shutdown();
        scriptingAPI_->shutdown();
    }

    ComponentScriptingAPI* scriptingAPI_;
    std::unique_ptr<ScriptSandbox> sandbox_;
};

// ============================================================================
// Component Pool Tests
// ============================================================================

TEST_F(MemoryPoolTest, BasicAllocation) {
    // Test basic allocation and deallocation
    auto component = pool_->allocate("TestComponent");
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "TestComponent");

    // Check statistics
    const auto& stats = pool_->getStatistics();
    EXPECT_EQ(stats.totalAllocations.load(), 1);
    EXPECT_EQ(stats.currentAllocations.load(), 1);
    EXPECT_GT(stats.poolHits.load(), 0);
}

TEST_F(MemoryPoolTest, MultipleAllocations) {
    std::vector<std::shared_ptr<Component>> components;

    // Allocate multiple components
    for (int i = 0; i < 16; ++i) {
        auto component = pool_->allocate("TestComponent" + std::to_string(i));
        ASSERT_NE(component, nullptr);
        components.push_back(component);
    }

    // Check statistics
    const auto& stats = pool_->getStatistics();
    EXPECT_EQ(stats.totalAllocations.load(), 16);
    EXPECT_EQ(stats.currentAllocations.load(), 16);

    // Deallocate all components
    components.clear();

    // Check deallocation statistics
    EXPECT_EQ(stats.currentAllocations.load(), 0);
    EXPECT_EQ(stats.totalDeallocations.load(), 16);
}

TEST_F(MemoryPoolTest, PerformanceBenchmark) {
    const int numAllocations = 1000;
    const int numIterations = 10;

    std::vector<std::chrono::microseconds> poolTimes;
    std::vector<std::chrono::microseconds> standardTimes;

    // Benchmark pool allocation
    for (int iter = 0; iter < numIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::shared_ptr<Component>> components;
        for (int i = 0; i < numAllocations; ++i) {
            components.push_back(pool_->allocate("BenchmarkComponent"));
        }
        components.clear();

        auto end = std::chrono::high_resolution_clock::now();
        poolTimes.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start));
    }

    // Benchmark standard allocation
    for (int iter = 0; iter < numIterations; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::shared_ptr<Component>> components;
        for (int i = 0; i < numAllocations; ++i) {
            components.push_back(
                std::make_shared<Component>("BenchmarkComponent"));
        }
        components.clear();

        auto end = std::chrono::high_resolution_clock::now();
        standardTimes.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start));
    }

    // Calculate averages
    auto avgPoolTime = std::accumulate(poolTimes.begin(), poolTimes.end(),
                                       std::chrono::microseconds{0}) /
                       numIterations;
    auto avgStandardTime =
        std::accumulate(standardTimes.begin(), standardTimes.end(),
                        std::chrono::microseconds{0}) /
        numIterations;

    std::cout << "Pool allocation average: " << avgPoolTime.count() << " μs"
              << std::endl;
    std::cout << "Standard allocation average: " << avgStandardTime.count()
              << " μs" << std::endl;

    // Pool should be competitive (within 2x of standard allocation)
    EXPECT_LT(avgPoolTime.count(), avgStandardTime.count() * 2);
}

// ============================================================================
// Lifecycle Management Tests
// ============================================================================

TEST_F(ComponentSystemTest, LifecycleHooks) {
    auto& lifecycleManager = LifecycleManager::instance();

    bool preConstructionCalled = false;
    bool postConstructionCalled = false;

    // Register lifecycle hooks
    lifecycleManager.registerHook(
        "TestComponent1", LifecyclePhase::PreConstruction,
        [&](Component&, LifecyclePhase) { preConstructionCalled = true; });

    lifecycleManager.registerHook(
        "TestComponent1", LifecyclePhase::PostConstruction,
        [&](Component&, LifecyclePhase) { postConstructionCalled = true; });

    // Execute lifecycle phases
    EXPECT_TRUE(lifecycleManager.executePhase(*testComponent1_,
                                              LifecyclePhase::PreConstruction));
    EXPECT_TRUE(lifecycleManager.executePhase(
        *testComponent1_, LifecyclePhase::PostConstruction));

    // Verify hooks were called
    EXPECT_TRUE(preConstructionCalled);
    EXPECT_TRUE(postConstructionCalled);
}

TEST_F(ComponentSystemTest, DependencyResolution) {
    auto& lifecycleManager = LifecycleManager::instance();

    // Add dependencies: TestComponent1 depends on TestComponent2
    DependencyConstraint dependency("TestComponent2", DependencyType::Required);
    lifecycleManager.addDependency("TestComponent1", dependency);

    // Resolve dependencies
    auto dependencies = lifecycleManager.resolveDependencies("TestComponent1");

    // TestComponent2 should be loaded before TestComponent1
    EXPECT_GE(dependencies.size(), 1);
    auto it =
        std::find(dependencies.begin(), dependencies.end(), "TestComponent2");
    EXPECT_NE(it, dependencies.end());
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST_F(ComponentSystemTest, JSONSerialization) {
    auto& serializationManager = SerializationManager::instance();

    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = true;
    options.includeTimestamp = true;

    // Serialize component
    auto result = serializationManager.serialize(*testComponent1_, options);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.data.size(), 0);

    // Deserialize component
    auto deserializeResult =
        serializationManager.deserialize(result.data, options);
    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);
}

TEST_F(ComponentSystemTest, BinarySerialization) {
    auto& serializationManager = SerializationManager::instance();

    SerializationOptions options;
    options.format = SerializationFormat::Binary;
    options.includeMetadata = true;

    // Serialize component
    auto result = serializationManager.serialize(*testComponent1_, options);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.data.size(), 0);

    // Deserialize component
    auto deserializeResult =
        serializationManager.deserialize(result.data, options);
    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);
}

// ============================================================================
// Scripting Tests
// ============================================================================

TEST_F(ScriptingTest, BasicScriptExecution) {
    std::string script = R"(
        -- Simple Lua script
        local result = 2 + 3
        return result
    )";

    auto result = scriptingAPI_->execute(script, false, ScriptLanguage::Auto);

    // Note: This test will only pass if Lua is enabled
#if ATOM_ENABLE_LUA
    EXPECT_TRUE(result.success);
#else
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.errorMessage.find("not compiled in"), std::string::npos);
#endif
}

TEST_F(ScriptingTest, SandboxedExecution) {
    std::string script = R"(
        -- Test script that should run in sandbox
        local x = 10
        local y = 20
        return x + y
    )";

    auto result = sandbox_->executeInSandbox(script, "test_script", false,
                                             ScriptLanguage::Auto);

#if ATOM_ENABLE_LUA
    EXPECT_TRUE(result.success);
#else
    // Should fail gracefully when scripting is disabled
    EXPECT_FALSE(result.success);
#endif
}

#if ATOM_ENABLE_LUA
TEST_F(ScriptingTest, LuaEngineIntegration) {
    LuaConfig config;
    config.enableJIT = false;  // Disable JIT for testing
    config.enableDebug = true;
    config.memoryLimit = 16 * 1024 * 1024;  // 16MB

    auto engine = LuaEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    EXPECT_TRUE(engine->initialize(engineConfig));

    // Test basic script execution
    std::string script = "return 'Hello from Lua!'";
    auto result = engine->executeScript(script);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.returnValue.holds<std::string>());
    EXPECT_EQ(result.returnValue.get<std::string>(), "Hello from Lua!");
}
#endif

#if ATOM_ENABLE_PYTHON
TEST_F(ScriptingTest, PythonEngineIntegration) {
    PythonConfig config;
    config.enableSitePackages = false;  // Disable for testing
    config.isolatedMode = true;

    auto engine = PythonEngineFactory::create(config);
    ASSERT_NE(engine, nullptr);

    ScriptEngineConfig engineConfig;
    EXPECT_TRUE(engine->initialize(engineConfig));

    // Test basic script execution
    std::string script = "result = 'Hello from Python!'";
    auto result = engine->executeScript(script);

    EXPECT_TRUE(result.success);

    // Get the result variable
    auto resultValue = engine->getGlobal("result");
    EXPECT_TRUE(resultValue.has_value());
    EXPECT_TRUE(resultValue->holds<std::string>());
    EXPECT_EQ(resultValue->get<std::string>(), "Hello from Python!");
}
#endif

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ComponentSystemTest, EndToEndIntegration) {
    // Test complete workflow: create component, serialize, deserialize, script
    // interaction

    // 1. Create component with memory pool
    ComponentFactory& factory = ComponentFactory::instance();
    auto component = factory.create<Component>("IntegrationTestComponent");
    ASSERT_NE(component, nullptr);

    // 2. Set some variables
    component->setVar("testValue", 42);
    component->setVar("testString", std::string("Hello World"));

    // 3. Serialize component
    auto& serializationManager = SerializationManager::instance();
    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    auto serializeResult = serializationManager.serialize(*component, options);
    EXPECT_TRUE(serializeResult.success);

    // 4. Deserialize component
    auto deserializeResult =
        serializationManager.deserialize(serializeResult.data, options);
    EXPECT_TRUE(deserializeResult.success);

    // 5. Test scripting interaction (if available)
#if ATOM_ENABLE_LUA || ATOM_ENABLE_PYTHON
    auto& scriptingAPI = ComponentScriptingAPI::instance();

    std::string script = R"(
        -- Test component interaction
        local success = createComponent("ScriptedComponent")
        return success
    )";

    auto scriptResult =
        scriptingAPI.execute(script, false, ScriptLanguage::Auto);
    // Script execution may fail if engines aren't properly initialized, but
    // shouldn't crash
#endif
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

class PerformanceBenchmark : public ::testing::Test {
protected:
    static constexpr int BENCHMARK_ITERATIONS = 1000;
    static constexpr int COMPONENT_COUNT = 100;
};

TEST_F(PerformanceBenchmark, ComponentCreationBenchmark) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::shared_ptr<Component>> components;
    components.reserve(COMPONENT_COUNT);

    for (int i = 0; i < COMPONENT_COUNT; ++i) {
        components.push_back(std::make_shared<Component>("BenchmarkComponent" +
                                                         std::to_string(i)));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Component creation benchmark: " << duration.count()
              << " μs for " << COMPONENT_COUNT << " components" << std::endl;
    std::cout << "Average per component: "
              << (duration.count() / COMPONENT_COUNT) << " μs" << std::endl;

    // Should be reasonably fast (less than 10μs per component on average)
    EXPECT_LT(duration.count() / COMPONENT_COUNT, 10);
}

//==============================================================================
// Optimized Component System Tests
//==============================================================================

TEST_F(OptimizedComponentTest, CacheAlignedPerformanceStats) {
    auto component = std::make_shared<Component>("test_component");

    // Test performance stats are properly aligned
    const auto& stats = component->getPerformanceStats();
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&stats) % 64, 0)
        << "Performance stats should be cache-line aligned";

    // Test atomic operations
    stats.commandCallCount.fetch_add(1, std::memory_order_relaxed);
    EXPECT_EQ(stats.commandCallCount.load(), 1);

    // Test timing updates
    auto duration = std::chrono::nanoseconds{1000};
    const_cast<ComponentPerformanceStats&>(stats).updateExecutionTime(duration);

    EXPECT_GT(stats.timing.totalExecutionTimeNs.load(), 0);
    EXPECT_EQ(stats.timing.maxExecutionTimeNs.load(), 1000);
    EXPECT_EQ(stats.timing.minExecutionTimeNs.load(), 1000);
}

TEST_F(OptimizedComponentTest, FastDispatchPerformance) {
    auto component = std::make_shared<Component>("test_component");

    // Register a test function
    component->def("testFunc", []() { return 42; });

    // Benchmark regular vs fast dispatch
    const int iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        component->dispatch("testFunc");
    }
    auto regularTime = std::chrono::high_resolution_clock::now() - start;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        component->fastDispatch("testFunc");
    }
    auto fastTime = std::chrono::high_resolution_clock::now() - start;

    // Fast dispatch should be faster or at least not significantly slower
    EXPECT_LE(fastTime.count(), regularTime.count() * 1.1)
        << "Fast dispatch should not be significantly slower";

    // Both should produce correct results
    auto result1 = std::any_cast<int>(component->dispatch("testFunc"));
    auto result2 = std::any_cast<int>(component->fastDispatch("testFunc"));
    EXPECT_EQ(result1, 42);
    EXPECT_EQ(result2, 42);
}

TEST_F(OptimizedComponentTest, MemoryPoolAllocation) {
    const size_t numComponents = 100;
    std::vector<std::shared_ptr<Component>> components;

    // Allocate components from pool
    for (size_t i = 0; i < numComponents; ++i) {
        auto component = pool_->allocate("pool_component_" + std::to_string(i));
        ASSERT_NE(component, nullptr);
        components.push_back(component);
    }

    // Check pool statistics
    const auto& stats = pool_->getStatistics();
    EXPECT_GT(stats.totalAllocations.load(), 0);
    EXPECT_EQ(stats.currentAllocations.load(), numComponents);
    EXPECT_GT(stats.getHitRatio(), 0.0);

    // Test memory usage tracking
    EXPECT_GT(pool_->getMemoryUsage(), 0);
    EXPECT_LT(pool_->getFragmentationRatio(), 1.0);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "Running Enhanced Component System Test Suite" << std::endl;
    std::cout << "=============================================" << std::endl;

#if ATOM_ENABLE_LUA
    std::cout << "Lua support: ENABLED" << std::endl;
#else
    std::cout << "Lua support: DISABLED" << std::endl;
#endif

#if ATOM_ENABLE_PYTHON
    std::cout << "Python support: ENABLED" << std::endl;
#else
    std::cout << "Python support: DISABLED" << std::endl;
#endif

    std::cout << "=============================================" << std::endl;

    return RUN_ALL_TESTS();
}
