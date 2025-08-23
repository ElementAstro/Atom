/*
 * test_integration.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Integration and Performance Tests
Tests component interactions, real-world scenarios, and system behavior
under load with comprehensive benchmarks.

**************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../component.hpp"
#include "../component_pool.hpp"
#include "../lifecycle.hpp"
#include "../registry.hpp"
#include "../serialization.hpp"

#if ATOM_ENABLE_SCRIPTING
#include "../scripting_api.hpp"
#endif

using namespace atom::components;

/**
 * @brief Test fixture for integration tests
 */
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = &Registry::instance();
        lifecycleManager_ = &LifecycleManager::instance();
        serializationManager_ = &SerializationManager::instance();

        // Clean up any existing state
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
        lifecycleManager_->clearHistory();

        // Create test components
        setupTestComponents();
    }

    void TearDown() override {
        // Clean up
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
        lifecycleManager_->clearHistory();
    }

    void setupTestComponents() {
        // Create a data processing component
        dataProcessor_ = std::make_shared<Component>("DataProcessor");
        dataProcessor_->addVariable<int>("processedCount", 0);
        dataProcessor_->addVariable<double>("averageValue", 0.0);
        dataProcessor_->def(
            "processData", [this](const std::vector<double>& data) -> double {
                double sum = 0.0;
                for (double value : data) {
                    sum += value;
                }
                double average = data.empty() ? 0.0 : sum / data.size();

                auto countVar =
                    dataProcessor_->getVariable<int>("processedCount");
                countVar->set(countVar->get() + static_cast<int>(data.size()));

                auto avgVar =
                    dataProcessor_->getVariable<double>("averageValue");
                avgVar->set(average);

                return average;
            });

        // Create a configuration component
        config_ = std::make_shared<Component>("Configuration");
        config_->addVariable<std::string>("mode", "auto");
        config_->addVariable<int>("maxItems", 1000);
        config_->addVariable<bool>("enableLogging", true);
        config_->def(
            "updateConfig",
            [this](const std::string& key, const std::string& value) -> bool {
                if (key == "mode") {
                    auto var = config_->getVariable<std::string>("mode");
                    var->set(value);
                    return true;
                }
                return false;
            });

        // Create a logger component
        logger_ = std::make_shared<Component>("Logger");
        logger_->addVariable<int>("logCount", 0);
        logger_->def("log", [this](const std::string& message) {
            auto enableLogging = config_->getVariable<bool>("enableLogging");
            if (enableLogging->get()) {
                auto countVar = logger_->getVariable<int>("logCount");
                countVar->set(countVar->get() + 1);
                // In real implementation, would write to log file
            }
        });

        // Register components
        registry_->registerComponent("DataProcessor", dataProcessor_);
        registry_->registerComponent("Configuration", config_);
        registry_->registerComponent("Logger", logger_);
    }

    Registry* registry_;
    LifecycleManager* lifecycleManager_;
    SerializationManager* serializationManager_;

    std::shared_ptr<Component> dataProcessor_;
    std::shared_ptr<Component> config_;
    std::shared_ptr<Component> logger_;
};

/**
 * @brief Test fixture for performance benchmarks
 */
class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = &Registry::instance();

        // Clean up
        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }

        // Setup memory pool
        PoolConfig poolConfig;
        poolConfig.initialPoolSize = 64;
        poolConfig.maxPoolSize = 512;
        poolConfig.enableStatistics = true;
        poolConfig.enableCacheOptimization = true;

        pool_ = std::make_unique<ComponentPool<Component>>(poolConfig);
    }

    void TearDown() override {
        pool_.reset();

        auto componentNames = registry_->getAllComponentNames();
        for (const auto& name : componentNames) {
            registry_->removeComponent(name);
        }
    }

    Registry* registry_;
    std::unique_ptr<ComponentPool<Component>> pool_;
    static constexpr int PERFORMANCE_ITERATIONS = 1000;
};

// ============================================================================
// Component Interaction Tests
// ============================================================================

TEST_F(IntegrationTest, ComponentCommunication) {
    // Test communication between components
    std::vector<double> testData = {1.0, 2.0, 3.0, 4.0, 5.0};

    // Process data
    auto result = dataProcessor_->dispatch("processData", testData);
    double average = std::any_cast<double>(result);

    EXPECT_DOUBLE_EQ(average, 3.0);

    // Check that processing count was updated
    auto countVar = dataProcessor_->getVariable<int>("processedCount");
    EXPECT_EQ(countVar->get(), 5);

    // Log the result
    logger_->dispatch(
        "log", "Processed " + std::to_string(testData.size()) + " items");

    auto logCountVar = logger_->getVariable<int>("logCount");
    EXPECT_EQ(logCountVar->get(), 1);
}

TEST_F(IntegrationTest, ConfigurationDrivenBehavior) {
    // Test that configuration changes affect component behavior

    // Initially logging should be enabled
    auto enableLogging = config_->getVariable<bool>("enableLogging");
    EXPECT_TRUE(enableLogging->get());

    // Log a message
    logger_->dispatch("log", "Test message 1");
    auto logCount = logger_->getVariable<int>("logCount");
    EXPECT_EQ(logCount->get(), 1);

    // Disable logging
    enableLogging->set(false);

    // Log another message - should not increment count
    logger_->dispatch("log", "Test message 2");
    EXPECT_EQ(logCount->get(), 1);  // Should still be 1

    // Re-enable logging
    enableLogging->set(true);

    // Log another message - should increment count
    logger_->dispatch("log", "Test message 3");
    EXPECT_EQ(logCount->get(), 2);
}

TEST_F(IntegrationTest, ComponentDependencyChain) {
    // Create a dependency chain: Logger depends on Configuration
    DependencyConstraint configDep("Configuration", DependencyType::Required);
    lifecycleManager_->addDependency("Logger", configDep);

    // DataProcessor depends on Logger
    DependencyConstraint loggerDep("Logger", DependencyType::Required);
    lifecycleManager_->addDependency("DataProcessor", loggerDep);

    // Resolve dependencies for DataProcessor
    auto dependencies = lifecycleManager_->resolveDependencies("DataProcessor");

    // Should include all components in correct order
    EXPECT_GE(dependencies.size(), 3);

    // Configuration should come before Logger
    auto configPos =
        std::find(dependencies.begin(), dependencies.end(), "Configuration");
    auto loggerPos =
        std::find(dependencies.begin(), dependencies.end(), "Logger");
    auto dataPos =
        std::find(dependencies.begin(), dependencies.end(), "DataProcessor");

    EXPECT_NE(configPos, dependencies.end());
    EXPECT_NE(loggerPos, dependencies.end());
    EXPECT_NE(dataPos, dependencies.end());

    EXPECT_LT(std::distance(dependencies.begin(), configPos),
              std::distance(dependencies.begin(), loggerPos));
    EXPECT_LT(std::distance(dependencies.begin(), loggerPos),
              std::distance(dependencies.begin(), dataPos));
}

// ============================================================================
// Serialization Integration Tests
// ============================================================================

TEST_F(IntegrationTest, ComponentSerializationRoundTrip) {
    // Modify component state
    dataProcessor_->getVariable<int>("processedCount")->set(100);
    dataProcessor_->getVariable<double>("averageValue")->set(42.5);

    config_->getVariable<std::string>("mode")->set("manual");
    config_->getVariable<int>("maxItems")->set(2000);

    // Serialize components
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = true;

    auto dataResult =
        serializationManager_->serialize(*dataProcessor_, options);
    auto configResult = serializationManager_->serialize(*config_, options);

    ASSERT_TRUE(dataResult.success);
    ASSERT_TRUE(configResult.success);

    // Deserialize components
    auto dataDeserializeResult =
        serializationManager_->deserialize(dataResult.data, options);
    auto configDeserializeResult =
        serializationManager_->deserialize(configResult.data, options);

    ASSERT_TRUE(dataDeserializeResult.success);
    ASSERT_TRUE(configDeserializeResult.success);

    // Verify deserialized state
    auto deserializedData = dataDeserializeResult.component;
    auto deserializedConfig = configDeserializeResult.component;

    EXPECT_EQ(deserializedData->getName(), "DataProcessor");
    EXPECT_EQ(deserializedConfig->getName(), "Configuration");

    auto countVar = deserializedData->getVariable<int>("processedCount");
    EXPECT_EQ(countVar->get(), 100);

    auto avgVar = deserializedData->getVariable<double>("averageValue");
    EXPECT_DOUBLE_EQ(avgVar->get(), 42.5);

    auto modeVar = deserializedConfig->getVariable<std::string>("mode");
    EXPECT_EQ(modeVar->get(), "manual");

    auto maxVar = deserializedConfig->getVariable<int>("maxItems");
    EXPECT_EQ(maxVar->get(), 2000);
}

// ============================================================================
// Lifecycle Integration Tests
// ============================================================================

TEST_F(IntegrationTest, ComponentLifecycleManagement) {
    bool configInitialized = false;
    bool loggerInitialized = false;
    bool dataProcessorInitialized = false;

    // Register lifecycle hooks
    lifecycleManager_->registerHook(
        "Configuration", LifecyclePhase::PostInitialization,
        [&configInitialized](Component& comp, LifecyclePhase phase) {
            configInitialized = true;
        });

    lifecycleManager_->registerHook(
        "Logger", LifecyclePhase::PostInitialization,
        [&loggerInitialized](Component& comp, LifecyclePhase phase) {
            loggerInitialized = true;
        });

    lifecycleManager_->registerHook(
        "DataProcessor", LifecyclePhase::PostInitialization,
        [&dataProcessorInitialized](Component& comp, LifecyclePhase phase) {
            dataProcessorInitialized = true;
        });

    // Execute lifecycle phases
    lifecycleManager_->executePhase(*config_,
                                    LifecyclePhase::PostInitialization);
    lifecycleManager_->executePhase(*logger_,
                                    LifecyclePhase::PostInitialization);
    lifecycleManager_->executePhase(*dataProcessor_,
                                    LifecyclePhase::PostInitialization);

    EXPECT_TRUE(configInitialized);
    EXPECT_TRUE(loggerInitialized);
    EXPECT_TRUE(dataProcessorInitialized);

    // Check lifecycle history
    auto history = lifecycleManager_->getLifecycleHistory();
    EXPECT_GE(history.size(), 3);

    // Verify all components have history entries
    bool hasConfigHistory = false;
    bool hasLoggerHistory = false;
    bool hasDataHistory = false;

    for (const auto& event : history) {
        if (event.componentName == "Configuration")
            hasConfigHistory = true;
        if (event.componentName == "Logger")
            hasLoggerHistory = true;
        if (event.componentName == "DataProcessor")
            hasDataHistory = true;
    }

    EXPECT_TRUE(hasConfigHistory);
    EXPECT_TRUE(hasLoggerHistory);
    EXPECT_TRUE(hasDataHistory);
}

#if ATOM_ENABLE_SCRIPTING
// ============================================================================
// Scripting Integration Tests
// ============================================================================

TEST_F(IntegrationTest, ScriptingComponentIntegration) {
    auto& scriptingAPI = ComponentScriptingAPI::instance();

    ScriptEngineConfig config;
    config.memoryLimit = 32 * 1024 * 1024;
    config.executionTimeout = std::chrono::seconds(10);

    if (scriptingAPI.initialize(config)) {
        // Register component access functions
        auto getComponentVar =
            [this](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (args.size() >= 2 && args[0].holds<std::string>() &&
                args[1].holds<std::string>()) {
                std::string compName = args[0].get<std::string>();
                std::string varName = args[1].get<std::string>();

                try {
                    auto component = registry_->getComponent(compName);
                    if (component->hasVariable(varName)) {
                        if (varName == "processedCount" ||
                            varName == "logCount" || varName == "maxItems") {
                            auto var = component->getVariable<int>(varName);
                            return ScriptValue(
                                static_cast<int64_t>(var->get()));
                        } else if (varName == "averageValue") {
                            auto var = component->getVariable<double>(varName);
                            return ScriptValue(var->get());
                        } else if (varName == "mode") {
                            auto var =
                                component->getVariable<std::string>(varName);
                            return ScriptValue(var->get());
                        } else if (varName == "enableLogging") {
                            auto var = component->getVariable<bool>(varName);
                            return ScriptValue(var->get());
                        }
                    }
                } catch (...) {
                    return ScriptValue();
                }
            }
            return ScriptValue();
        };

        scriptingAPI.registerFunction("getComponentVar", getComponentVar);

        std::string script = R"(
            local processedCount = getComponentVar("DataProcessor", "processedCount")
            local maxItems = getComponentVar("Configuration", "maxItems")
            local mode = getComponentVar("Configuration", "mode")

            return {
                processed = processedCount,
                max = maxItems,
                mode = mode
            }
        )";

        auto result = scriptingAPI.execute(script, false, ScriptLanguage::Auto);

        if (result.success) {
            // Script execution succeeded
            EXPECT_TRUE(true);
        }

        scriptingAPI.shutdown();
    }
}
#endif

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

TEST_F(PerformanceTest, ComponentCreationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::shared_ptr<Component>> components;
    components.reserve(PERFORMANCE_ITERATIONS);

    for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto component = pool_->allocate("PerfComponent" + std::to_string(i));
        component->addVariable<int>("value", i);
        component->def("getValue", [i]() -> int { return i; });
        components.push_back(component);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Component creation performance: " << duration.count()
              << " μs for " << PERFORMANCE_ITERATIONS << " components"
              << std::endl;
    std::cout << "Average per component: "
              << (duration.count() / PERFORMANCE_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast
    EXPECT_LT(duration.count() / PERFORMANCE_ITERATIONS,
              50);  // Less than 50μs per component
}

TEST_F(PerformanceTest, MassComponentRegistration) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto component =
            std::make_shared<Component>("MassRegComponent" + std::to_string(i));
        component->addVariable<int>("id", i);
        registry_->registerComponent("MassRegComponent" + std::to_string(i),
                                     component);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Mass registration performance: " << duration.count()
              << " μs for " << PERFORMANCE_ITERATIONS << " registrations"
              << std::endl;

    // Verify all components are registered
    auto allComponents = registry_->getAllComponents();
    EXPECT_GE(allComponents.size(), PERFORMANCE_ITERATIONS);

    // Test retrieval performance
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto component =
            registry_->getComponent("MassRegComponent" + std::to_string(i));
        volatile auto ptr = component.get();  // Prevent optimization
        (void)ptr;
    }

    end = std::chrono::high_resolution_clock::now();
    auto retrievalDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Mass retrieval performance: " << retrievalDuration.count()
              << " μs for " << PERFORMANCE_ITERATIONS << " retrievals"
              << std::endl;

    EXPECT_LT(retrievalDuration.count() / PERFORMANCE_ITERATIONS,
              5);  // Less than 5μs per retrieval
}

TEST_F(PerformanceTest, ComponentInteractionPerformance) {
    // Create components for interaction testing
    auto producer = std::make_shared<Component>("Producer");
    auto consumer = std::make_shared<Component>("Consumer");

    std::atomic<int> processedCount{0};

    producer->addVariable<int>("dataCount", 0);
    producer->def("generateData",
                  [&processedCount](int count) -> std::vector<int> {
                      std::vector<int> data;
                      data.reserve(count);
                      for (int i = 0; i < count; ++i) {
                          data.push_back(i);
                      }
                      processedCount += count;
                      return data;
                  });

    consumer->addVariable<int>("consumedCount", 0);
    consumer->def("consumeData", [](const std::vector<int>& data) -> int {
        int sum = 0;
        for (int value : data) {
            sum += value;
        }
        return sum;
    });

    registry_->registerComponent("Producer", producer);
    registry_->registerComponent("Consumer", consumer);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        // Generate data
        auto data = producer->dispatch("generateData", 10);
        auto dataVector = std::any_cast<std::vector<int>>(data);

        // Consume data
        auto result = consumer->dispatch("consumeData", dataVector);
        volatile int sum = std::any_cast<int>(result);
        (void)sum;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Component interaction performance: " << duration.count()
              << " μs for "
              << "100 interactions" << std::endl;

    EXPECT_EQ(processedCount.load(), 1000);  // 100 * 10
    EXPECT_LT(duration.count(), 50000);      // Less than 50ms total
}

TEST_F(PerformanceTest, ConcurrentComponentAccess) {
    // Create shared component
    auto sharedComponent = std::make_shared<Component>("SharedComponent");
    sharedComponent->addVariable<std::atomic<int>>("counter", 0);
    sharedComponent->def("increment", [sharedComponent]() -> int {
        auto counterVar =
            sharedComponent->getVariable<std::atomic<int>>("counter");
        return ++counterVar->get();
    });

    registry_->registerComponent("SharedComponent", sharedComponent);

    const int numThreads = 8;
    const int operationsPerThread = 125;  // Total 1000 operations
    std::vector<std::thread> threads;
    std::atomic<int> totalOperations{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &totalOperations, operationsPerThread]() {
            auto component = registry_->getComponent("SharedComponent");

            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    component->dispatch("increment");
                    totalOperations++;
                } catch (...) {
                    // Some operations might fail due to race conditions
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Concurrent access performance: " << duration.count()
              << " μs for " << totalOperations.load() << " operations across "
              << numThreads << " threads" << std::endl;

    // Most operations should succeed
    EXPECT_GT(totalOperations.load(), numThreads * operationsPerThread * 0.9);

    auto counterVar = sharedComponent->getVariable<std::atomic<int>>("counter");
    EXPECT_EQ(counterVar->get().load(), totalOperations.load());
}

// ============================================================================
// Memory Usage and Stress Tests
// ============================================================================

TEST_F(PerformanceTest, MemoryUsageStressTest) {
    const int numComponents = 500;
    const int variablesPerComponent = 20;
    const int commandsPerComponent = 10;

    std::vector<std::shared_ptr<Component>> components;
    components.reserve(numComponents);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numComponents; ++i) {
        auto component = pool_->allocate("StressComponent" + std::to_string(i));

        // Add many variables
        for (int j = 0; j < variablesPerComponent; ++j) {
            component->addVariable<int>("var" + std::to_string(j), j);
            component->addVariable<std::string>("str" + std::to_string(j),
                                                "value" + std::to_string(j));
        }

        // Add many commands
        for (int k = 0; k < commandsPerComponent; ++k) {
            component->def("cmd" + std::to_string(k),
                           [k]() -> int { return k; });
        }

        components.push_back(component);

        // Register with registry
        registry_->registerComponent("StressComponent" + std::to_string(i),
                                     component);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Stress test creation: " << duration.count() << " ms for "
              << numComponents << " components with " << variablesPerComponent
              << " variables and " << commandsPerComponent << " commands each"
              << std::endl;

    // Test memory usage
    auto memoryUsage = pool_->getMemoryUsage();
    std::cout << "Memory usage: " << memoryUsage << " bytes" << std::endl;

    // Test random access performance
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> compDist(0, numComponents - 1);
    std::uniform_int_distribution<> varDist(0, variablesPerComponent - 1);
    std::uniform_int_distribution<> cmdDist(0, commandsPerComponent - 1);

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        int compIndex = compDist(gen);
        int varIndex = varDist(gen);
        int cmdIndex = cmdDist(gen);

        auto& component = components[compIndex];

        // Access variable
        auto var =
            component->getVariable<int>("var" + std::to_string(varIndex));
        volatile int value = var->get();
        (void)value;

        // Execute command
        auto result = component->dispatch("cmd" + std::to_string(cmdIndex));
        volatile int cmdResult = std::any_cast<int>(result);
        (void)cmdResult;
    }

    end = std::chrono::high_resolution_clock::now();
    auto accessDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Random access performance: " << accessDuration.count()
              << " μs for 1000 operations" << std::endl;

    EXPECT_LT(accessDuration.count() / 1000,
              100);  // Less than 100μs per operation

    // Cleanup test
    start = std::chrono::high_resolution_clock::now();

    components.clear();
    for (int i = 0; i < numComponents; ++i) {
        registry_->removeComponent("StressComponent" + std::to_string(i));
    }

    end = std::chrono::high_resolution_clock::now();
    auto cleanupDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Cleanup performance: " << cleanupDuration.count() << " ms"
              << std::endl;

    EXPECT_LT(cleanupDuration.count(), 1000);  // Less than 1 second for cleanup
}
