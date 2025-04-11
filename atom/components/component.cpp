/*
 * component.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-26

Description: Basic Component Definition

**************************************************/

#include "component.hpp"

#include "atom/log/loguru.hpp"
#include "dispatch.hpp"
#include "registry.hpp"

Component::Component(std::string name) : m_name_(std::move(name)) {
    LOG_F(INFO, "Component created: {}", m_name_);
    setState(ComponentState::Created);
}

auto Component::getInstance() const -> std::weak_ptr<const Component> {
    LOG_SCOPE_FUNCTION(INFO);
    return shared_from_this();
}

auto Component::initialize() -> bool {
    LOG_SCOPE_FUNCTION(INFO);
    LOG_F(INFO, "Initializing component: {}", m_name_);
    
    setState(ComponentState::Initializing);
    
    if (initFunc) {
        try {
            initFunc(*this);
            LOG_F(INFO, "Successfully ran initialization function for: {}", m_name_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during initialization of {}: {}", m_name_, e.what());
            setState(ComponentState::Error);
            return false;
        } catch (...) {
            LOG_F(ERROR, "Unknown error during initialization of {}", m_name_);
            setState(ComponentState::Error);
            return false;
        }
    }
    
    setState(ComponentState::Active);
    
    // 触发组件初始化事件
    #if ENABLE_EVENT_SYSTEM
    emitEvent("component.initialized");
    #endif
    
    return true;
}

auto Component::destroy() -> bool {
    LOG_SCOPE_FUNCTION(INFO);
    LOG_F(INFO, "Destroying component: {}", m_name_);
    
    setState(ComponentState::Destroying);
    
    if (cleanupFunc) {
        try {
            cleanupFunc();
            LOG_F(INFO, "Successfully ran cleanup function for: {}", m_name_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during cleanup of {}: {}", m_name_, e.what());
            setState(ComponentState::Error);
            return false;
        } catch (...) {
            LOG_F(ERROR, "Unknown error during cleanup of {}", m_name_);
            setState(ComponentState::Error);
            return false;
        }
    }
    
    // 清理资源
    m_OtherComponents_.clear();
    
    // 触发组件销毁事件
    #if ENABLE_EVENT_SYSTEM
    emitEvent("component.destroyed");
    #endif
    
    return true;
}

auto Component::getName() const -> std::string {
    return m_name_;
}

auto Component::getTypeInfo() const -> atom::meta::TypeInfo {
    return m_typeInfo_;
}

void Component::setTypeInfo(atom::meta::TypeInfo typeInfo) {
    m_typeInfo_ = typeInfo;
}

auto Component::getState() const -> ComponentState {
    return m_state_.load();
}

void Component::setState(ComponentState state) {
    // 记录上一个状态
    ComponentState oldState = m_state_.exchange(state);
    
    LOG_F(INFO, "Component '{}' state changed: {} -> {}", 
          m_name_, static_cast<int>(oldState), static_cast<int>(state));
    
    // 触发状态变更事件
    #if ENABLE_EVENT_SYSTEM
    atom::components::Event event;
    event.name = "component.state_changed";
    event.source = m_name_;
    event.timestamp = std::chrono::steady_clock::now();
    
    struct StateChange {
        ComponentState oldState;
        ComponentState newState;
    };
    
    event.data = StateChange{oldState, state};
    handleEvent(event);
    #endif
}

auto Component::getPerformanceStats() const -> const Component::PerformanceStats& {
    return m_PerformanceStats_;
}

void Component::resetPerformanceStats() {
    m_PerformanceStats_.reset();
    LOG_F(INFO, "Reset performance stats for component: {}", m_name_);
}

#if ENABLE_EVENT_SYSTEM
void Component::emitEvent(const std::string& eventName, std::any eventData) {
    atom::components::Event event;
    event.name = eventName;
    event.data = std::move(eventData);
    event.source = m_name_;
    event.timestamp = std::chrono::steady_clock::now();
    
    LOG_F(INFO, "Component '{}' emitting event: {}", m_name_, eventName);
    
    // 更新统计信息
    m_PerformanceStats_.eventCount++;
    
    // 在内部处理事件
    handleEvent(event);
    
    // 将事件传播到全局事件系统
    Registry::instance().triggerEvent(event);
}

atom::components::EventCallbackId Component::on(
    const std::string& eventName, atom::components::EventCallback callback) {
    
    std::lock_guard<std::mutex> lock(m_EventMutex_);
    
    EventHandler handler{
        m_NextEventId_++,
        std::move(callback),
        false // 非一次性
    };
    
    m_EventHandlers_[eventName].push_back(std::move(handler));
    
    LOG_F(INFO, "Component '{}' registered handler for event '{}' with ID {}",
          m_name_, eventName, handler.id);
    
    return handler.id;
}

atom::components::EventCallbackId Component::once(
    const std::string& eventName, atom::components::EventCallback callback) {
    
    std::lock_guard<std::mutex> lock(m_EventMutex_);
    
    EventHandler handler{
        m_NextEventId_++,
        std::move(callback),
        true // 一次性
    };
    
    m_EventHandlers_[eventName].push_back(std::move(handler));
    
    LOG_F(INFO, "Component '{}' registered one-time handler for event '{}' with ID {}",
          m_name_, eventName, handler.id);
    
    return handler.id;
}

bool Component::off(const std::string& eventName, atom::components::EventCallbackId callbackId) {
    std::lock_guard<std::mutex> lock(m_EventMutex_);
    
    auto it = m_EventHandlers_.find(eventName);
    if (it == m_EventHandlers_.end()) {
        LOG_F(WARNING, "Component '{}' has no handlers for event '{}'", m_name_, eventName);
        return false;
    }
    
    auto& handlers = it->second;
    auto handlerIt = std::find_if(handlers.begin(), handlers.end(),
                                 [callbackId](const EventHandler& handler) {
                                     return handler.id == callbackId;
                                 });
    
    if (handlerIt == handlers.end()) {
        LOG_F(WARNING, "Component '{}' has no handler with ID {} for event '{}'",
              m_name_, callbackId, eventName);
        return false;
    }
    
    handlers.erase(handlerIt);
    
    // 如果没有更多处理器，移除整个条目
    if (handlers.empty()) {
        m_EventHandlers_.erase(it);
    }
    
    LOG_F(INFO, "Component '{}' unregistered handler with ID {} for event '{}'",
          m_name_, callbackId, eventName);
    
    return true;
}

void Component::handleEvent(const atom::components::Event& event) {
    // 复制处理器，避免在迭代时修改
    std::vector<EventHandler> handlers;
    std::vector<atom::components::EventCallbackId> handlersToRemove;
    
    {
        std::lock_guard<std::mutex> lock(m_EventMutex_);
        auto it = m_EventHandlers_.find(event.name);
        if (it != m_EventHandlers_.end()) {
            handlers = it->second;
        }
    }
    
    // 调用所有处理器
    for (const auto& handler : handlers) {
        try {
            handler.callback(event);
            
            // 如果是一次性处理器，标记为待移除
            if (handler.once) {
                handlersToRemove.push_back(handler.id);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error in event handler for '{}' in component '{}': {}",
                  event.name, m_name_, e.what());
        }
    }
    
    // 移除一次性处理器
    for (auto id : handlersToRemove) {
        off(event.name, id);
    }
    
    LOG_F(INFO, "Component '{}' handled event '{}' from source '{}'",
          m_name_, event.name, event.source);
}
#endif

void Component::addAlias(const std::string& name,
                         const std::string& alias) const {
    LOG_F(INFO, "Adding alias '{}' for command '{}'", alias, name);
    m_CommandDispatcher_->addAlias(name, alias);
}

void Component::addGroup(const std::string& name,
                         const std::string& group) const {
    LOG_F(INFO, "Adding command '{}' to group '{}'", name, group);
    m_CommandDispatcher_->addGroup(name, group);
}

void Component::setTimeout(const std::string& name,
                           std::chrono::milliseconds timeout) const {
    LOG_F(INFO, "Setting timeout for command '{}': {} ms", name,
          timeout.count());
    m_CommandDispatcher_->setTimeout(name, timeout);
}

void Component::removeCommand(const std::string& name) const {
    LOG_F(INFO, "Removing command '{}'", name);
    m_CommandDispatcher_->removeCommand(name);
}

auto Component::getCommandsInGroup(const std::string& group) const
    -> std::vector<std::string> {
    return m_CommandDispatcher_->getCommandsInGroup(group);
}

auto Component::getCommandDescription(const std::string& name) const
    -> std::string {
    return m_CommandDispatcher_->getCommandDescription(name);
}

#if ENABLE_FASTHASH
emhash::HashSet<std::string> Component::getCommandAliases(
    const std::string& name) const
#else
auto Component::getCommandAliases(const std::string& name) const
    -> std::unordered_set<std::string>
#endif
{
    return m_CommandDispatcher_->getCommandAliases(name);
}

auto Component::getCommandArgAndReturnType(const std::string& name)
    -> std::vector<CommandDispatcher::CommandArgRet> {
    return m_CommandDispatcher_->getCommandArgAndReturnType(name);
}

auto Component::getNeededComponents() -> std::vector<std::string> {
    // 默认实现返回空列表
    // 派生类可以覆盖此方法以指定依赖关系
    return {};
}

void Component::addOtherComponent(const std::string& name,
                                  const std::weak_ptr<Component>& component) {
    if (name.empty()) {
        LOG_F(ERROR, "Cannot add component with empty name");
        THROW_INVALID_ARGUMENT("Cannot add component with empty name");
        return;
    }
    
    if (component.expired()) {
        LOG_F(ERROR, "Cannot add expired component: {}", name);
        THROW_INVALID_ARGUMENT("Cannot add expired component: {}", name);
        return;
    }
    
    if (m_OtherComponents_.contains(name)) {
        LOG_F(WARNING, "Replacing existing component '{}'", name);
    }
    
    LOG_F(INFO, "Adding component '{}' to '{}'", name, m_name_);
    m_OtherComponents_[name] = component;
    
    #if ENABLE_EVENT_SYSTEM
    // 触发组件依赖添加事件
    emitEvent("component.dependency_added", name);
    #endif
}

void Component::removeOtherComponent(const std::string& name) {
    LOG_F(INFO, "Removing component '{}' from '{}'", name, m_name_);
    
    // 检查组件是否存在
    if (!m_OtherComponents_.contains(name)) {
        LOG_F(WARNING, "Component '{}' not found in '{}'", name, m_name_);
        return;
    }
    
    m_OtherComponents_.erase(name);
    
    #if ENABLE_EVENT_SYSTEM
    // 触发组件依赖移除事件
    emitEvent("component.dependency_removed", name);
    #endif
}

void Component::clearOtherComponents() {
    LOG_F(INFO, "Clearing all components from '{}'", m_name_);
    m_OtherComponents_.clear();
    
    #if ENABLE_EVENT_SYSTEM
    // 触发组件依赖清空事件
    emitEvent("component.dependencies_cleared");
    #endif
}

auto Component::getOtherComponent(const std::string& name) -> std::weak_ptr<Component> {
    auto it = m_OtherComponents_.find(name);
    if (it != m_OtherComponents_.end()) {
        if (it->second.expired()) {
            LOG_F(WARNING, "Component '{}' has expired", name);
            m_OtherComponents_.erase(it);
            THROW_OBJECT_EXPIRED("Component '{}' has expired", name);
        }
        return it->second;
    }
    
    LOG_F(ERROR, "Component '{}' not found in '{}'", name, m_name_);
    return {};
}

bool Component::has(const std::string& name) const {
    return m_CommandDispatcher_->has(name);
}

bool Component::hasType(std::string_view name) const {
    return m_classes_.find(name) != m_classes_.end();
}

auto Component::getAllCommands() const -> std::vector<std::string> {
    if (!m_CommandDispatcher_) {
        LOG_F(ERROR, "Command dispatcher not initialized in '{}'", m_name_);
        THROW_OBJ_UNINITIALIZED("Command dispatcher not initialized");
    }
    return m_CommandDispatcher_->getAllCommands();
}

auto Component::getRegisteredTypes() const -> std::vector<std::string> {
    if (!m_TypeCaster_) {
        LOG_F(ERROR, "Type caster not initialized in '{}'", m_name_);
        THROW_OBJ_UNINITIALIZED("Type caster not initialized");
    }
    return m_TypeCaster_->getRegisteredTypes();
}

auto Component::runCommand(const std::string& name,
                           const std::vector<std::any>& args) -> std::any {
    LOG_F(INFO, "Running command '{}' in '{}'", name, m_name_);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // 首先尝试在本组件中运行命令
        if (has(name)) {
            auto result = m_CommandDispatcher_->dispatch(name, args);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime);
            
            // 更新性能统计
            m_PerformanceStats_.commandCallCount++;
            m_PerformanceStats_.updateExecutionTime(duration);
            
            return result;
        }
        
        // 如果本组件中没有找到命令，尝试在其他组件中查找
        std::vector<std::string> expiredComponents;
        
        for (const auto& [key, value] : m_OtherComponents_) {
            if (value.expired()) {
                LOG_F(WARNING, "Component '{}' has expired", key);
                expiredComponents.push_back(key);
                continue;
            }
            
            auto component = value.lock();
            if (component->has(name)) {
                try {
                    LOG_F(INFO, "Running command '{}' in other component '{}'", name, key);
                    auto result = component->dispatch(name, args);
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                        endTime - startTime);
                    
                    // 更新性能统计
                    m_PerformanceStats_.commandCallCount++;
                    m_PerformanceStats_.updateExecutionTime(duration);
                    
                    return result;
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Error running command '{}' in component '{}': {}", 
                          name, key, e.what());
                    m_PerformanceStats_.commandErrorCount++;
                    throw; // 重新抛出异常
                }
            }
        }
        
        // 清理过期组件
        for (const auto& key : expiredComponents) {
            m_OtherComponents_.erase(key);
        }
        
        LOG_F(ERROR, "Command '{}' not found in '{}' or any of its dependencies", name, m_name_);
        THROW_EXCEPTION("Command '{}' not found in '{}' or any of its dependencies", name, m_name_);
    } catch (const std::exception& e) {
        // 更新错误统计
        m_PerformanceStats_.commandErrorCount++;
        
        // 重新抛出异常
        throw;
    }
}

void Component::doc(const std::string& description) {
    m_doc_ = description;
}

auto Component::getDoc() const -> std::string {
    return m_doc_;
}

void Component::defClassConversion(
    const std::shared_ptr<atom::meta::TypeConversionBase>& conversion) {
    if (!m_TypeConverter_) {
        LOG_F(ERROR, "Type converter not initialized in '{}'", m_name_);
        THROW_OBJ_UNINITIALIZED("Type converter not initialized");
    }
    m_TypeConverter_->addConversion(conversion);
}

auto Component::hasVariable(const std::string& name) const -> bool {
    return m_VariableManager_->has(name);
}

auto Component::getVariableDescription(const std::string& name) const
    -> std::string {
    return m_VariableManager_->getDescription(name);
}

auto Component::getVariableAlias(const std::string& name) const -> std::string {
    return m_VariableManager_->getAlias(name);
}

auto Component::getVariableGroup(const std::string& name) const -> std::string {
    return m_VariableManager_->getGroup(name);
}

auto Component::getVariableNames() const -> std::vector<std::string> {
    return m_VariableManager_->getAllVariables();
}
