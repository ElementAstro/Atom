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
#include <filesystem>
#include <thread>

#include "atom/log/loguru.hpp"
#include "atom/utils/to_string.hpp"

#define THROW_REGISTRY_EXCEPTION(...)                                        \
    throw Registry::RegistryException(ATOM_FILE_NAME, ATOM_FILE_LINE,         \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

auto Registry::instance() -> Registry& {
    static Registry instance;
    return instance;
}

void Registry::registerModule(const std::string& name,
                              Component::InitFunc init_func) {
    std::scoped_lock lock(mutex_);
    LOG_F(INFO, "Registering module: {}", name);
    module_initializers_[name] = std::move(init_func);
    
    // 创建基础的组件元数据
    if (!componentInfos_.contains(name)) {
        ComponentInfo info;
        info.name = name;
        info.loadTime = std::chrono::system_clock::now();
        componentInfos_[name] = info;
    }
}

void Registry::addInitializer(const std::string& name,
                              Component::InitFunc init_func,
                              Component::CleanupFunc cleanup_func,
                              std::optional<ComponentInfo> metadata) {
    std::scoped_lock lock(mutex_);
    if (initializers_.contains(name)) {
        LOG_F(WARNING, "Component '{}' already registered, skipping", name);
        return;
    }
    
    LOG_F(INFO, "Adding initializer for component: {}", name);
    
    // 创建组件实例
    initializers_[name] = std::make_shared<Component>(name);
    initializers_[name]->initFunc = std::move(init_func);
    initializers_[name]->cleanupFunc = std::move(cleanup_func);
    
    // 更新组件元数据
    if (metadata.has_value()) {
        componentInfos_[name] = *metadata;
        componentInfos_[name].loadTime = std::chrono::system_clock::now();
    } else if (!componentInfos_.contains(name)) {
        ComponentInfo info;
        info.name = name;
        info.loadTime = std::chrono::system_clock::now();
        componentInfos_[name] = info;
    }
    
    componentInfos_[name].isInitialized = false;
}

void Registry::addDependency(const std::string& name,
                             const std::string& dependency,
                             bool isOptional) {
    std::unique_lock lock(mutex_);
    
    if (name == dependency) {
        LOG_F(ERROR, "Component '{}' cannot depend on itself", name);
        THROW_REGISTRY_EXCEPTION("Component '{}' cannot depend on itself", name);
    }
    
    if (hasCircularDependency(name, dependency)) {
        LOG_F(ERROR, "Circular dependency detected: {} -> {}", name, dependency);
        THROW_REGISTRY_EXCEPTION("Circular dependency detected: {} -> {}", name, dependency);
    }
    
    LOG_F(INFO, "Adding {} dependency: {} -> {}", 
          isOptional ? "optional" : "required", name, dependency);
    
    if (isOptional) {
        optionalDependencies_[name].insert(dependency);
        
        // 添加到组件元数据
        if (componentInfos_.contains(name)) {
            auto& deps = componentInfos_[name].optionalDeps;
            if (std::find(deps.begin(), deps.end(), dependency) == deps.end()) {
                deps.push_back(dependency);
            }
        }
    } else {
        dependencies_[name].insert(dependency);
        
        // 添加到组件元数据
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
    LOG_F(INFO, "Initializing all components");
    
    if (forceReload) {
        LOG_F(INFO, "Force reloading all components");
        // 清理所有初始化状态
        for (auto& [name, info] : componentInfos_) {
            info.isInitialized = false;
        }
    }
    
    // 确定初始化顺序
    determineInitializationOrder();
    
    // 按顺序初始化组件
    for (const auto& name : initializationOrder_) {
        std::unordered_set<std::string> initStack;
        LOG_F(INFO, "Initializing component: {}", name);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        initializeComponent(name, initStack);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        // 更新性能统计
        if (componentInfos_.contains(name)) {
            componentInfos_[name].stats.initTime = 
                std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        }
    }
    
    LOG_F(INFO, "All components initialized successfully");
}

void Registry::cleanupAll(bool force) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Cleaning up all components");
    
    // 按初始化顺序的逆序进行清理
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
            LOG_F(INFO, "Cleaning up component: {}", name);
            component->cleanupFunc();
            componentInfos_[name].isInitialized = false;
            
            // 触发组件卸载事件
            #if ENABLE_EVENT_SYSTEM
            atom::components::Event event;
            event.name = "component.unloaded";
            event.source = name;
            event.timestamp = std::chrono::steady_clock::now();
            triggerEvent(event);
            #endif
            
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error cleaning up component {}: {}", name, e.what());
            
            if (force) {
                LOG_F(WARNING, "Forcing cleanup to continue despite error");
            } else {
                throw;
            }
        }
    }
    
    if (force) {
        // 强制清理所有资源
        LOG_F(INFO, "Force clearing all component resources");
        initializers_.clear();
        for (auto& [name, info] : componentInfos_) {
            info.isInitialized = false;
        }
    }
    
    LOG_F(INFO, "All components cleaned up successfully");
}

auto Registry::isInitialized(const std::string& name) const -> bool {
    std::shared_lock lock(mutex_);
    
    if (!componentInfos_.contains(name)) {
        return false;
    }
    
    return componentInfos_.at(name).isInitialized;
}

auto Registry::isEnabled(const std::string& name) const -> bool {
    std::shared_lock lock(mutex_);
    
    if (!componentInfos_.contains(name)) {
        return false;
    }
    
    return componentInfos_.at(name).isEnabled;
}

bool Registry::enableComponent(const std::string& name, bool enable) {
    std::unique_lock lock(mutex_);
    
    if (!componentInfos_.contains(name)) {
        LOG_F(ERROR, "Cannot enable/disable non-existent component: {}", name);
        return false;
    }
    
    componentInfos_[name].isEnabled = enable;
    LOG_F(INFO, "{} component: {}", enable ? "Enabled" : "Disabled", name);
    
    // 触发组件启用/禁用事件
    #if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = enable ? "component.enabled" : "component.disabled";
    event.source = name;
    event.timestamp = std::chrono::steady_clock::now();
    // 在锁外触发事件，避免死锁
    lock.unlock();
    triggerEvent(event);
    #endif
    
    return true;
}

void Registry::reinitializeComponent(const std::string& name, bool reloadDependencies) {
    std::unique_lock lock(mutex_);
    LOG_F(INFO, "Reinitializing component: {}", name);
    
    if (!initializers_.contains(name)) {
        LOG_F(ERROR, "Cannot reinitialize non-existent component: {}", name);
        THROW_OBJ_NOT_EXIST("Component not registered: {}", name);
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 如果已初始化，先清理
    if (componentInfos_[name].isInitialized && initializers_[name]->cleanupFunc) {
        try {
            initializers_[name]->cleanupFunc();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during cleanup of {}: {}", name, e.what());
        }
        componentInfos_[name].isInitialized = false;
    }
    
    // 如果需要重新加载依赖
    if (reloadDependencies) {
        auto deps = dependencies_[name];
        for (const auto& dep : deps) {
            if (initializers_.contains(dep)) {
                reinitializeComponent(dep, false); // 避免循环依赖
            }
        }
    }
    
    // 获取模块初始化函数
    auto it = module_initializers_.find(name);
    if (it != module_initializers_.end()) {
        // 创建新的组件实例
        auto component = std::make_shared<Component>(name);
        it->second(*component);
        initializers_[name] = component;
        
        // 标记为已初始化
        componentInfos_[name].isInitialized = true;
        componentInfos_[name].lastUsed = std::chrono::system_clock::now();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        componentInfos_[name].stats.initTime = 
            std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // 触发组件重载事件
        #if ENABLE_EVENT_SYSTEM
        atom::components::Event event;
        event.name = "component.reloaded";
        event.source = name;
        event.timestamp = std::chrono::steady_clock::now();
        lock.unlock(); // 释放锁以避免死锁
        triggerEvent(event);
        #endif
    } else {
        LOG_F(ERROR, "No initializer function found for component: {}", name);
        THROW_OBJ_UNINITIALIZED("No initializer function for component: {}", name);
    }
}

auto Registry::getComponent(const std::string& name) const
    -> std::shared_ptr<Component> {
    std::shared_lock lock(mutex_);
    
    if (!initializers_.contains(name)) {
        LOG_F(ERROR, "Component not registered: {}", name);
        THROW_OBJ_NOT_EXIST("Component not registered: {}", name);
    }
    
    if (componentInfos_.contains(name)) {
        // 更新最后使用时间
        auto& info = const_cast<ComponentInfo&>(componentInfos_.at(name));
        info.lastUsed = std::chrono::system_clock::now();
    }
    
    return initializers_.at(name);
}

auto Registry::getOrLoadComponent(const std::string& name) -> std::shared_ptr<Component> {
    {
        std::shared_lock readLock(mutex_);
        
        // 检查组件是否已加载
        if (initializers_.contains(name) && 
            componentInfos_.contains(name) && 
            componentInfos_[name].isInitialized) {
            
            // 更新最后使用时间
            componentInfos_[name].lastUsed = std::chrono::system_clock::now();
            return initializers_[name];
        }
    }
    
    // 需要加载组件
    std::unique_lock writeLock(mutex_);
    
    // 再次检查，避免在获取写锁期间组件被其他线程加载
    if (initializers_.contains(name) && 
        componentInfos_.contains(name) && 
        componentInfos_[name].isInitialized) {
        
        componentInfos_[name].lastUsed = std::chrono::system_clock::now();
        return initializers_[name];
    }
    
    LOG_F(INFO, "Lazy loading component: {}", name);
    
    // 检查是否有注册此组件
    if (!module_initializers_.contains(name)) {
        LOG_F(ERROR, "Cannot lazy load unregistered component: {}", name);
        THROW_OBJ_NOT_EXIST("Component not registered: {}", name);
    }
    
    // 检查依赖是否满足
    auto [satisfied, missingDeps] = checkDependenciesSatisfied(name);
    if (!satisfied) {
        LOG_F(ERROR, "Cannot load component {} due to missing dependencies: {}", 
              name, atom::utils::toString(missingDeps));
        THROW_REGISTRY_EXCEPTION("Cannot load component {} due to missing dependencies", name);
    }
    
    // 初始化组件
    std::unordered_set<std::string> initStack;
    auto startTime = std::chrono::high_resolution_clock::now();
    initializeComponent(name, initStack);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // 更新性能统计
    componentInfos_[name].stats.initTime = 
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // 触发组件加载事件
    #if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = "component.loaded";
    event.source = name;
    event.timestamp = std::chrono::steady_clock::now();
    writeLock.unlock(); // 释放锁以避免死锁
    triggerEvent(event);
    #endif
    
    return initializers_[name];
}

auto Registry::getAllComponents() const -> std::vector<std::shared_ptr<Component>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<Component>> components;
    
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

auto Registry::getComponentInfo(const std::string& name) const -> const ComponentInfo& {
    std::shared_lock lock(mutex_);
    
    if (!componentInfos_.contains(name)) {
        LOG_F(ERROR, "Component info not found: {}", name);
        THROW_OBJ_NOT_EXIST("Component info not found: {}", name);
    }
    
    return componentInfos_.at(name);
}

bool Registry::updateComponentInfo(const std::string& name, const ComponentInfo& info) {
    std::unique_lock lock(mutex_);
    
    if (!componentInfos_.contains(name)) {
        LOG_F(ERROR, "Cannot update info for non-existent component: {}", name);
        return false;
    }
    
    // 保持某些不可修改的字段
    ComponentInfo newInfo = info;
    newInfo.name = name;  // 名称不可修改
    newInfo.loadTime = componentInfos_[name].loadTime;  // 加载时间不可修改
    newInfo.isInitialized = componentInfos_[name].isInitialized;  // 初始化状态不可直接修改
    
    componentInfos_[name] = newInfo;
    LOG_F(INFO, "Updated component info for: {}", name);
    
    return true;
}

bool Registry::loadComponentFromFile(const std::string& path) {
    #if ENABLE_HOT_RELOAD
    namespace fs = std::filesystem;
    
    if (!fs::exists(path)) {
        LOG_F(ERROR, "Component file not found: {}", path);
        return false;
    }
    
    std::string name = fs::path(path).stem().string();
    LOG_F(INFO, "Loading component from file: {} (name: {})", path, name);
    
    // 记录文件时间戳，用于热重载检测
    componentFileTimestamps_[name] = fs::last_write_time(path);
    
    // TODO: 实现动态库加载
    // 这里需要根据实际需求实现动态库加载逻辑
    LOG_F(WARNING, "Dynamic library loading not implemented yet");
    
    return true;
    #else
    LOG_F(ERROR, "Hot reload not enabled, cannot load component from file");
    return false;
    #endif
}

bool Registry::watchComponentChanges(bool enable) {
    #if ENABLE_HOT_RELOAD
    std::unique_lock lock(mutex_);
    
    if (enable == watchingForChanges_) {
        return true;  // 已经是所需状态
    }
    
    if (enable) {
        LOG_F(INFO, "Starting component file watcher");
        watchingForChanges_ = true;
        
        // 启动文件监控线程
        fileWatcherFuture_ = std::async(std::launch::async, [this]() {
            namespace fs = std::filesystem;
            
            while (watchingForChanges_) {
                {
                    std::shared_lock lock(mutex_);
                    
                    // 检查所有已注册组件文件是否有变化
                    for (auto& [name, lastTime] : componentFileTimestamps_) {
                        if (!componentInfos_.contains(name) || 
                            !componentInfos_.at(name).isHotReload) {
                            continue;
                        }
                        
                        try {
                            std::string path = componentInfos_.at(name).configPath;
                            if (path.empty() || !fs::exists(path)) {
                                continue;
                            }
                            
                            auto currentTime = fs::last_write_time(path);
                            if (currentTime != lastTime) {
                                LOG_F(INFO, "Detected change in component file: {}", path);
                                lastTime = currentTime;
                                
                                // 需要释放共享锁并获取独占锁才能重新加载组件
                                lock.unlock();
                                reinitializeComponent(name, false);
                                lock = std::shared_lock(mutex_);
                            }
                        } catch (const std::exception& e) {
                            LOG_F(ERROR, "Error checking component file: {}", e.what());
                        }
                    }
                }
                
                // 休眠一段时间，避免频繁检查
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
    } else {
        LOG_F(INFO, "Stopping component file watcher");
        watchingForChanges_ = false;
        
        // 等待监控线程结束
        if (fileWatcherFuture_.valid()) {
            fileWatcherFuture_.wait();
        }
    }
    
    return true;
    #else
    LOG_F(ERROR, "Hot reload not enabled, cannot watch component changes");
    return false;
    #endif
}

bool Registry::removeComponent(const std::string& name) {
    std::unique_lock lock(mutex_);
    
    if (!initializers_.contains(name)) {
        LOG_F(WARNING, "Cannot remove non-existent component: {}", name);
        return false;
    }
    
    // 检查是否有其他组件依赖此组件
    std::vector<std::string> dependents;
    for (const auto& [compName, deps] : dependencies_) {
        if (deps.contains(name) && compName != name) {
            dependents.push_back(compName);
        }
    }
    
    if (!dependents.empty()) {
        LOG_F(ERROR, "Cannot remove component {} because it is depended upon by: {}",
              name, atom::utils::toString(dependents));
        return false;
    }
    
    // 先清理组件资源
    if (componentInfos_[name].isInitialized && initializers_[name]->cleanupFunc) {
        try {
            initializers_[name]->cleanupFunc();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during cleanup of {}: {}", name, e.what());
        }
    }
    
    // 移除组件注册信息
    initializers_.erase(name);
    module_initializers_.erase(name);
    dependencies_.erase(name);
    optionalDependencies_.erase(name);
    componentInfos_.erase(name);
    
    // 从初始化顺序中移除
    auto it = std::find(initializationOrder_.begin(), initializationOrder_.end(), name);
    if (it != initializationOrder_.end()) {
        initializationOrder_.erase(it);
    }
    
    #if ENABLE_HOT_RELOAD
    // 移除文件监控信息
    componentFileTimestamps_.erase(name);
    #endif
    
    LOG_F(INFO, "Component removed: {}", name);
    
    // 触发组件移除事件
    #if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = "component.removed";
    event.source = name;
    event.timestamp = std::chrono::steady_clock::now();
    lock.unlock(); // 释放锁以避免死锁
    triggerEvent(event);
    #endif
    
    return true;
}

#if ENABLE_EVENT_SYSTEM
atom::components::EventCallbackId Registry::subscribeToEvent(
    const std::string& eventName, 
    atom::components::EventCallback callback) {
    
    std::unique_lock lock(mutex_);
    
    EventSubscription sub;
    sub.id = nextEventId_++;
    sub.callback = std::move(callback);
    
    eventSubscriptions_[eventName].push_back(std::move(sub));
    
    LOG_F(INFO, "Subscribed to event '{}' with ID {}", eventName, sub.id);
    return sub.id;
}

bool Registry::unsubscribeFromEvent(
    const std::string& eventName, 
    atom::components::EventCallbackId callbackId) {
    
    std::unique_lock lock(mutex_);
    
    auto it = eventSubscriptions_.find(eventName);
    if (it == eventSubscriptions_.end()) {
        LOG_F(WARNING, "No subscriptions found for event: {}", eventName);
        return false;
    }
    
    auto& subs = it->second;
    auto subIt = std::find_if(subs.begin(), subs.end(), 
                             [callbackId](const EventSubscription& sub) {
                                 return sub.id == callbackId;
                             });
    
    if (subIt == subs.end()) {
        LOG_F(WARNING, "Subscription ID {} not found for event {}", callbackId, eventName);
        return false;
    }
    
    subs.erase(subIt);
    LOG_F(INFO, "Unsubscribed from event '{}' with ID {}", eventName, callbackId);
    
    // 如果没有更多订阅，移除整个条目
    if (subs.empty()) {
        eventSubscriptions_.erase(it);
    }
    
    return true;
}

void Registry::triggerEvent(const atom::components::Event& event) {
    // 复制回调列表，避免在调用回调时死锁
    std::vector<atom::components::EventCallback> callbacks;
    
    {
        std::shared_lock lock(mutex_);
        auto it = eventSubscriptions_.find(event.name);
        if (it != eventSubscriptions_.end()) {
            for (const auto& sub : it->second) {
                callbacks.push_back(sub.callback);
            }
        }
    }
    
    // 调用所有回调
    for (const auto& callback : callbacks) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error in event callback for {}: {}", event.name, e.what());
        }
    }
    
    LOG_F(INFO, "Triggered event '{}' from source '{}'", event.name, event.source);
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
    
    // 如果已经初始化，则跳过
    if (componentInfos_.contains(name) && componentInfos_[name].isInitialized) {
        if (init_stack.contains(name)) {
            THROW_REGISTRY_EXCEPTION(
                "Circular dependency detected while initializing component '{}'",
                name);
        }
        return;
    }
    
    // 检查组件是否禁用
    if (componentInfos_.contains(name) && !componentInfos_[name].isEnabled) {
        LOG_F(INFO, "Skipping disabled component: {}", name);
        return;
    }
    
    // 检查循环依赖
    if (init_stack.contains(name)) {
        THROW_REGISTRY_EXCEPTION(
            "Circular dependency detected while initializing: {}", name);
    }
    
    init_stack.insert(name);
    
    // 初始化所有依赖
    for (const auto& dep : dependencies_[name]) {
        initializeComponent(dep, init_stack);
    }
    
    // 尝试初始化可选依赖
    for (const auto& dep : optionalDependencies_[name]) {
        if (module_initializers_.contains(dep)) {
            try {
                initializeComponent(dep, init_stack);
            } catch (const std::exception& e) {
                LOG_F(WARNING, "Failed to initialize optional dependency {} for {}: {}",
                      dep, name, e.what());
                // 可选依赖初始化失败不影响当前组件
            }
        }
    }
    
    // 初始化当前组件
    if (auto it = module_initializers_.find(name); it != module_initializers_.end()) {
        if (!initializers_.contains(name)) {
            initializers_[name] = std::make_shared<Component>(name);
        }
        
        LOG_F(INFO, "Running initializer for component: {}", name);
        try {
            auto startTime = std::chrono::high_resolution_clock::now();
            it->second(*initializers_[name]);
            auto endTime = std::chrono::high_resolution_clock::now();
            
            // 更新性能统计
            if (componentInfos_.contains(name)) {
                componentInfos_[name].stats.loadTime = 
                    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            }
            
            // 标记为已初始化
            if (initializers_[name]->initialize()) {
                LOG_F(INFO, "Component initialized successfully: {}", name);
                componentInfos_[name].isInitialized = true;
                componentInfos_[name].lastUsed = std::chrono::system_clock::now();
            } else {
                LOG_F(ERROR, "Component initialization returned false: {}", name);
                THROW_REGISTRY_EXCEPTION("Component initialization failed: {}", name);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error initializing component {}: {}", name, e.what());
            throw;
        }
    } else {
        LOG_F(ERROR, "No initializer function found for component: {}", name);
        THROW_REGISTRY_EXCEPTION("No initializer function for component: {}", name);
    }
    
    init_stack.erase(name);
}

void Registry::determineInitializationOrder() {
    initializationOrder_.clear();
    std::unordered_set<std::string> visited;
    
    std::function<void(const std::string&)> visit =
        [&](const std::string& name) {
            if (!visited.contains(name)) {
                visited.insert(name);
                
                // 首先访问此组件的所有依赖
                for (const auto& dep : dependencies_[name]) {
                    if (module_initializers_.contains(dep)) {
                        visit(dep);
                    } else {
                        LOG_F(WARNING, "Dependency '{}' not found for component '{}'", dep, name);
                    }
                }
                
                // 然后尝试访问可选依赖
                for (const auto& dep : optionalDependencies_[name]) {
                    if (module_initializers_.contains(dep)) {
                        visit(dep);
                    }
                }
                
                // 最后将此组件添加到初始化顺序
                initializationOrder_.push_back(name);
            }
        };
    
    // 确保所有注册的组件都被添加到初始化顺序中
    for (const auto& pair : module_initializers_) {
        visit(pair.first);
    }
    
    LOG_F(INFO, "Determined initialization order: {}", atom::utils::toString(initializationOrder_));
}

std::tuple<bool, std::vector<std::string>> Registry::checkDependenciesSatisfied(
    const std::string& name) {
    
    std::vector<std::string> missingDeps;
    
    // 检查必需依赖
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
    
    // 检查冲突组件
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
