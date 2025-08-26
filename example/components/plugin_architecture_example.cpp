/*
 * plugin_architecture_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Plugin Architecture Example
Demonstrates dynamic component loading and management using the Atom component
framework. Shows how to create a plugin system where components can be loaded,
unloaded, and managed at runtime.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/lifecycle.hpp"

using namespace atom::components;

/**
 * @brief Plugin interface for loadable components
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual std::shared_ptr<Component> createComponent(const std::string& name) = 0;
};

/**
 * @brief Audio processing plugin component
 */
class AudioProcessorComponent : public Component {
public:
    explicit AudioProcessorComponent(const std::string& name) : Component(name) {
        std::cout << "AudioProcessorComponent '" << name << "' created" << std::endl;
        
        // Initialize audio processing variables
        addVariable<double>("volume", 1.0);
        addVariable<int>("sampleRate", 44100);
        addVariable<bool>("enabled", true);
        addVariable<std::string>("effect", "none");
        
        // Add audio processing commands
        def("setVolume", [this](double volume) {
            setValue("volume", std::clamp(volume, 0.0, 2.0));
            auto vol = getVariable<double>("volume");
            std::cout << "  Volume set to: " << vol->get() << std::endl;
        });
        
        def("applyEffect", [this](const std::string& effect) {
            setValue("effect", effect);
            std::cout << "  Applied effect: " << effect << std::endl;
        });
        
        def("process", [this]() {
            auto enabled = getVariable<bool>("enabled");
            auto effect = getVariable<std::string>("effect");
            auto volume = getVariable<double>("volume");
            
            if (enabled && enabled->get()) {
                std::cout << "  Processing audio with effect: " << effect->get() 
                          << ", volume: " << volume->get() << std::endl;
            } else {
                std::cout << "  Audio processing disabled" << std::endl;
            }
        });
    }
};

/**
 * @brief Audio plugin implementation
 */
class AudioPlugin : public IPlugin {
public:
    std::string getName() const override { return "AudioPlugin"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::string getDescription() const override { return "Audio processing plugin"; }
    
    bool initialize() override {
        std::cout << "AudioPlugin initialized" << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "AudioPlugin shutdown" << std::endl;
    }
    
    std::shared_ptr<Component> createComponent(const std::string& name) override {
        return std::make_shared<AudioProcessorComponent>(name);
    }
};

/**
 * @brief Graphics rendering plugin component
 */
class GraphicsRendererComponent : public Component {
public:
    explicit GraphicsRendererComponent(const std::string& name) : Component(name) {
        std::cout << "GraphicsRendererComponent '" << name << "' created" << std::endl;
        
        // Initialize graphics variables
        addVariable<int>("width", 1920);
        addVariable<int>("height", 1080);
        addVariable<bool>("vsync", true);
        addVariable<std::string>("renderer", "OpenGL");
        addVariable<double>("fps", 60.0);
        
        // Add graphics commands
        def("setResolution", [this](int width, int height) {
            setValue("width", width);
            setValue("height", height);
            std::cout << "  Resolution set to: " << width << "x" << height << std::endl;
        });
        
        def("setRenderer", [this](const std::string& renderer) {
            setValue("renderer", renderer);
            std::cout << "  Renderer set to: " << renderer << std::endl;
        });
        
        def("render", [this]() {
            auto width = getVariable<int>("width");
            auto height = getVariable<int>("height");
            auto renderer = getVariable<std::string>("renderer");
            auto fps = getVariable<double>("fps");
            
            std::cout << "  Rendering frame at " << width->get() << "x" << height->get() 
                      << " using " << renderer->get() << " (" << fps->get() << " FPS)" << std::endl;
        });
    }
};

/**
 * @brief Graphics plugin implementation
 */
class GraphicsPlugin : public IPlugin {
public:
    std::string getName() const override { return "GraphicsPlugin"; }
    std::string getVersion() const override { return "2.1.0"; }
    std::string getDescription() const override { return "Graphics rendering plugin"; }
    
    bool initialize() override {
        std::cout << "GraphicsPlugin initialized" << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "GraphicsPlugin shutdown" << std::endl;
    }
    
    std::shared_ptr<Component> createComponent(const std::string& name) override {
        return std::make_shared<GraphicsRendererComponent>(name);
    }
};

/**
 * @brief Network communication plugin component
 */
class NetworkComponent : public Component {
public:
    explicit NetworkComponent(const std::string& name) : Component(name) {
        std::cout << "NetworkComponent '" << name << "' created" << std::endl;
        
        // Initialize network variables
        addVariable<std::string>("host", "localhost");
        addVariable<int>("port", 8080);
        addVariable<bool>("connected", false);
        addVariable<std::string>("protocol", "TCP");
        
        // Add network commands
        def("connect", [this](const std::string& host, int port) {
            setValue("host", host);
            setValue("port", port);
            setValue("connected", true);
            std::cout << "  Connected to " << host << ":" << port << std::endl;
        });
        
        def("disconnect", [this]() {
            setValue("connected", false);
            std::cout << "  Disconnected from network" << std::endl;
        });
        
        def("sendData", [this](const std::string& data) {
            auto connected = getVariable<bool>("connected");
            if (connected && connected->get()) {
                std::cout << "  Sent data: " << data << std::endl;
            } else {
                std::cout << "  Cannot send data: not connected" << std::endl;
            }
        });
    }
};

/**
 * @brief Network plugin implementation
 */
class NetworkPlugin : public IPlugin {
public:
    std::string getName() const override { return "NetworkPlugin"; }
    std::string getVersion() const override { return "1.5.2"; }
    std::string getDescription() const override { return "Network communication plugin"; }
    
    bool initialize() override {
        std::cout << "NetworkPlugin initialized" << std::endl;
        return true;
    }
    
    void shutdown() override {
        std::cout << "NetworkPlugin shutdown" << std::endl;
    }
    
    std::shared_ptr<Component> createComponent(const std::string& name) override {
        return std::make_shared<NetworkComponent>(name);
    }
};

/**
 * @brief Plugin manager for loading and managing plugins
 */
class PluginManager {
private:
    std::unordered_map<std::string, std::unique_ptr<IPlugin>> plugins_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Component>>> pluginComponents_;
    
public:
    PluginManager() {
        std::cout << "PluginManager created" << std::endl;
    }
    
    ~PluginManager() {
        unloadAllPlugins();
    }
    
    bool loadPlugin(std::unique_ptr<IPlugin> plugin) {
        if (!plugin) {
            std::cerr << "Cannot load null plugin" << std::endl;
            return false;
        }
        
        std::string name = plugin->getName();
        
        if (plugins_.find(name) != plugins_.end()) {
            std::cout << "Plugin '" << name << "' already loaded" << std::endl;
            return false;
        }
        
        if (!plugin->initialize()) {
            std::cerr << "Failed to initialize plugin '" << name << "'" << std::endl;
            return false;
        }
        
        plugins_[name] = std::move(plugin);
        pluginComponents_[name] = std::vector<std::shared_ptr<Component>>();
        
        std::cout << "Plugin '" << name << "' loaded successfully" << std::endl;
        return true;
    }
    
    bool unloadPlugin(const std::string& name) {
        auto it = plugins_.find(name);
        if (it == plugins_.end()) {
            std::cout << "Plugin '" << name << "' not found" << std::endl;
            return false;
        }
        
        // Shutdown all components created by this plugin
        auto& components = pluginComponents_[name];
        for (auto& comp : components) {
            if (comp) {
                comp->setState(ComponentState::Destroyed);
            }
        }
        components.clear();
        
        // Shutdown the plugin
        it->second->shutdown();
        plugins_.erase(it);
        pluginComponents_.erase(name);
        
        std::cout << "Plugin '" << name << "' unloaded" << std::endl;
        return true;
    }
    
    void unloadAllPlugins() {
        std::cout << "Unloading all plugins..." << std::endl;
        auto pluginNames = getLoadedPlugins();
        for (const auto& name : pluginNames) {
            unloadPlugin(name);
        }
    }
    
    std::shared_ptr<Component> createComponent(const std::string& pluginName, 
                                             const std::string& componentName) {
        auto it = plugins_.find(pluginName);
        if (it == plugins_.end()) {
            std::cerr << "Plugin '" << pluginName << "' not found" << std::endl;
            return nullptr;
        }
        
        auto component = it->second->createComponent(componentName);
        if (component) {
            pluginComponents_[pluginName].push_back(component);
            std::cout << "Created component '" << componentName 
                      << "' from plugin '" << pluginName << "'" << std::endl;
        }
        
        return component;
    }
    
    std::vector<std::string> getLoadedPlugins() const {
        std::vector<std::string> names;
        for (const auto& pair : plugins_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void listPlugins() const {
        std::cout << "\n=== Loaded Plugins ===" << std::endl;
        for (const auto& pair : plugins_) {
            const auto& plugin = pair.second;
            std::cout << "Plugin: " << plugin->getName() 
                      << " v" << plugin->getVersion() << std::endl;
            std::cout << "  Description: " << plugin->getDescription() << std::endl;
            std::cout << "  Components: " << pluginComponents_.at(pair.first).size() << std::endl;
        }
    }
};

int main() {
    std::cout << "=== Atom Component Plugin Architecture Example ===" << std::endl;
    
    try {
        // Create plugin manager
        PluginManager pluginManager;
        
        std::cout << "\n1. Loading plugins..." << std::endl;
        
        // Load plugins
        pluginManager.loadPlugin(std::make_unique<AudioPlugin>());
        pluginManager.loadPlugin(std::make_unique<GraphicsPlugin>());
        pluginManager.loadPlugin(std::make_unique<NetworkPlugin>());
        
        // List loaded plugins
        pluginManager.listPlugins();
        
        std::cout << "\n2. Creating components from plugins..." << std::endl;
        
        // Create components from plugins
        auto audioComp = pluginManager.createComponent("AudioPlugin", "MainAudio");
        auto graphicsComp = pluginManager.createComponent("GraphicsPlugin", "MainRenderer");
        auto networkComp = pluginManager.createComponent("NetworkPlugin", "GameNetwork");
        
        std::cout << "\n3. Using plugin components..." << std::endl;
        
        // Use audio component
        if (audioComp) {
            audioComp->executeCommand("setVolume", {"0.8"});
            audioComp->executeCommand("applyEffect", {"reverb"});
            audioComp->executeCommand("process", {});
        }
        
        // Use graphics component
        if (graphicsComp) {
            graphicsComp->executeCommand("setResolution", {"2560", "1440"});
            graphicsComp->executeCommand("setRenderer", {"Vulkan"});
            graphicsComp->executeCommand("render", {});
        }
        
        // Use network component
        if (networkComp) {
            networkComp->executeCommand("connect", {"game.server.com", "9999"});
            networkComp->executeCommand("sendData", {"player_position:100,200,50"});
        }
        
        std::cout << "\n4. Plugin hot-swapping demonstration..." << std::endl;
        
        // Unload and reload a plugin
        pluginManager.unloadPlugin("AudioPlugin");
        pluginManager.loadPlugin(std::make_unique<AudioPlugin>());
        
        // Create new component from reloaded plugin
        auto newAudioComp = pluginManager.createComponent("AudioPlugin", "ReloadedAudio");
        if (newAudioComp) {
            newAudioComp->executeCommand("setVolume", {"1.2"});
            newAudioComp->executeCommand("applyEffect", {"echo"});
            newAudioComp->executeCommand("process", {});
        }
        
        pluginManager.listPlugins();
        
        std::cout << "\n=== Plugin Architecture Example Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
