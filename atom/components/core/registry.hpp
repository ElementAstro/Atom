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
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <stdexcept>
#include "../lifecycle/lifecycle.hpp"
#include "component.hpp"
#include "component_pool.hpp"

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
     * @brief Component performance statistics
     */
    struct ComponentStats {
        std::chrono::microseconds initTime{0};
        std::chrono::microseconds loadTime{0};
        uint64_t commandCount{0};
        uint64_t eventCount{0};
        uint64_t callCount{0};
    };

    /**
     * @brief Component metadata structure
     */
    struct ComponentInfo {
        std::string name;
        std::string version;
        std::string description;
        std::string author;
        std::string license;
        std::string configPath;
        std::chrono::system_clock::time_point loadTime;
        std::chrono::system_clock::time_point lastUsed;
        bool isInitialized{false};
        bool isEnabled{true};
        bool isAutoLoad{false};
        bool isLazyLoad{false};
        bool isHotReload{false};

        std::vector<std::string> dependencies;
        std::vector<std::string> conflicts;
        std::vector<std::string> optionalDeps;
        std::vector<std::string> provides;

        ComponentStats stats;
    };

    /**
     * @brief Component registration exception
     */
    class RegistryException : public std::runtime_error {
    public:
        explicit RegistryException(const std::string& message)
            : std::runtime_error(message) {}
    };

    /**
     * @brief Get the singleton instance of the registry
     * @return Registry& Singleton instance
     */
    static auto instance() -> Registry&;

    /**
     * @brief Register a module and its initialization function
     * @param name Module name
     * @param init_func Initialization function
     */
    void registerModule(const std::string& name, Component::InitFunc init_func);

    /**
     * @brief Add a component initializer
     * @param name Component name
     * @param init_func Initialization function
     * @param cleanup_func Cleanup function
     * @param metadata Component metadata
     */
    void addInitializer(const std::string& name, Component::InitFunc init_func,
                        Component::CleanupFunc cleanup_func = nullptr,
                        std::optional<ComponentInfo> metadata = std::nullopt);

    /**
     * @brief Register an externally created component instance
     * @param name Component name
     * @param instance Existing component instance (must not be null)
     * @param init_func Optional initialization function run by
     * initializeAll()/initializeComponent
     * @param cleanup_func Optional cleanup function run during cleanup
     * @throws RegistryException If the instance is null
     */
    void registerComponentInstance(
        const std::string& name, std::shared_ptr<Component> instance,
        Component::InitFunc init_func = nullptr,
        Component::CleanupFunc cleanup_func = nullptr);

    /**
     * @brief Add a component dependency
     * @param name The component that depends on another
     * @param dependency The component being depended on
     * @param isOptional Whether it is an optional dependency
     */
    void addDependency(const std::string& name, const std::string& dependency,
                       bool isOptional = false);

    /**
     * @brief Initialize all components
     * @param forceReload Whether to force reload
     */
    void initializeAll(bool forceReload = false);

    /**
     * @brief Clean up all component resources
     * @param force Whether to force cleanup
     */
    void cleanupAll(bool force = false);

    /**
     * @brief Check if a component is initialized
     * @param name Component name
     * @return bool Whether it is initialized
     */
    [[nodiscard]] bool isInitialized(const std::string& name) const;

    /**
     * @brief Check if a component is enabled
     * @param name Component name
     * @return bool Whether it is enabled
     */
    [[nodiscard]] bool isEnabled(const std::string& name) const;

    /**
     * @brief Enable or disable a component
     * @param name Component name
     * @param enable Whether to enable
     * @return bool Whether the operation was successful
     */
    bool enableComponent(const std::string& name, bool enable = true);

    /**
     * @brief Reinitialize a component
     * @param name Component name
     * @param reloadDependencies Whether to reload dependencies
     */
    void reinitializeComponent(const std::string& name,
                               bool reloadDependencies = false);

    /**
     * @brief Get a component instance
     * @param name Component name
     * @return std::shared_ptr<Component> Component instance
     * @throws RegistryException Thrown if the component does not exist
     */
    [[nodiscard]] auto getComponent(const std::string& name) const
        -> std::shared_ptr<Component>;

    /**
     * @brief Get or load a component (supports lazy loading)
     * @param name Component name
     * @return std::shared_ptr<Component> Component instance
     */
    auto getOrLoadComponent(const std::string& name)
        -> std::shared_ptr<Component>;

    /**
     * @brief Get all component instances
     * @return std::vector<std::shared_ptr<Component>> All component instances
     */
    [[nodiscard]] auto getAllComponents() const
        -> std::vector<std::shared_ptr<Component>>;

    /**
     * @brief Get all component names
     * @return std::vector<std::string> All component names
     */
    [[nodiscard]] auto getAllComponentNames() const -> std::vector<std::string>;

    /**
     * @brief Get component metadata
     * @param name Component name
     * @return const ComponentInfo& Component metadata
     * @throws RegistryException Thrown if the component does not exist
     */
    [[nodiscard]] auto getComponentInfo(const std::string& name) const
        -> const ComponentInfo&;

    /**
     * @brief Update component metadata
     * @param name Component name
     * @param info New metadata
     * @return bool Whether the update was successful
     */
    bool updateComponentInfo(const std::string& name,
                             const ComponentInfo& info);

    /**
     * @brief Load a component from the file system
     * @param path Component path
     * @return bool Whether loading was successful
     */
    bool loadComponentFromFile(const std::string& path);

    /**
     * @brief Monitor component file changes for hot reloading
     * @param enable Whether to enable monitoring
     * @return bool Whether the operation was successful
     */
    bool watchComponentChanges(bool enable = true);

    /**
     * @brief Remove a component
     * @param name Component name
     * @return bool Whether removal was successful
     */
    bool removeComponent(const std::string& name);

    /**
     * @brief Create a component using the memory pool system
     * @tparam T Component type
     * @tparam Args Constructor argument types
     * @param name Component name
     * @param args Constructor arguments
     * @return Shared pointer to the created component
     */
    template <typename T, typename... Args>
    std::shared_ptr<T> createComponent(const std::string& name, Args&&... args);

    /**
     * @brief Configure memory pool for a component type
     * @tparam T Component type
     * @param config Pool configuration
     */
    template <typename T>
    void configureComponentPool(const atom::components::PoolConfig& config);

    /**
     * @brief Get memory pool statistics for a component type
     * @tparam T Component type
     * @return Pool statistics
     */
    template <typename T>
    const atom::components::PoolStatistics& getPoolStatistics() const;

    /**
     * @brief Get total memory usage of all component pools
     * @return Total memory usage in bytes
     */
    size_t getTotalPoolMemoryUsage() const;

    /**
     * @brief Perform cleanup on all component pools
     */
    void cleanupComponentPools();

    /**
     * @brief Register a lifecycle hook for a component
     * @param componentName Component name
     * @param phase Lifecycle phase
     * @param hook Hook function
     */
    void registerLifecycleHook(const std::string& componentName,
                               atom::components::LifecyclePhase phase,
                               atom::components::LifecycleHook hook);

    /**
     * @brief Add a dependency constraint for a component
     * @param componentName Component name
     * @param constraint Dependency constraint
     */
    void addDependencyConstraint(
        const std::string& componentName,
        const atom::components::DependencyConstraint& constraint);

    /**
     * @brief Get dependency resolution order for a component
     * @param componentName Component name
     * @return Vector of component names in dependency order
     */
    [[nodiscard]] std::vector<std::string> getDependencyOrder(
        const std::string& componentName) const;

    /**
     * @brief Check for circular dependencies
     * @param componentName Component name
     * @return True if circular dependencies exist
     */
    [[nodiscard]] bool hasCircularDependencies(
        const std::string& componentName) const;

    /**
     * @brief Get lifecycle events for a component
     * @param componentName Component name (empty for all)
     * @return Vector of lifecycle events
     */
    [[nodiscard]] std::vector<atom::components::LifecycleEvent>
    getLifecycleEvents(const std::string& componentName = "") const;

#if ENABLE_EVENT_SYSTEM
    /**
     * @brief Subscribe to a component event
     * @param eventName Event name
     * @param callback Callback function
     * @return EventCallbackId Event callback ID
     */
    atom::components::EventCallbackId subscribeToEvent(
        const std::string& eventName, atom::components::EventCallback callback);

    /**
     * @brief Unsubscribe from a component event
     * @param eventName Event name
     * @param callbackId Event callback ID
     * @return bool Whether the operation was successful
     */
    bool unsubscribeFromEvent(const std::string& eventName,
                              atom::components::EventCallbackId callbackId);

    /**
     * @brief Trigger a component event
     * @param event Event object
     */
    void triggerEvent(const atom::components::Event& event);
#endif

private:
    Registry() = default;
    ~Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    bool hasCircularDependency(const std::string& name,
                               const std::string& dependency);

    void initializeComponent(const std::string& name,
                             std::unordered_set<std::string>& init_stack);

    void determineInitializationOrder();

    std::tuple<bool, std::vector<std::string>> checkDependenciesSatisfied(
        const std::string& name);

    std::tuple<bool, std::vector<std::string>> checkConflicts(
        const std::string& name);

    mutable std::shared_mutex mutex_;

#if ENABLE_FASTHASH
    emhash::HashMap<std::string, std::shared_ptr<Component>> initializers_;
    emhash::HashMap<std::string, ComponentInfo> componentInfos_;
    emhash::HashMap<std::string, Component::InitFunc> module_initializers_;
    emhash::HashMap<std::string, emhash::HashSet<std::string>> dependencies_;
    emhash::HashMap<std::string, emhash::HashSet<std::string>>
        optionalDependencies_;
#else
    std::unordered_map<std::string, std::shared_ptr<Component>> initializers_;
    std::unordered_map<std::string, ComponentInfo> componentInfos_;
    std::unordered_map<std::string, Component::InitFunc> module_initializers_;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        dependencies_;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        optionalDependencies_;
#endif

    std::vector<std::string> initializationOrder_;

#if ENABLE_EVENT_SYSTEM
    struct EventSubscription {
        atom::components::EventCallbackId id;
        atom::components::EventCallback callback;
    };

    // Events use their own lock so triggerEvent can be called from code
    // paths that already hold mutex_ (e.g. initializeAll/cleanupAll).
    mutable std::shared_mutex eventMutex_;
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

#define REGISTER_COMPONENT(name, func) \
    static bool registered_##name =    \
        (Registry::instance().registerModule(#name, func), true)

#define DECLARE_COMPONENT_DEPENDENCIES(name, ...)          \
    std::vector<std::string> name::getNeededComponents() { \
        return {__VA_ARGS__};                              \
    }

// Template method implementations
template <typename T, typename... Args>
std::shared_ptr<T> Registry::createComponent(const std::string& name,
                                             Args&&... args) {
    std::unique_lock lock(mutex_);

    // Create the component instance
    auto component = std::make_shared<T>(name, std::forward<Args>(args)...);

    // Store it in the initializers map
    initializers_[name] = component;

    // Create component info if it doesn't exist
    if (!componentInfos_.contains(name)) {
        ComponentInfo info;
        info.name = name;
        info.loadTime = std::chrono::system_clock::now();
        info.isInitialized = true;
        componentInfos_[name] = std::move(info);
    } else {
        componentInfos_[name].isInitialized = true;
        componentInfos_[name].loadTime = std::chrono::system_clock::now();
    }

    return component;
}

template <typename T>
void Registry::configureComponentPool(
    const atom::components::PoolConfig& config) {
    // Implementation for component pool configuration
    // This is a placeholder - actual implementation would depend on the pool
    // system
    (void)config;  // Suppress unused parameter warning
}

#endif  // ATOM_COMPONENT_REGISTRY_HPP
