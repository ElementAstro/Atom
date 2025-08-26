/*
 * comprehensive_integration_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Comprehensive Integration Example
Demonstrates integration of multiple component system features including
registry management, lifecycle hooks, variable management, command dispatch,
serialization, and optional scripting support.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <fstream>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/lifecycle.hpp"
#include "atom/components/serialization.hpp"

// Conditional scripting support
#if ATOM_ENABLE_LUA
#include "atom/components/lua_engine.hpp"
#include "atom/components/scripting_api.hpp"
#endif

using namespace atom::components;

/**
 * @brief Data processing component with full feature integration
 */
class DataProcessorComponent : public Component {
public:
    explicit DataProcessorComponent(const std::string& name) : Component(name) {
        std::cout << "DataProcessorComponent '" << name << "' created" << std::endl;
        
        // Initialize processing variables
        addVariable<int>("processedCount", 0);
        addVariable<double>("processingRate", 100.0);
        addVariable<bool>("enabled", true);
        addVariable<std::string>("status", "idle");
        addVariable<std::vector<std::string>>("dataQueue", std::vector<std::string>());
        
        // Add processing commands
        def("processData", [this](const std::string& data) {
            auto enabled = getVariable<bool>("enabled");
            if (!enabled || !enabled->get()) {
                std::cout << "  Processing disabled" << std::endl;
                return;
            }
            
            // Add to queue
            auto queue = getVariable<std::vector<std::string>>("dataQueue");
            if (queue) {
                auto currentQueue = queue->get();
                currentQueue.push_back(data);
                setValue("dataQueue", currentQueue);
            }
            
            // Update counters
            auto count = getVariable<int>("processedCount");
            if (count) {
                setValue("processedCount", count->get() + 1);
            }
            
            setValue("status", "processing");
            std::cout << "  Processed data: " << data << std::endl;
        });
        
        def("getStats", [this]() {
            auto count = getVariable<int>("processedCount");
            auto rate = getVariable<double>("processingRate");
            auto status = getVariable<std::string>("status");
            
            std::cout << "  Processing Stats:" << std::endl;
            std::cout << "    Count: " << count->get() << std::endl;
            std::cout << "    Rate: " << rate->get() << " items/sec" << std::endl;
            std::cout << "    Status: " << status->get() << std::endl;
        });
        
        def("clearQueue", [this]() {
            setValue("dataQueue", std::vector<std::string>());
            std::cout << "  Data queue cleared" << std::endl;
        });
    }
};

/**
 * @brief Service component with lifecycle management
 */
class ServiceComponent : public Component {
public:
    explicit ServiceComponent(const std::string& name) : Component(name) {
        std::cout << "ServiceComponent '" << name << "' created" << std::endl;
        
        // Initialize service variables
        addVariable<std::string>("serviceName", "DefaultService");
        addVariable<bool>("running", false);
        addVariable<int>("port", 8080);
        addVariable<std::string>("host", "localhost");
        
        // Register lifecycle hooks
        auto& lifecycle = LifecycleManager::instance();
        
        lifecycle.registerHook(name, LifecyclePhase::Initialize, 
            [this](Component& comp, LifecyclePhase phase) {
                std::cout << "  Service initializing..." << std::endl;
                setValue("running", false);
            });
            
        lifecycle.registerHook(name, LifecyclePhase::Activate,
            [this](Component& comp, LifecyclePhase phase) {
                std::cout << "  Service starting..." << std::endl;
                setValue("running", true);
            });
            
        lifecycle.registerHook(name, LifecyclePhase::Deactivate,
            [this](Component& comp, LifecyclePhase phase) {
                std::cout << "  Service stopping..." << std::endl;
                setValue("running", false);
            });
        
        // Add service commands
        def("start", [this]() {
            auto& lifecycle = LifecycleManager::instance();
            lifecycle.executePhase(*this, LifecyclePhase::Activate);
        });
        
        def("stop", [this]() {
            auto& lifecycle = LifecycleManager::instance();
            lifecycle.executePhase(*this, LifecyclePhase::Deactivate);
        });
        
        def("isRunning", [this]() -> bool {
            auto running = getVariable<bool>("running");
            return running ? running->get() : false;
        });
    }
};

/**
 * @brief Comprehensive integration demonstration
 */
class IntegrationDemo {
private:
    std::shared_ptr<DataProcessorComponent> processor_;
    std::shared_ptr<ServiceComponent> service_;
    
#if ATOM_ENABLE_LUA
    std::unique_ptr<atom::components::scripting::LuaEngine> luaEngine_;
#endif

public:
    IntegrationDemo() {
        std::cout << "IntegrationDemo created" << std::endl;
        initializeComponents();
        setupScripting();
    }
    
    void initializeComponents() {
        std::cout << "\n1. Creating and registering components..." << std::endl;
        
        auto& registry = Registry::instance();
        
        // Create components
        processor_ = registry.createComponent<DataProcessorComponent>("DataProcessor");
        service_ = registry.createComponent<ServiceComponent>("WebService");
        
        // Initialize lifecycle
        auto& lifecycle = LifecycleManager::instance();
        lifecycle.executePhase(*processor_, LifecyclePhase::Initialize);
        lifecycle.executePhase(*service_, LifecyclePhase::Initialize);
    }
    
    void setupScripting() {
#if ATOM_ENABLE_LUA
        std::cout << "\n2. Setting up Lua scripting..." << std::endl;
        
        atom::components::scripting::LuaConfig config;
        config.enableJIT = true;
        config.enableDebug = true;
        
        luaEngine_ = std::make_unique<atom::components::scripting::LuaEngine>(config);
        
        atom::components::scripting::ScriptEngineConfig engineConfig;
        if (luaEngine_->initialize(engineConfig)) {
            std::cout << "  Lua engine initialized successfully" << std::endl;
            
            // Register component API
            atom::components::ComponentScriptingAPI::registerComponentAPI(*luaEngine_);
            
            // Execute a simple Lua script
            std::string luaScript = R"(
                print("Lua script executing...")
                -- This would interact with components if fully implemented
                print("Script execution complete")
            )";
            
            auto result = luaEngine_->executeScript(luaScript, "demo_script");
            if (result.success) {
                std::cout << "  Lua script executed successfully" << std::endl;
            } else {
                std::cout << "  Lua script failed: " << result.errorMessage << std::endl;
            }
        }
#else
        std::cout << "\n2. Scripting support not compiled in" << std::endl;
#endif
    }
    
    void demonstrateFeatures() {
        std::cout << "\n3. Demonstrating integrated features..." << std::endl;
        
        // Lifecycle management
        std::cout << "\n--- Lifecycle Management ---" << std::endl;
        service_->executeCommand("start", {});
        
        // Variable management and command dispatch
        std::cout << "\n--- Data Processing ---" << std::endl;
        processor_->executeCommand("processData", {"sample_data_1"});
        processor_->executeCommand("processData", {"sample_data_2"});
        processor_->executeCommand("processData", {"sample_data_3"});
        processor_->executeCommand("getStats", {});
        
        // Serialization demonstration
        std::cout << "\n--- Serialization ---" << std::endl;
        demonstrateSerialization();
        
        // Component registry operations
        std::cout << "\n--- Registry Operations ---" << std::endl;
        demonstrateRegistry();
        
        // Cleanup
        std::cout << "\n--- Cleanup ---" << std::endl;
        processor_->executeCommand("clearQueue", {});
        service_->executeCommand("stop", {});
    }
    
    void demonstrateSerialization() {
        try {
            // Create serializer
            ComponentSerializer serializer;
            
            // Serialize processor component
            std::string serializedData = serializer.serialize(*processor_);
            std::cout << "  Component serialized successfully" << std::endl;
            std::cout << "  Serialized data length: " << serializedData.length() << " bytes" << std::endl;
            
            // Save to file
            std::ofstream file("processor_component.json");
            if (file.is_open()) {
                file << serializedData;
                file.close();
                std::cout << "  Serialized data saved to file" << std::endl;
            }
            
            // Demonstrate deserialization (would create new component)
            std::cout << "  Deserialization would restore component state" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "  Serialization error: " << e.what() << std::endl;
        }
    }
    
    void demonstrateRegistry() {
        auto& registry = Registry::instance();
        
        // List all components
        auto components = registry.getAllComponents();
        std::cout << "  Total components in registry: " << components.size() << std::endl;
        
        for (const auto& comp : components) {
            if (comp) {
                std::cout << "    - " << comp->getName() 
                          << " (State: " << static_cast<int>(comp->getState()) << ")" << std::endl;
            }
        }
        
        // Demonstrate component lookup
        auto foundProcessor = registry.getComponent("DataProcessor");
        if (foundProcessor) {
            std::cout << "  Successfully retrieved DataProcessor from registry" << std::endl;
        }
    }
    
    void runPerformanceTest() {
        std::cout << "\n4. Running performance test..." << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Process many items
        for (int i = 0; i < 1000; ++i) {
            processor_->executeCommand("processData", {"test_data_" + std::to_string(i)});
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        std::cout << "  Processed 1000 items in " << duration.count() << " microseconds" << std::endl;
        std::cout << "  Average: " << (duration.count() / 1000.0) << " microseconds per item" << std::endl;
        
        processor_->executeCommand("getStats", {});
    }
};

int main() {
    std::cout << "=== Atom Component Comprehensive Integration Example ===" << std::endl;
    
    try {
        // Create and run integration demo
        IntegrationDemo demo;
        
        // Demonstrate all features
        demo.demonstrateFeatures();
        
        // Run performance test
        demo.runPerformanceTest();
        
        std::cout << "\n=== Comprehensive Integration Example Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
