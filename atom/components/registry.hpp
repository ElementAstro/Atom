/*
 * registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-10

Description: Component Registry for Managing Component Lifecycle

**************************************************/

#ifndef ATOM_COMPONENT_REGISTRY_HPP
#define ATOM_COMPONENT_REGISTRY_HPP

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "component.hpp"

#include "atom/log/loguru.hpp"

class Component;

/**
 * @brief Registry for managing component lifecycle.
 *
 * The Registry is responsible for managing the lifecycle of components,
 * including initialization, dependency resolution, and cleanup.
 */
class Registry {
public:
    /**
     * @brief Component metadata structure
     */
    struct ComponentInfo {
        std::string name;         // Component name
        std::string version;      // Component version
        std::string description;  // Component description
        std::string author;       // Component author
        std::string license;      // Component license
        std::string configPath;   // Component configuration file path
        std::chrono::system_clock::time_point loadTime;  // Load time
        std::chrono::system_clock::time_point lastUsed;  // Last used time
        bool isInitialized{false};  // Whether it has been initialized
        bool isEnabled{true};       // Whether it is enabled
        bool isAutoLoad{false};     // Whether to autoload
        bool isLazyLoad{false};     // Whether to lazy load
        bool isHotReload{false};    // Whether hot reload is supported

        std::vector<std::string> dependencies;  // Dependent components
        std::vector<std::string> conflicts;     // Conflicting components
        std::vector<std::string> optionalDeps;  // Optional dependencies
        std::vector<std::string> provides;      // Provided features

        // Performance statistics
        struct {
            std::chrono::microseconds initTime{0};  // Initialization time
            std::chrono::microseconds loadTime{0};  // Load time
            uint64_t commandCount{0};               // Number of commands
            uint64_t eventCount{0};                 // Number of events
            uint64_t callCount{0};                  // Number of calls
        } stats;
    };

    /**
     * @brief Component registration exception
     */
    class RegistryException : public atom::error::Exception {
    public:
        using atom::error::Exception::Exception;
    };

    /**
     * @brief Get the singleton instance of the registry
     */
    static auto instance() -> Registry&;

    /**
     * @brief Register a module and its initialization function
     *
     * @param name Module name
     * @param init_func Initialization function
     */
    void registerModule(const std::string& name, Component::InitFunc init_func);

    /**
     * @brief Add a component initializer
     *
     * @param name Component name
     * @param init_func Initialization function
     * @param cleanup_func Cleanup function
     * @param metadata Component metadata
     */
    void addInitializer(const std::string& name, Component::InitFunc init_func,
                        Component::CleanupFunc cleanup_func = nullptr,
                        std::optional<ComponentInfo> metadata = std::nullopt);

    /**
     * @brief Add a component dependency
     *
     * @param name The component that depends on another
     * @param dependency The component being depended on
     * @param isOptional Whether it is an optional dependency
     */
    void addDependency(const std::string& name, const std::string& dependency,
                       bool isOptional = false);

    /**
     * @brief Initialize all components
     *
     * @param forceReload Whether to force reload
     */
    void initializeAll(bool forceReload = false);

    /**
     * @brief Clean up all component resources
     *
     * @param force Whether to force cleanup
     */
    void cleanupAll(bool force = false);

    /**
     * @brief Check if a component is initialized
     *
     * @param name Component name
     * @return bool Whether it is initialized
     */
    [[nodiscard]] auto isInitialized(const std::string& name) const -> bool;

    /**
     * @brief Check if a component is enabled
     *
     * @param name Component name
     * @return bool Whether it is enabled
     */
    [[nodiscard]] auto isEnabled(const std::string& name) const -> bool;

    /**
     * @brief Enable a component
     *
     * @param name Component name
     * @param enable Whether to enable
     * @return bool Whether the operation was successful
     */
    bool enableComponent(const std::string& name, bool enable = true);

    /**
     * @brief Reinitialize a component
     *
     * @param name Component name
     * @param reloadDependencies Whether to reload dependencies
     */
    void reinitializeComponent(const std::string& name,
                               bool reloadDependencies = false);

    /**
     * @brief Get a component instance
     *
     * @param name Component name
     * @return std::shared_ptr<Component> Component instance
     * @throws ObjectNotExist Thrown if the component does not exist
     */
    [[nodiscard]] auto getComponent(const std::string& name) const
        -> std::shared_ptr<Component>;

    /**
     * @brief Get or load a component (supports lazy loading)
     *
     * @param name Component name
     * @return std::shared_ptr<Component> Component instance
     */
    auto getOrLoadComponent(const std::string& name)
        -> std::shared_ptr<Component>;

    /**
     * @brief Get all component instances
     *
     * @return std::vector<std::shared_ptr<Component>> All component instances
     */
    [[nodiscard]] auto getAllComponents() const
        -> std::vector<std::shared_ptr<Component>>;

    /**
     * @brief Get all component names
     *
     * @return std::vector<std::string> All component names
     */
    [[nodiscard]] auto getAllComponentNames() const -> std::vector<std::string>;

    /**
     * @brief Get component metadata
     *
     * @param name Component name
     * @return const ComponentInfo& Component metadata
     * @throws ObjectNotExist Thrown if the component does not exist
     */
    [[nodiscard]] auto getComponentInfo(const std::string& name) const
        -> const ComponentInfo&;

    /**
     * @brief Update component metadata
     *
     * @param name Component name
     * @param info New metadata
     * @return bool Whether the update was successful
     */
    bool updateComponentInfo(const std::string& name,
                             const ComponentInfo& info);

    /**
     * @brief Load a component from the file system
     *
     * @param path Component path
     * @return bool Whether loading was successful
     */
    bool loadComponentFromFile(const std::string& path);

    /**
     * @brief Monitor component file changes for hot reloading
     *
     * @param enable Whether to enable monitoring
     * @return bool Whether the operation was successful
     */
    bool watchComponentChanges(bool enable = true);

    /**
     * @brief Unload a component
     *
     * @param name Component name
     * @return bool Whether unloading was successful
     */
    bool removeComponent(const std::string& name);

#if ENABLE_EVENT_SYSTEM
    /**
     * @brief Subscribe to a component event
     *
     * @param eventName Event name
     * @param callback Callback function
     * @return EventCallbackId Event callback ID
     */
    atom::components::EventCallbackId subscribeToEvent(
        const std::string& eventName, atom::components::EventCallback callback);

    /**
     * @brief Unsubscribe from a component event
     *
     * @param eventName Event name
     * @param callbackId Event callback ID
     * @return bool Whether the operation was successful
     */
    bool unsubscribeFromEvent(const std::string& eventName,
                              atom::components::EventCallbackId callbackId);

    /**
     * @brief Trigger a component event
     *
     * @param event Event object
     */
    void triggerEvent(const atom::components::Event& event);
#endif

private:
    Registry() = default;
    ~Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /**
     * @brief Check for circular dependencies
     *
     * @param name Component name
     * @param dependency Name of the dependent component
     * @return bool Whether a circular dependency exists
     */
    bool hasCircularDependency(const std::string& name,
                               const std::string& dependency);

    /**
     * @brief Initialize a component
     *
     * @param name Component name
     * @param init_stack Initialization stack
     */
    void initializeComponent(const std::string& name,
                             std::unordered_set<std::string>& init_stack);

    /**
     * @brief Determine the component initialization order
     */
    void determineInitializationOrder();

    /**
     * @brief Check if component dependencies are satisfied
     *
     * @param name Component name
     * @return std::tuple<bool, std::vector<std::string>> Whether satisfied and
     * list of unsatisfied dependencies
     */
    std::tuple<bool, std::vector<std::string>> checkDependenciesSatisfied(
        const std::string& name);

    /**
     * @brief Check for component conflicts
     *
     * @param name Component name
     * @return std::tuple<bool, std::vector<std::string>> Whether there is a
     * conflict and list of conflicting components
     */
    std::tuple<bool, std::vector<std::string>> checkConflicts(
        const std::string& name);

    mutable std::shared_mutex mutex_;  // Mutex for thread safety

// Use container type selected by configuration
#if ENABLE_FASTHASH
    emhash::HashMap<std::string, std::shared_ptr<Component>>
        initializers_;  // Component instance map
    emhash::HashMap<std::string, ComponentInfo>
        componentInfos_;  // Component metadata
    emhash::HashMap<std::string, Component::InitFunc>
        module_initializers_;  // Module initialization functions
    emhash::HashMap<std::string, emhash::HashSet<std::string>>
        dependencies_;  // Component dependencies
    emhash::HashMap<std::string, emhash::HashSet<std::string>>
        optionalDependencies_;  // Optional dependencies
#else
    std::unordered_map<std::string, std::shared_ptr<Component>>
        initializers_;  // Component instance map
    std::unordered_map<std::string, ComponentInfo>
        componentInfos_;  // Component metadata
    std::unordered_map<std::string, Component::InitFunc>
        module_initializers_;  // Module initialization functions
    std::unordered_map<std::string, std::unordered_set<std::string>>
        dependencies_;  // Component dependencies
    std::unordered_map<std::string, std::unordered_set<std::string>>
        optionalDependencies_;  // Optional dependencies
#endif

    std::vector<std::string>
        initializationOrder_;  // Component initialization order

#if ENABLE_EVENT_SYSTEM
    struct EventSubscription {
        atom::components::EventCallbackId id;
        atom::components::EventCallback callback;
    };

    std::unordered_map<std::string, std::vector<EventSubscription>>
        eventSubscriptions_;
    std::atomic<atom::components::EventCallbackId> nextEventId_{1};
#endif

#if ENABLE_HOT_RELOAD
    std::unordered_map<std::string, std::filesystem::file_time_type>
        componentFileTimestamps_;
    std::atomic<bool> watchingForChanges_{false};
    std::future<void> fileWatcherFuture_;
#endif
};

// Macro for convenient component registration
#define REGISTER_COMPONENT(name, func) \
    static bool registered_##name =    \
        (Registry::instance().registerModule(#name, func), true)

// Helper macro to define component dependencies
#define DECLARE_COMPONENT_DEPENDENCIES(name, ...)          \
    std::vector<std::string> name::getNeededComponents() { \
        return {__VA_ARGS__};                              \
    }

#endif  // ATOM_COMPONENT_REGISTRY_HPP
