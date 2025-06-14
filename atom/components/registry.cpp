/*
 * registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-10

Description: Registry Pattern Implementation

**************************************************/

#include "registry.hpp"

#include <algorithm>
#include <chrono>

#include "atom/utils/to_string.hpp"
#include "spdlog/spdlog.h"

#define THROW_REGISTRY_EXCEPTION(...)                                 \
    throw Registry::RegistryException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

auto Registry::instance() -> Registry& {
    static Registry instance;
    return instance;
}

void Registry::registerModule(const std::string& name,
                              Component::InitFunc init_func) {
    std::scoped_lock lock(mutex_);
    spdlog::info("Registering module: {}", name);
    module_initializers_[name] = std::move(init_func);

    if (!componentInfos_.contains(name)) {
        ComponentInfo info;
        info.name = name;
        info.loadTime = std::chrono::system_clock::now();
        componentInfos_[name] = std::move(info);
    }
}

void Registry::addInitializer(const std::string& name,
                              Component::InitFunc init_func,
                              Component::CleanupFunc cleanup_func,
                              std::optional<ComponentInfo> metadata) {
    std::scoped_lock lock(mutex_);
    if (initializers_.contains(name)) {
        spdlog::warn("Component '{}' already registered, skipping", name);
        return;
    }

    spdlog::info("Adding initializer for component: {}", name);

    initializers_[name] = std::make_shared<Component>(name);
    initializers_[name]->initFunc = std::move(init_func);
    initializers_[name]->cleanupFunc = std::move(cleanup_func);

    if (metadata.has_value()) {
        componentInfos_[name] = std::move(*metadata);
        componentInfos_[name].loadTime = std::chrono::system_clock::now();
    } else if (!componentInfos_.contains(name)) {
        ComponentInfo info;
        info.name = name;
        info.loadTime = std::chrono::system_clock::now();
        componentInfos_[name] = std::move(info);
    }

    componentInfos_[name].isInitialized = false;
}

void Registry::addDependency(const std::string& name,
                             const std::string& dependency, bool isOptional) {
    std::unique_lock lock(mutex_);

    if (name == dependency) {
        spdlog::error("Component '{}' cannot depend on itself", name);
        THROW_REGISTRY_EXCEPTION("Component '{}' cannot depend on itself",
                                 name);
    }

    if (hasCircularDependency(name, dependency)) {
        spdlog::error("Circular dependency detected: {} -> {}", name,
                      dependency);
        THROW_REGISTRY_EXCEPTION("Circular dependency detected: {} -> {}", name,
                                 dependency);
    }

    spdlog::info("Adding {} dependency: {} -> {}",
                 isOptional ? "optional" : "required", name, dependency);

    if (isOptional) {
        optionalDependencies_[name].insert(dependency);

        if (componentInfos_.contains(name)) {
            auto& deps = componentInfos_[name].optionalDeps;
            if (std::find(deps.begin(), deps.end(), dependency) == deps.end()) {
                deps.push_back(dependency);
            }
        }
    } else {
        dependencies_[name].insert(dependency);

        if (componentInfos_.contains(name)) {
            auto& deps = componentInfos_[name].dependencies;
            if (std::find(deps.begin(), deps.end(), dependency) == deps.end()) {
                deps.push_back(dependency);
            }
        }
    }
}

void Registry::initializeAll(bool forceReload) {
    std::unique_lock lock(mutex_);
    spdlog::info("Initializing all components");

    if (forceReload) {
        spdlog::info("Force reloading all components");
        for (auto& [name, info] : componentInfos_) {
            info.isInitialized = false;
        }
    }

    determineInitializationOrder();

    for (const auto& name : initializationOrder_) {
        std::unordered_set<std::string> initStack;
        spdlog::info("Initializing component: {}", name);

        auto startTime = std::chrono::high_resolution_clock::now();
        initializeComponent(name, initStack);
        auto endTime = std::chrono::high_resolution_clock::now();

        if (componentInfos_.contains(name)) {
            componentInfos_[name].stats.initTime =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime);
        }
    }

    spdlog::info("All components initialized successfully");
}

void Registry::cleanupAll(bool force) {
    std::unique_lock lock(mutex_);
    spdlog::info("Cleaning up all components");

    for (const auto& name : std::ranges::reverse_view(initializationOrder_)) {
        if (!componentInfos_.contains(name) ||
            !componentInfos_[name].isInitialized) {
            continue;
        }

        auto component = initializers_[name];
        if (!component || !component->cleanupFunc) {
            continue;
        }

        try {
            spdlog::info("Cleaning up component: {}", name);
            component->cleanupFunc();
            componentInfos_[name].isInitialized = false;

#if ENABLE_EVENT_SYSTEM
            atom::components::Event event;
            event.name = "component.unloaded";
            event.source = name;
            event.timestamp = std::chrono::steady_clock::now();
            triggerEvent(event);
#endif

        } catch (const std::exception& e) {
            spdlog::error("Error cleaning up component {}: {}", name, e.what());

            if (force) {
                spdlog::warn("Forcing cleanup to continue despite error");
            } else {
                throw;
            }
        }
    }

    if (force) {
        spdlog::info("Force clearing all component resources");
        initializers_.clear();
        for (auto& [name, info] : componentInfos_) {
            info.isInitialized = false;
        }
    }

    spdlog::info("All components cleaned up successfully");
}

bool Registry::isInitialized(const std::string& name) const {
    std::shared_lock lock(mutex_);

    if (!componentInfos_.contains(name)) {
        return false;
    }

    return componentInfos_.at(name).isInitialized;
}

bool Registry::isEnabled(const std::string& name) const {
    std::shared_lock lock(mutex_);

    if (!componentInfos_.contains(name)) {
        return false;
    }

    return componentInfos_.at(name).isEnabled;
}

bool Registry::enableComponent(const std::string& name, bool enable) {
    std::unique_lock lock(mutex_);

    if (!componentInfos_.contains(name)) {
        spdlog::error("Cannot enable/disable non-existent component: {}", name);
        return false;
    }

    componentInfos_[name].isEnabled = enable;
    spdlog::info("{} component: {}", enable ? "Enabled" : "Disabled", name);

#if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = enable ? "component.enabled" : "component.disabled";
    event.source = name;
    event.timestamp = std::chrono::steady_clock::now();
    lock.unlock();
    triggerEvent(event);
#endif

    return true;
}

void Registry::reinitializeComponent(const std::string& name,
                                     bool reloadDependencies) {
    std::unique_lock lock(mutex_);
    spdlog::info("Reinitializing component: {}", name);

    if (!initializers_.contains(name)) {
        spdlog::error("Cannot reinitialize non-existent component: {}", name);
        THROW_REGISTRY_EXCEPTION("Component not registered: {}", name);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    if (componentInfos_[name].isInitialized &&
        initializers_[name]->cleanupFunc) {
        try {
            initializers_[name]->cleanupFunc();
        } catch (const std::exception& e) {
            spdlog::error("Error during cleanup of {}: {}", name, e.what());
        }
        componentInfos_[name].isInitialized = false;
    }

    if (reloadDependencies) {
        auto deps = dependencies_[name];
        for (const auto& dep : deps) {
            if (initializers_.contains(dep)) {
                reinitializeComponent(dep, false);
            }
        }
    }

    auto it = module_initializers_.find(name);
    if (it != module_initializers_.end()) {
        auto component = std::make_shared<Component>(name);
        it->second(*component);
        initializers_[name] = std::move(component);

        componentInfos_[name].isInitialized = true;
        componentInfos_[name].lastUsed = std::chrono::system_clock::now();

        auto endTime = std::chrono::high_resolution_clock::now();
        componentInfos_[name].stats.initTime =
            std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                                  startTime);

#if ENABLE_EVENT_SYSTEM
        atom::components::Event event;
        event.name = "component.reloaded";
        event.source = name;
        event.timestamp = std::chrono::steady_clock::now();
        lock.unlock();
        triggerEvent(event);
#endif
    } else {
        spdlog::error("No initializer function found for component: {}", name);
        THROW_REGISTRY_EXCEPTION("No initializer function for component: {}",
                                 name);
    }
}

auto Registry::getComponent(const std::string& name) const
    -> std::shared_ptr<Component> {
    std::shared_lock lock(mutex_);

    if (!initializers_.contains(name)) {
        spdlog::error("Component not registered: {}", name);
        THROW_REGISTRY_EXCEPTION("Component not registered: {}", name);
    }

    if (componentInfos_.contains(name)) {
        auto& info = const_cast<ComponentInfo&>(componentInfos_.at(name));
        info.lastUsed = std::chrono::system_clock::now();
    }

    return initializers_.at(name);
}

auto Registry::getOrLoadComponent(const std::string& name)
    -> std::shared_ptr<Component> {
    {
        std::shared_lock readLock(mutex_);

        if (initializers_.contains(name) && componentInfos_.contains(name) &&
            componentInfos_[name].isInitialized) {
            componentInfos_[name].lastUsed = std::chrono::system_clock::now();
            return initializers_[name];
        }
    }

    std::unique_lock writeLock(mutex_);

    if (initializers_.contains(name) && componentInfos_.contains(name) &&
        componentInfos_[name].isInitialized) {
        componentInfos_[name].lastUsed = std::chrono::system_clock::now();
        return initializers_[name];
    }

    spdlog::info("Lazy loading component: {}", name);

    if (!module_initializers_.contains(name)) {
        spdlog::error("Cannot lazy load unregistered component: {}", name);
        THROW_REGISTRY_EXCEPTION("Component not registered: {}", name);
    }

    auto [satisfied, missingDeps] = checkDependenciesSatisfied(name);
    if (!satisfied) {
        spdlog::error(
            "Cannot load component {} due to missing dependencies: {}", name,
            atom::utils::toString(missingDeps));
        THROW_REGISTRY_EXCEPTION(
            "Cannot load component {} due to missing dependencies", name);
    }

    std::unordered_set<std::string> initStack;
    auto startTime = std::chrono::high_resolution_clock::now();
    initializeComponent(name, initStack);
    auto endTime = std::chrono::high_resolution_clock::now();

    componentInfos_[name].stats.initTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

#if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = "component.loaded";
    event.source = name;
    event.timestamp = std::chrono::steady_clock::now();
    writeLock.unlock();
    triggerEvent(event);
#endif

    return initializers_[name];
}

auto Registry::getAllComponents() const
    -> std::vector<std::shared_ptr<Component>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<Component>> components;
    components.reserve(initializers_.size());

    for (const auto& [name, component] : initializers_) {
        if (component && componentInfos_.contains(name) &&
            componentInfos_.at(name).isEnabled) {
            components.push_back(component);
        }
    }

    return components;
}

auto Registry::getAllComponentNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(componentInfos_.size());

    for (const auto& [name, info] : componentInfos_) {
        if (info.isEnabled) {
            names.push_back(name);
        }
    }

    return names;
}

auto Registry::getComponentInfo(const std::string& name) const
    -> const ComponentInfo& {
    std::shared_lock lock(mutex_);

    if (!componentInfos_.contains(name)) {
        spdlog::error("Component info not found: {}", name);
        THROW_REGISTRY_EXCEPTION("Component info not found: {}", name);
    }

    return componentInfos_.at(name);
}

bool Registry::updateComponentInfo(const std::string& name,
                                   const ComponentInfo& info) {
    std::unique_lock lock(mutex_);

    if (!componentInfos_.contains(name)) {
        spdlog::error("Cannot update info for non-existent component: {}",
                      name);
        return false;
    }

    ComponentInfo newInfo = info;
    newInfo.name = name;
    newInfo.loadTime = componentInfos_[name].loadTime;
    newInfo.isInitialized = componentInfos_[name].isInitialized;

    componentInfos_[name] = std::move(newInfo);
    spdlog::info("Updated component info for: {}", name);

    return true;
}

bool Registry::loadComponentFromFile(const std::string& path) {
#if ENABLE_HOT_RELOAD
    namespace fs = std::filesystem;

    if (!fs::exists(path)) {
        spdlog::error("Component file not found: {}", path);
        return false;
    }

    std::string name = fs::path(path).stem().string();
    spdlog::info("Loading component from file: {} (name: {})", path, name);

    componentFileTimestamps_[name] = fs::last_write_time(path);

    spdlog::warn("Dynamic library loading not implemented yet");
    return true;
#else
    spdlog::error("Hot reload not enabled, cannot load component from file");
    return false;
#endif
}

bool Registry::watchComponentChanges(bool enable) {
#if ENABLE_HOT_RELOAD
    std::unique_lock lock(mutex_);

    if (enable == watchingForChanges_) {
        return true;
    }

    if (enable) {
        spdlog::info("Starting component file watcher");
        watchingForChanges_ = true;

        fileWatcherFuture_ = std::async(std::launch::async, [this]() {
            namespace fs = std::filesystem;

            while (watchingForChanges_) {
                {
                    std::shared_lock lock(mutex_);

                    for (auto& [name, lastTime] : componentFileTimestamps_) {
                        if (!componentInfos_.contains(name) ||
                            !componentInfos_.at(name).isHotReload) {
                            continue;
                        }

                        try {
                            const std::string& path =
                                componentInfos_.at(name).configPath;
                            if (path.empty() || !fs::exists(path)) {
                                continue;
                            }

                            auto currentTime = fs::last_write_time(path);
                            if (currentTime != lastTime) {
                                spdlog::info(
                                    "Detected change in component file: {}",
                                    path);
                                lastTime = currentTime;

                                lock.unlock();
                                reinitializeComponent(name, false);
                                lock = std::shared_lock(mutex_);
                            }
                        } catch (const std::exception& e) {
                            spdlog::error("Error checking component file: {}",
                                          e.what());
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
    } else {
        spdlog::info("Stopping component file watcher");
        watchingForChanges_ = false;

        if (fileWatcherFuture_.valid()) {
            fileWatcherFuture_.wait();
        }
    }

    return true;
#else
    spdlog::error("Hot reload not enabled, cannot watch component changes");
    return false;
#endif
}

bool Registry::removeComponent(const std::string& name) {
    std::unique_lock lock(mutex_);

    if (!initializers_.contains(name)) {
        spdlog::warn("Cannot remove non-existent component: {}", name);
        return false;
    }

    std::vector<std::string> dependents;
    for (const auto& [compName, deps] : dependencies_) {
        if (deps.contains(name) && compName != name) {
            dependents.push_back(compName);
        }
    }

    if (!dependents.empty()) {
        spdlog::error(
            "Cannot remove component {} because it is depended upon by: {}",
            name, atom::utils::toString(dependents));
        return false;
    }

    if (componentInfos_[name].isInitialized &&
        initializers_[name]->cleanupFunc) {
        try {
            initializers_[name]->cleanupFunc();
        } catch (const std::exception& e) {
            spdlog::error("Error during cleanup of {}: {}", name, e.what());
        }
    }

    initializers_.erase(name);
    module_initializers_.erase(name);
    dependencies_.erase(name);
    optionalDependencies_.erase(name);
    componentInfos_.erase(name);

    auto it = std::find(initializationOrder_.begin(),
                        initializationOrder_.end(), name);
    if (it != initializationOrder_.end()) {
        initializationOrder_.erase(it);
    }

#if ENABLE_HOT_RELOAD
    componentFileTimestamps_.erase(name);
#endif

    spdlog::info("Component removed: {}", name);

#if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = "component.removed";
    event.source = name;
    event.timestamp = std::chrono::steady_clock::now();
    lock.unlock();
    triggerEvent(event);
#endif

    return true;
}

#if ENABLE_EVENT_SYSTEM
atom::components::EventCallbackId Registry::subscribeToEvent(
    const std::string& eventName, atom::components::EventCallback callback) {
    std::unique_lock lock(mutex_);

    EventSubscription sub;
    sub.id = nextEventId_++;
    sub.callback = std::move(callback);

    eventSubscriptions_[eventName].push_back(std::move(sub));

    spdlog::info("Subscribed to event '{}' with ID {}", eventName, sub.id);
    return sub.id;
}

bool Registry::unsubscribeFromEvent(
    const std::string& eventName,
    atom::components::EventCallbackId callbackId) {
    std::unique_lock lock(mutex_);

    auto it = eventSubscriptions_.find(eventName);
    if (it == eventSubscriptions_.end()) {
        spdlog::warn("No subscriptions found for event: {}", eventName);
        return false;
    }

    auto& subs = it->second;
    auto subIt = std::find_if(subs.begin(), subs.end(),
                              [callbackId](const EventSubscription& sub) {
                                  return sub.id == callbackId;
                              });

    if (subIt == subs.end()) {
        spdlog::warn("Subscription ID {} not found for event {}", callbackId,
                     eventName);
        return false;
    }

    subs.erase(subIt);
    spdlog::info("Unsubscribed from event '{}' with ID {}", eventName,
                 callbackId);

    if (subs.empty()) {
        eventSubscriptions_.erase(it);
    }

    return true;
}

void Registry::triggerEvent(const atom::components::Event& event) {
    std::vector<atom::components::EventCallback> callbacks;

    {
        std::shared_lock lock(mutex_);
        auto it = eventSubscriptions_.find(event.name);
        if (it != eventSubscriptions_.end()) {
            callbacks.reserve(it->second.size());
            for (const auto& sub : it->second) {
                callbacks.push_back(sub.callback);
            }
        }
    }

    for (const auto& callback : callbacks) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            spdlog::error("Error in event callback for {}: {}", event.name,
                          e.what());
        }
    }

    spdlog::info("Triggered event '{}' from source '{}'", event.name,
                 event.source);
}
#endif

bool Registry::hasCircularDependency(const std::string& name,
                                     const std::string& dependency) {
    if (dependencies_[dependency].contains(name)) {
        return true;
    }

    for (const auto& dep : dependencies_[dependency]) {
        if (hasCircularDependency(name, dep)) {
            return true;
        }
    }

    return false;
}

void Registry::initializeComponent(
    const std::string& name, std::unordered_set<std::string>& init_stack) {
    if (componentInfos_.contains(name) && componentInfos_[name].isInitialized) {
        if (init_stack.contains(name)) {
            THROW_REGISTRY_EXCEPTION(
                "Circular dependency detected while initializing component "
                "'{}'",
                name);
        }
        return;
    }

    if (componentInfos_.contains(name) && !componentInfos_[name].isEnabled) {
        spdlog::info("Skipping disabled component: {}", name);
        return;
    }

    if (init_stack.contains(name)) {
        THROW_REGISTRY_EXCEPTION(
            "Circular dependency detected while initializing: {}", name);
    }

    init_stack.insert(name);

    for (const auto& dep : dependencies_[name]) {
        initializeComponent(dep, init_stack);
    }

    for (const auto& dep : optionalDependencies_[name]) {
        if (module_initializers_.contains(dep)) {
            try {
                initializeComponent(dep, init_stack);
            } catch (const std::exception& e) {
                spdlog::warn(
                    "Failed to initialize optional dependency {} for {}: {}",
                    dep, name, e.what());
            }
        }
    }

    if (auto it = module_initializers_.find(name);
        it != module_initializers_.end()) {
        if (!initializers_.contains(name)) {
            initializers_[name] = std::make_shared<Component>(name);
        }

        spdlog::info("Running initializer for component: {}", name);
        try {
            auto startTime = std::chrono::high_resolution_clock::now();
            it->second(*initializers_[name]);
            auto endTime = std::chrono::high_resolution_clock::now();

            if (componentInfos_.contains(name)) {
                componentInfos_[name].stats.loadTime =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        endTime - startTime);
            }

            if (initializers_[name]->initialize()) {
                spdlog::info("Component initialized successfully: {}", name);
                componentInfos_[name].isInitialized = true;
                componentInfos_[name].lastUsed =
                    std::chrono::system_clock::now();
            } else {
                spdlog::error("Component initialization returned false: {}",
                              name);
                THROW_REGISTRY_EXCEPTION("Component initialization failed: {}",
                                         name);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error initializing component {}: {}", name,
                          e.what());
            throw;
        }
    } else {
        spdlog::error("No initializer function found for component: {}", name);
        THROW_REGISTRY_EXCEPTION("No initializer function for component: {}",
                                 name);
    }

    init_stack.erase(name);
}

void Registry::determineInitializationOrder() {
    initializationOrder_.clear();
    std::unordered_set<std::string> visited;

    std::function<void(const std::string&)> visit =
        [&](const std::string& name) {
            if (visited.contains(name)) {
                return;
            }

            visited.insert(name);

            for (const auto& dep : dependencies_[name]) {
                if (module_initializers_.contains(dep)) {
                    visit(dep);
                } else {
                    spdlog::warn("Dependency '{}' not found for component '{}'",
                                 dep, name);
                }
            }

            for (const auto& dep : optionalDependencies_[name]) {
                if (module_initializers_.contains(dep)) {
                    visit(dep);
                }
            }

            initializationOrder_.push_back(name);
        };

    for (const auto& [name, _] : module_initializers_) {
        visit(name);
    }

    spdlog::info("Determined initialization order: {}",
                 atom::utils::toString(initializationOrder_));
}

std::tuple<bool, std::vector<std::string>> Registry::checkDependenciesSatisfied(
    const std::string& name) {
    std::vector<std::string> missingDeps;

    for (const auto& dep : dependencies_[name]) {
        if (!module_initializers_.contains(dep)) {
            missingDeps.push_back(dep);
        }
    }

    return {missingDeps.empty(), missingDeps};
}

std::tuple<bool, std::vector<std::string>> Registry::checkConflicts(
    const std::string& name) {
    std::vector<std::string> conflicts;

    if (!componentInfos_.contains(name)) {
        return {false, {"Component not found"}};
    }

    for (const auto& conflict : componentInfos_[name].conflicts) {
        if (module_initializers_.contains(conflict) &&
            componentInfos_.contains(conflict) &&
            componentInfos_.at(conflict).isEnabled &&
            componentInfos_.at(conflict).isInitialized) {
            conflicts.push_back(conflict);
        }
    }

    return {conflicts.empty(), conflicts};
}
