/*
 * lifecycle.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Advanced Component Lifecycle Management
Provides sophisticated dependency resolution, lifecycle hooks,
and component state management with circular dependency detection.

**************************************************/

#ifndef ATOM_COMPONENT_LIFECYCLE_HPP
#define ATOM_COMPONENT_LIFECYCLE_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../component.hpp"

namespace atom::components {

/**
 * @brief Component lifecycle phases
 */
enum class LifecyclePhase : uint8_t {
    PreConstruction,
    PostConstruction,
    PreInitialization,
    PostInitialization,
    PreActivation,
    PostActivation,
    PreDeactivation,
    PostDeactivation,
    PreDestruction,
    PostDestruction
};

/**
 * @brief Dependency relationship types
 */
enum class DependencyType : uint8_t {
    Required,  // Component cannot function without this dependency
    Optional,  // Component can function without this dependency
    Weak,      // Dependency should be loaded first if available
    Circular   // Circular dependency (requires special handling)
};

/**
 * @brief Dependency constraint for advanced dependency resolution
 */
struct DependencyConstraint {
    std::string name;
    DependencyType type;
    std::string version;  // Version requirement (e.g., ">=1.0.0")
    std::chrono::milliseconds timeout{std::chrono::seconds(30)};
    std::function<bool(const Component&)> validator;  // Custom validation

    DependencyConstraint(const std::string& n,
                         DependencyType t = DependencyType::Required)
        : name(n), type(t) {}
};

/**
 * @brief Lifecycle hook function type
 */
using LifecycleHook = std::function<void(Component&, LifecyclePhase)>;

/**
 * @brief Component lifecycle event
 */
struct LifecycleEvent {
    std::string componentName;
    LifecyclePhase phase;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::microseconds duration{0};
    bool success = true;
    std::string errorMessage;
};

/**
 * @brief Advanced lifecycle manager for components
 */
class LifecycleManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to the lifecycle manager
     */
    static LifecycleManager& instance();

    /**
     * @brief Registers a lifecycle hook for a component
     * @param componentName Component name
     * @param phase Lifecycle phase
     * @param hook Hook function
     */
    void registerHook(const std::string& componentName, LifecyclePhase phase,
                      LifecycleHook hook);

    /**
     * @brief Registers a global lifecycle hook (applies to all components)
     * @param phase Lifecycle phase
     * @param hook Hook function
     */
    void registerGlobalHook(LifecyclePhase phase, LifecycleHook hook);

    /**
     * @brief Adds a dependency constraint
     * @param componentName Component name
     * @param constraint Dependency constraint
     */
    void addDependency(const std::string& componentName,
                       const DependencyConstraint& constraint);

    /**
     * @brief Resolves dependencies for a component
     * @param componentName Component name
     * @return Vector of resolved dependency names in load order
     */
    [[nodiscard]] std::vector<std::string> resolveDependencies(
        const std::string& componentName) const;

    /**
     * @brief Executes lifecycle phase for a component
     * @param component Component instance
     * @param phase Lifecycle phase
     * @return True if phase executed successfully
     */
    bool executePhase(Component& component, LifecyclePhase phase);

    /**
     * @brief Checks for circular dependencies
     * @param componentName Component name
     * @return True if circular dependencies detected
     */
    [[nodiscard]] bool hasCircularDependencies(
        const std::string& componentName) const;

    /**
     * @brief Gets dependency graph for visualization/debugging
     * @return Map of component -> dependencies
     */
    [[nodiscard]] std::unordered_map<std::string, std::vector<std::string>>
    getDependencyGraph() const;

    /**
     * @brief Gets lifecycle events history
     * @param componentName Component name (empty for all components)
     * @return Vector of lifecycle events
     */
    [[nodiscard]] std::vector<LifecycleEvent> getLifecycleHistory(
        const std::string& componentName = "") const;

    /**
     * @brief Clears lifecycle history
     */
    void clearHistory();

    /**
     * @brief Validates all dependencies are satisfied
     * @param componentName Component name
     * @return True if all dependencies are satisfied
     */
    [[nodiscard]] bool validateDependencies(
        const std::string& componentName) const;

private:
    LifecycleManager() = default;
    ~LifecycleManager() = default;

    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;

    // Internal methods
    void recordEvent(const std::string& componentName, LifecyclePhase phase,
                     std::chrono::microseconds duration, bool success = true,
                     const std::string& error = "");

    bool checkCircularDependency(
        const std::string& componentName, const std::string& dependency,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& recursionStack) const;

    std::vector<std::string> topologicalSort(
        const std::unordered_map<std::string, std::vector<std::string>>& graph)
        const;

    mutable std::shared_mutex mutex_;

    // Component-specific hooks
    std::unordered_map<
        std::string,
        std::unordered_map<LifecyclePhase, std::vector<LifecycleHook>>>
        hooks_;

    // Global hooks
    std::unordered_map<LifecyclePhase, std::vector<LifecycleHook>> globalHooks_;

    // Dependencies
    std::unordered_map<std::string, std::vector<DependencyConstraint>>
        dependencies_;

    // Event history
    std::vector<LifecycleEvent> history_;
    static constexpr size_t MAX_HISTORY_SIZE = 10000;
};

/**
 * @brief RAII helper for component lifecycle management
 */
class ComponentLifecycleGuard {
public:
    ComponentLifecycleGuard(Component& component, LifecyclePhase startPhase,
                            LifecyclePhase endPhase);
    ~ComponentLifecycleGuard();

    ComponentLifecycleGuard(const ComponentLifecycleGuard&) = delete;
    ComponentLifecycleGuard& operator=(const ComponentLifecycleGuard&) = delete;

private:
    Component& component_;
    LifecyclePhase endPhase_;
    bool success_;
};

}  // namespace atom::components

#endif  // ATOM_COMPONENT_LIFECYCLE_HPP
