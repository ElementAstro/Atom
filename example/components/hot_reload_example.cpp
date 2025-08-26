/*
 * hot_reload_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Hot Reload Example
Demonstrates runtime component updates and hot reloading capabilities using
the Atom component framework. Shows how components can be modified, reloaded,
and updated without stopping the application.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/lifecycle.hpp"

using namespace atom::components;

/**
 * @brief Configuration component that can be hot-reloaded
 */
class ConfigComponent : public Component {
private:
    std::atomic<int> version_{1};
    
public:
    explicit ConfigComponent(const std::string& name) : Component(name) {
        std::cout << "ConfigComponent '" << name << "' created (v" << version_ << ")" << std::endl;
        initializeConfig();
    }
    
    void initializeConfig() {
        // Initialize configuration variables
        addVariable<std::string>("serverUrl", "http://localhost:8080");
        addVariable<int>("timeout", 30);
        addVariable<bool>("debugMode", false);
        addVariable<double>("refreshRate", 60.0);
        addVariable<std::string>("theme", "default");
        
        // Add configuration management commands
        def("reload", [this]() {
            std::cout << "  Reloading configuration..." << std::endl;
            version_++;
            
            // Simulate loading new configuration
            setValue("serverUrl", "http://production.server.com:9090");
            setValue("timeout", 45);
            setValue("debugMode", true);
            setValue("refreshRate", 120.0);
            setValue("theme", "dark");
            
            std::cout << "  Configuration reloaded (v" << version_ << ")" << std::endl;
        });
        
        def("getVersion", [this]() -> int {
            return version_.load();
        });
        
        def("printConfig", [this]() {
            auto serverUrl = getVariable<std::string>("serverUrl");
            auto timeout = getVariable<int>("timeout");
            auto debugMode = getVariable<bool>("debugMode");
            auto refreshRate = getVariable<double>("refreshRate");
            auto theme = getVariable<std::string>("theme");
            
            std::cout << "  Configuration (v" << version_ << "):" << std::endl;
            std::cout << "    Server URL: " << serverUrl->get() << std::endl;
            std::cout << "    Timeout: " << timeout->get() << "s" << std::endl;
            std::cout << "    Debug Mode: " << (debugMode->get() ? "ON" : "OFF") << std::endl;
            std::cout << "    Refresh Rate: " << refreshRate->get() << " Hz" << std::endl;
            std::cout << "    Theme: " << theme->get() << std::endl;
        });
    }
};

/**
 * @brief Behavior component that can be hot-swapped
 */
class BehaviorComponent : public Component {
private:
    std::function<void()> currentBehavior_;
    std::string behaviorName_;
    
public:
    explicit BehaviorComponent(const std::string& name) : Component(name) {
        std::cout << "BehaviorComponent '" << name << "' created" << std::endl;
        setBehavior("default");
        
        // Add behavior management commands
        def("setBehavior", [this](const std::string& behaviorName) {
            setBehavior(behaviorName);
        });
        
        def("execute", [this]() {
            if (currentBehavior_) {
                std::cout << "  Executing behavior: " << behaviorName_ << std::endl;
                currentBehavior_();
            }
        });
        
        def("getCurrentBehavior", [this]() -> std::string {
            return behaviorName_;
        });
    }
    
private:
    void setBehavior(const std::string& name) {
        behaviorName_ = name;
        
        if (name == "aggressive") {
            currentBehavior_ = [this]() {
                std::cout << "    Performing aggressive actions!" << std::endl;
                setValue("aggressionLevel", 100);
            };
            addVariable<int>("aggressionLevel", 100);
        } else if (name == "defensive") {
            currentBehavior_ = [this]() {
                std::cout << "    Taking defensive stance..." << std::endl;
                setValue("defenseLevel", 80);
            };
            addVariable<int>("defenseLevel", 80);
        } else if (name == "passive") {
            currentBehavior_ = [this]() {
                std::cout << "    Remaining passive and observing..." << std::endl;
                setValue("observationLevel", 60);
            };
            addVariable<int>("observationLevel", 60);
        } else {
            currentBehavior_ = [this]() {
                std::cout << "    Executing default behavior..." << std::endl;
                setValue("defaultLevel", 50);
            };
            addVariable<int>("defaultLevel", 50);
        }
        
        std::cout << "  Behavior set to: " << name << std::endl;
    }
};

/**
 * @brief Hot reload manager for managing component updates
 */
class HotReloadManager {
private:
    std::unordered_map<std::string, std::shared_ptr<Component>> watchedComponents_;
    std::atomic<bool> running_{false};
    std::thread watchThread_;
    
public:
    HotReloadManager() {
        std::cout << "HotReloadManager created" << std::endl;
    }
    
    ~HotReloadManager() {
        stop();
    }
    
    void watchComponent(const std::string& name, std::shared_ptr<Component> component) {
        watchedComponents_[name] = component;
        std::cout << "Now watching component: " << name << std::endl;
    }
    
    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        watchThread_ = std::thread([this]() {
            std::cout << "Hot reload watcher started" << std::endl;
            
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                
                if (!running_) break;
                
                // Simulate file change detection and hot reload
                simulateHotReload();
            }
            
            std::cout << "Hot reload watcher stopped" << std::endl;
        });
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            if (watchThread_.joinable()) {
                watchThread_.join();
            }
        }
    }
    
    void triggerReload(const std::string& componentName) {
        auto it = watchedComponents_.find(componentName);
        if (it != watchedComponents_.end()) {
            std::cout << "\n--- Hot Reloading Component: " << componentName << " ---" << std::endl;
            
            auto component = it->second;
            
            // Simulate component hot reload
            if (componentName.find("Config") != std::string::npos) {
                component->executeCommand("reload", {});
            } else if (componentName.find("Behavior") != std::string::npos) {
                // Randomly change behavior
                std::vector<std::string> behaviors = {"aggressive", "defensive", "passive", "default"};
                static int behaviorIndex = 0;
                component->executeCommand("setBehavior", {behaviors[behaviorIndex % behaviors.size()]});
                behaviorIndex++;
            }
            
            std::cout << "--- Hot Reload Complete ---\n" << std::endl;
        }
    }
    
private:
    void simulateHotReload() {
        if (watchedComponents_.empty()) {
            return;
        }
        
        // Randomly select a component to reload
        static int reloadCounter = 0;
        std::vector<std::string> componentNames;
        for (const auto& pair : watchedComponents_) {
            componentNames.push_back(pair.first);
        }
        
        if (!componentNames.empty()) {
            std::string componentToReload = componentNames[reloadCounter % componentNames.size()];
            triggerReload(componentToReload);
            reloadCounter++;
        }
    }
};

/**
 * @brief Application that demonstrates hot reloading
 */
class HotReloadApp {
private:
    std::shared_ptr<ConfigComponent> config_;
    std::shared_ptr<BehaviorComponent> behavior_;
    HotReloadManager reloadManager_;
    std::atomic<bool> running_{false};
    
public:
    HotReloadApp() {
        std::cout << "HotReloadApp created" << std::endl;
        
        // Create components
        auto& registry = Registry::instance();
        config_ = registry.createComponent<ConfigComponent>("AppConfig");
        behavior_ = registry.createComponent<BehaviorComponent>("AppBehavior");
        
        // Register components with hot reload manager
        reloadManager_.watchComponent("AppConfig", config_);
        reloadManager_.watchComponent("AppBehavior", behavior_);
    }
    
    void run() {
        std::cout << "\n=== Starting Hot Reload Application ===" << std::endl;
        
        running_ = true;
        reloadManager_.start();
        
        // Main application loop
        int iteration = 0;
        while (running_ && iteration < 10) {
            std::cout << "\n--- Application Iteration " << (iteration + 1) << " ---" << std::endl;
            
            // Use configuration
            config_->executeCommand("printConfig", {});
            
            // Execute behavior
            behavior_->executeCommand("execute", {});
            
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            iteration++;
        }
        
        stop();
    }
    
    void stop() {
        std::cout << "\nStopping application..." << std::endl;
        running_ = false;
        reloadManager_.stop();
    }
    
    void manualReload(const std::string& componentName) {
        reloadManager_.triggerReload(componentName);
    }
};

int main() {
    std::cout << "=== Atom Component Hot Reload Example ===" << std::endl;
    
    try {
        // Create and run the hot reload application
        HotReloadApp app;
        
        // Start the application in a separate thread
        std::thread appThread([&app]() {
            app.run();
        });
        
        // Simulate manual hot reloads
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "\n>>> Manual hot reload triggered <<<" << std::endl;
        app.manualReload("AppConfig");
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "\n>>> Manual hot reload triggered <<<" << std::endl;
        app.manualReload("AppBehavior");
        
        // Wait for application to complete
        if (appThread.joinable()) {
            appThread.join();
        }
        
        std::cout << "\n=== Hot Reload Example Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
