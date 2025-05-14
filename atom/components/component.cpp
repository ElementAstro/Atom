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

#include <algorithm>
#include <cassert>
#include <chrono>

Component::Component(std::string name) : m_name_(std::move(name)) {
    if (m_name_.empty()) {
        throw std::invalid_argument("Component name cannot be empty");
    }
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
            LOG_F(INFO, "Successfully ran initialization function for: {}",
                  m_name_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during initialization of {}: {}", m_name_,
                  e.what());
            setState(ComponentState::Error);
            return false;
        } catch (...) {
            LOG_F(ERROR, "Unknown error during initialization of {}", m_name_);
            setState(ComponentState::Error);
            return false;
        }
    }

    setState(ComponentState::Active);

// Trigger component initialization event
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

    // Clean up resources
    clearOtherComponents();

// Trigger component destruction event
#if ENABLE_EVENT_SYSTEM
    emitEvent("component.destroyed");
#endif

    return true;
}

auto Component::getName() const noexcept -> std::string_view { return m_name_; }

auto Component::getTypeInfo() const noexcept -> atom::meta::TypeInfo {
    return m_typeInfo_;
}

void Component::setTypeInfo(atom::meta::TypeInfo typeInfo) noexcept {
    m_typeInfo_ = typeInfo;
}

auto Component::getState() const noexcept -> ComponentState {
    return m_state_.load(std::memory_order_acquire);
}

void Component::setState(ComponentState state) noexcept {
    // Record the previous state
    ComponentState oldState =
        m_state_.exchange(state, std::memory_order_acq_rel);

    LOG_F(INFO, "Component '{}' state changed: {} -> {}", m_name_,
          static_cast<int>(oldState), static_cast<int>(state));

// Trigger state change event
#if ENABLE_EVENT_SYSTEM
    try {
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
    } catch (const std::exception& e) {
        // Do not let event handling exceptions affect state setting
        LOG_F(ERROR, "Failed to handle state change event: {}", e.what());
    }
#endif
}

auto Component::getPerformanceStats() const noexcept
    -> const Component::PerformanceStats& {
    return m_PerformanceStats_;
}

void Component::resetPerformanceStats() noexcept {
    m_PerformanceStats_.reset();
    LOG_F(INFO, "Reset performance stats for component: {}", m_name_);
}

#if ENABLE_EVENT_SYSTEM
void Component::emitEvent(std::string_view eventName, std::any eventData) {
    atom::components::Event event;
    event.name = std::string(eventName);
    event.data = std::move(eventData);
    event.source = m_name_;
    event.timestamp = std::chrono::steady_clock::now();

    LOG_F(INFO, "Component '{}' emitting event: {}", m_name_, eventName);

    // Update statistics
    m_PerformanceStats_.eventCount.fetch_add(1, std::memory_order_relaxed);

    // Handle event internally
    handleEvent(event);

    // Propagate event to global event system
    Registry::instance().triggerEvent(event);
}

atom::components::EventCallbackId Component::on(
    std::string_view eventName, atom::components::EventCallback callback) {
    if (!callback) {
        LOG_F(WARNING, "Attempting to register null callback for event '{}'",
              eventName);
        return 0;
    }

    std::unique_lock<std::shared_mutex> lock(m_EventMutex_);

    EventHandler handler{
        m_NextEventId_++, std::move(callback),
        false  // Not one-time
    };

    m_EventHandlers_[std::string(eventName)].push_back(std::move(handler));

    LOG_F(INFO, "Component '{}' registered handler for event '{}' with ID {}",
          m_name_, eventName, handler.id);

    return handler.id;
}

atom::components::EventCallbackId Component::once(
    std::string_view eventName, atom::components::EventCallback callback) {
    if (!callback) {
        LOG_F(WARNING, "Attempting to register null callback for event '{}'",
              eventName);
        return 0;
    }

    std::unique_lock<std::shared_mutex> lock(m_EventMutex_);

    EventHandler handler{
        m_NextEventId_++, std::move(callback),
        true  // One-time
    };

    m_EventHandlers_[std::string(eventName)].push_back(std::move(handler));

    LOG_F(
        INFO,
        "Component '{}' registered one-time handler for event '{}' with ID {}",
        m_name_, eventName, handler.id);

    return handler.id;
}

bool Component::off(std::string_view eventName,
                    atom::components::EventCallbackId callbackId) {
    std::unique_lock<std::shared_mutex> lock(m_EventMutex_);

    auto it = m_EventHandlers_.find(std::string(eventName));
    if (it == m_EventHandlers_.end()) {
        LOG_F(WARNING, "Component '{}' has no handlers for event '{}'", m_name_,
              eventName);
        return false;
    }

    auto& handlers = it->second;
    auto handlerIt = std::find_if(handlers.begin(), handlers.end(),
                                  [callbackId](const EventHandler& handler) {
                                      return handler.id == callbackId;
                                  });

    if (handlerIt == handlers.end()) {
        LOG_F(WARNING,
              "Component '{}' has no handler with ID {} for event '{}'",
              m_name_, callbackId, eventName);
        return false;
    }

    handlers.erase(handlerIt);

    // If no more handlers, remove the entire entry
    if (handlers.empty()) {
        m_EventHandlers_.erase(it);
    }

    LOG_F(INFO, "Component '{}' unregistered handler with ID {} for event '{}'",
          m_name_, callbackId, eventName);

    return true;
}

void Component::handleEvent(const atom::components::Event& event) {
    // Copy handlers to avoid modification during iteration
    std::vector<EventHandler> handlers;
    std::vector<atom::components::EventCallbackId> handlersToRemove;

    {
        std::shared_lock<std::shared_mutex> lock(m_EventMutex_);
        auto it = m_EventHandlers_.find(event.name);
        if (it != m_EventHandlers_.end()) {
            handlers = it->second;
        }
    }

    // Call all handlers
    for (const auto& handler : handlers) {
        try {
            handler.callback(event);

            // If it's a one-time handler, mark for removal
            if (handler.once) {
                handlersToRemove.push_back(handler.id);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR,
                  "Error in event handler for '{}' in component '{}': {}",
                  event.name, m_name_, e.what());
        }
    }

    // Remove one-time handlers
    for (auto id : handlersToRemove) {
        off(event.name, id);
    }

    LOG_F(INFO, "Component '{}' handled event '{}' from source '{}'", m_name_,
          event.name, event.source);
}
#endif

void Component::addAlias(std::string_view name, std::string_view alias) const {
    LOG_F(INFO, "Adding alias '{}' for command '{}'", alias, name);
    bool result =
        m_CommandDispatcher_->addAlias(std::string(name), std::string(alias));
    if (!result) {
        LOG_F(WARNING, "Failed to add alias '{}' for command '{}'", alias,
              name);
    }
}

void Component::addGroup(std::string_view name, std::string_view group) const {
    LOG_F(INFO, "Adding command '{}' to group '{}'", name, group);
    bool result =
        m_CommandDispatcher_->addGroup(std::string(name), std::string(group));
    if (!result) {
        LOG_F(WARNING, "Failed to add command '{}' to group '{}'", name, group);
    }
}

void Component::setTimeout(std::string_view name,
                           std::chrono::milliseconds timeout) const {
    LOG_F(INFO, "Setting timeout for command '{}': {} ms", name,
          timeout.count());
    bool result = m_CommandDispatcher_->setTimeout(std::string(name), timeout);
    if (!result) {
        LOG_F(WARNING, "Failed to set timeout for command '{}'", name);
    }
}

void Component::removeCommand(std::string_view name) const {
    LOG_F(INFO, "Removing command '{}'", name);
    bool result = m_CommandDispatcher_->removeCommand(std::string(name));
    if (!result) {
        LOG_F(WARNING, "Failed to remove command '{}'", name);
    }
}

auto Component::getCommandsInGroup(std::string_view group) const
    -> std::vector<std::string> {
    return m_CommandDispatcher_->getCommandsInGroup(std::string(group));
}

auto Component::getCommandDescription(std::string_view name) const
    -> std::string {
    return m_CommandDispatcher_->getCommandDescription(std::string(name));
}

#if ENABLE_FASTHASH
emhash::HashSet<std::string> Component::getCommandAliases(
    std::string_view name) const
#else
auto Component::getCommandAliases(std::string_view name) const
    -> std::unordered_set<std::string>
#endif
{
    return m_CommandDispatcher_->getCommandAliases(std::string(name));
}

auto Component::getCommandArgAndReturnType(std::string_view name)
    -> std::vector<CommandDispatcher::CommandArgRet> {
    return m_CommandDispatcher_->getCommandArgAndReturnType(std::string(name));
}

auto Component::getNeededComponents() -> std::vector<std::string> {
    // Default implementation returns an empty list
    // Derived classes can override this method to specify dependencies
    return {};
}

void Component::addOtherComponent(std::string_view name,
                                  const std::weak_ptr<Component>& component) {
    if (name.empty()) {
        LOG_F(ERROR, "Cannot add component with empty name");
        throw std::invalid_argument("Cannot add component with empty name");
    }

    if (component.expired()) {
        LOG_F(ERROR, "Cannot add expired component: {}", name);
        throw std::invalid_argument(
            std::string("Cannot add expired component: ") + std::string(name));
    }

    std::unique_lock<std::shared_mutex> lock(m_ComponentsMutex_);

    std::string nameStr(name);
    if (m_OtherComponents_.contains(nameStr)) {
        LOG_F(WARNING, "Replacing existing component '{}'", name);
    }

    LOG_F(INFO, "Adding component '{}' to '{}'", name, m_name_);
    m_OtherComponents_[nameStr] = component;

#if ENABLE_EVENT_SYSTEM
    // Trigger component dependency added event
    emitEvent("component.dependency_added", nameStr);
#endif
}

void Component::removeOtherComponent(std::string_view name) noexcept {
    std::unique_lock<std::shared_mutex> lock(m_ComponentsMutex_);

    LOG_F(INFO, "Removing component '{}' from '{}'", name, m_name_);

    // Check if the component exists
    std::string nameStr(name);
    if (!m_OtherComponents_.contains(nameStr)) {
        LOG_F(WARNING, "Component '{}' not found in '{}'", name, m_name_);
        return;
    }

    m_OtherComponents_.erase(nameStr);

#if ENABLE_EVENT_SYSTEM
    try {
        // Trigger component dependency removed event
        emitEvent("component.dependency_removed", nameStr);
    } catch (...) {
        // Ignore event handling exceptions
    }
#endif
}

void Component::clearOtherComponents() noexcept {
    std::unique_lock<std::shared_mutex> lock(m_ComponentsMutex_);

    LOG_F(INFO, "Clearing all components from '{}'", m_name_);
    m_OtherComponents_.clear();

#if ENABLE_EVENT_SYSTEM
    try {
        // Trigger component dependencies cleared event
        emitEvent("component.dependencies_cleared");
    } catch (...) {
        // Ignore event handling exceptions
    }
#endif
}

auto Component::getOtherComponent(std::string_view name)
    -> std::weak_ptr<Component> {
    std::shared_lock<std::shared_mutex> lock(m_ComponentsMutex_);

    std::string nameStr(name);
    auto it = m_OtherComponents_.find(nameStr);
    if (it != m_OtherComponents_.end()) {
        if (it->second.expired()) {
            LOG_F(WARNING, "Component '{}' has expired", name);

            // Release shared lock and acquire unique lock to modify container
            lock.unlock();
            std::unique_lock<std::shared_mutex> uniqueLock(m_ComponentsMutex_);

            // Check if the entry still exists and is expired while we acquired
            // the unique lock
            it = m_OtherComponents_.find(nameStr);
            if (it != m_OtherComponents_.end() && it->second.expired()) {
                m_OtherComponents_.erase(it);
            }

            THROW_OBJECT_EXPIRED("Component '{}' has expired", nameStr);
        }
        return it->second;
    }

    LOG_F(ERROR, "Component '{}' not found in '{}'", name, m_name_);
    return {};
}

bool Component::has(std::string_view name) const noexcept {
    try {
        return m_CommandDispatcher_->has(std::string(name));
    } catch (...) {
        LOG_F(ERROR, "Error checking if command '{}' exists", name);
        return false;
    }
}

bool Component::hasType(std::string_view name) const noexcept {
    return m_classes_.find(name) != m_classes_.end();
}

auto Component::getAllCommands() const -> std::vector<std::string> {
    if (!m_CommandDispatcher_) {
        LOG_F(ERROR, "Command dispatcher not initialized in '{}'", m_name_);
        throw std::runtime_error("Command dispatcher not initialized");
    }
    return m_CommandDispatcher_->getAllCommands();
}

auto Component::getRegisteredTypes() const -> std::vector<std::string> {
    if (!m_TypeCaster_) {
        LOG_F(ERROR, "Type caster not initialized in '{}'", m_name_);
        throw std::runtime_error("Type caster not initialized");
    }
    return m_TypeCaster_->getRegisteredTypes();
}

auto Component::dispatch(std::string_view name,
                         std::span<const std::any> args) const -> std::any {
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        std::vector<std::any> argsVec(args.begin(), args.end());
        auto result =
            m_CommandDispatcher_->dispatch(std::string(name), argsVec);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);

        // Update performance statistics
        auto& stats = const_cast<PerformanceStats&>(m_PerformanceStats_);
        stats.commandCallCount.fetch_add(1, std::memory_order_relaxed);
        stats.updateExecutionTime(duration);

        return result;
    } catch (const std::exception& e) {
        auto& stats = const_cast<PerformanceStats&>(m_PerformanceStats_);
        stats.commandErrorCount.fetch_add(1, std::memory_order_relaxed);

        LOG_F(ERROR, "Error dispatching command '{}': {}", name, e.what());
        throw;  // Rethrow exception
    }
}

auto Component::runCommand(std::string_view name,
                           std::span<const std::any> args) -> std::any {
    LOG_F(INFO, "Running command '{}' in '{}'", name, m_name_);

    auto startTime = std::chrono::high_resolution_clock::now();
    std::string nameStr(name);

    try {
        // First, try to run the command in this component
        if (has(name)) {
            std::vector<std::any> argsVec(args.begin(), args.end());
            auto result = m_CommandDispatcher_->dispatch(nameStr, argsVec);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime);

            // Update performance statistics
            m_PerformanceStats_.commandCallCount.fetch_add(
                1, std::memory_order_relaxed);
            m_PerformanceStats_.updateExecutionTime(duration);

            return result;
        }

        // If the command is not found in this component, try to find it in
        // other components
        std::shared_lock<std::shared_mutex> lock(m_ComponentsMutex_);
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
                    LOG_F(INFO, "Running command '{}' in other component '{}'",
                          name, key);
                    std::vector<std::any> argsVec(args.begin(), args.end());
                    auto result = component->dispatch(name, argsVec);
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration =
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            endTime - startTime);

                    // Update performance statistics
                    m_PerformanceStats_.commandCallCount.fetch_add(
                        1, std::memory_order_relaxed);
                    m_PerformanceStats_.updateExecutionTime(duration);

                    return result;
                } catch (const std::exception& e) {
                    LOG_F(ERROR,
                          "Error running command '{}' in component '{}': {}",
                          name, key, e.what());
                    m_PerformanceStats_.commandErrorCount.fetch_add(
                        1, std::memory_order_relaxed);
                    throw;  // Rethrow exception
                }
            }
        }

        // Release shared lock
        lock.unlock();

        // Acquire unique lock to clean up expired components
        if (!expiredComponents.empty()) {
            std::unique_lock<std::shared_mutex> uniqueLock(m_ComponentsMutex_);
            for (const auto& key : expiredComponents) {
                m_OtherComponents_.erase(key);
            }
        }

        LOG_F(ERROR,
              "Command '{}' not found in '{}' or any of its dependencies", name,
              m_name_);
        throw atom::error::Exception(
            ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
            "Command '{}' not found in '{}' or any of its dependencies",
            nameStr, m_name_);
    } catch (const std::exception& e) {
        // Update error statistics
        m_PerformanceStats_.commandErrorCount.fetch_add(
            1, std::memory_order_relaxed);

        // Rethrow exception
        throw;
    }
}

void Component::doc(std::string_view description) {
    m_doc_ = std::string(description);
}

auto Component::getDoc() const noexcept -> std::string_view { return m_doc_; }

void Component::defClassConversion(
    const std::shared_ptr<atom::meta::TypeConversionBase>& conversion) {
    if (!m_TypeConverter_) {
        LOG_F(ERROR, "Type converter not initialized in '{}'", m_name_);
        throw std::runtime_error("Type converter not initialized");
    }
    m_TypeConverter_->addConversion(conversion);
}

auto Component::hasVariable(std::string_view name) const noexcept -> bool {
    try {
        return m_VariableManager_->has(std::string(name));
    } catch (...) {
        LOG_F(ERROR, "Error checking if variable '{}' exists", name);
        return false;
    }
}

auto Component::getVariableDescription(std::string_view name) const
    -> std::string {
    return m_VariableManager_->getDescription(std::string(name));
}

auto Component::getVariableAlias(std::string_view name) const -> std::string {
    return m_VariableManager_->getAlias(std::string(name));
}

auto Component::getVariableGroup(std::string_view name) const -> std::string {
    return m_VariableManager_->getGroup(std::string(name));
}

auto Component::getVariableNames() const -> std::vector<std::string> {
    return m_VariableManager_->getAllVariables();
}
