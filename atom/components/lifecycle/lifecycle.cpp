/*
 * lifecycle.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "lifecycle.hpp"

#include <algorithm>
#include <queue>
// #include <sstream> // removed unused include

namespace atom::components {

LifecycleManager& LifecycleManager::instance() {
    static LifecycleManager instance;
    return instance;
}

void LifecycleManager::registerHook(const std::string& componentName,
                                    LifecyclePhase phase, LifecycleHook hook) {
    std::unique_lock lock(mutex_);
    hooks_[componentName][phase].push_back(std::move(hook));
}

void LifecycleManager::registerGlobalHook(LifecyclePhase phase,
                                          LifecycleHook hook) {
    std::unique_lock lock(mutex_);
    globalHooks_[phase].push_back(std::move(hook));
}

void LifecycleManager::addDependency(const std::string& componentName,
                                     const DependencyConstraint& constraint) {
    std::unique_lock lock(mutex_);
    dependencies_[componentName].push_back(constraint);
}

std::vector<std::string> LifecycleManager::resolveDependencies(
    const std::string& componentName) const {
    std::shared_lock lock(mutex_);

    // Build dependency graph
    std::unordered_map<std::string, std::vector<std::string>> graph;
    std::unordered_set<std::string> allComponents;

    std::function<void(const std::string&)> buildGraph =
        [&](const std::string& name) {
            if (allComponents.contains(name))
                return;
            allComponents.insert(name);

            auto it = dependencies_.find(name);
            if (it != dependencies_.end()) {
                for (const auto& dep : it->second) {
                    if (dep.type != DependencyType::Circular) {
                        graph[name].push_back(dep.name);
                        buildGraph(dep.name);
                    }
                }
            }
        };

    buildGraph(componentName);

    // Perform topological sort and ensure dependencies come before dependents
    auto order = topologicalSort(graph);
    // Exclude the root component itself from the dependency list
    order.erase(std::remove(order.begin(), order.end(), componentName),
                order.end());
    // Reverse to ensure deepest dependencies load first
    std::reverse(order.begin(), order.end());
    return order;
}

bool LifecycleManager::executePhase(Component& component,
                                    LifecyclePhase phase) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    bool success = true;
    std::string errorMessage;

    try {
        std::shared_lock lock(mutex_);

        // Execute global hooks first
        auto globalIt = globalHooks_.find(phase);
        if (globalIt != globalHooks_.end()) {
            for (const auto& hook : globalIt->second) {
                hook(component, phase);
            }
        }

        // Execute component-specific hooks
        auto componentIt = hooks_.find(std::string(component.getName()));
        if (componentIt != hooks_.end()) {
            auto phaseIt = componentIt->second.find(phase);
            if (phaseIt != componentIt->second.end()) {
                for (const auto& hook : phaseIt->second) {
                    hook(component, phase);
                }
            }
        }

    } catch (const std::exception& e) {
        success = false;
        errorMessage = e.what();
    } catch (...) {
        success = false;
        errorMessage = "Unknown error during lifecycle phase execution";
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime);

    recordEvent(std::string(component.getName()), phase, duration, success,
                errorMessage);

    return success;
}

bool LifecycleManager::hasCircularDependencies(
    const std::string& componentName) const {
    std::shared_lock lock(mutex_);

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;

    auto it = dependencies_.find(componentName);
    if (it == dependencies_.end())
        return false;

    for (const auto& dep : it->second) {
        if (checkCircularDependency(componentName, dep.name, visited,
                                    recursionStack)) {
            return true;
        }
    }

    return false;
}

std::unordered_map<std::string, std::vector<std::string>>
LifecycleManager::getDependencyGraph() const {
    std::shared_lock lock(mutex_);

    std::unordered_map<std::string, std::vector<std::string>> graph;

    for (const auto& [component, deps] : dependencies_) {
        for (const auto& dep : deps) {
            graph[component].push_back(dep.name);
        }
    }

    return graph;
}

std::vector<LifecycleEvent> LifecycleManager::getLifecycleHistory(
    const std::string& componentName) const {
    std::shared_lock lock(mutex_);

    if (componentName.empty()) {
        return history_;
    }

    std::vector<LifecycleEvent> filtered;
    std::copy_if(history_.begin(), history_.end(), std::back_inserter(filtered),
                 [&componentName](const LifecycleEvent& event) {
                     return event.componentName == componentName;
                 });

    return filtered;
}

void LifecycleManager::clearHistory() {
    std::unique_lock lock(mutex_);
    history_.clear();
    // Also clear hooks and dependencies to avoid cross-test stale
    // captures/state
    hooks_.clear();
    globalHooks_.clear();
    dependencies_.clear();
}

bool LifecycleManager::validateDependencies(
    const std::string& componentName) const {
    std::shared_lock lock(mutex_);

    auto it = dependencies_.find(componentName);
    if (it == dependencies_.end())
        return true;

    for (const auto& dep : it->second) {
        if (dep.type == DependencyType::Required) {
            // In a real implementation, we would check if the dependency is
            // available For now, we assume all required dependencies are
            // satisfied
            if (dep.validator) {
                // Custom validation would be performed here
                // return dep.validator(*dependencyComponent);
            }
        }
    }

    return true;
}

void LifecycleManager::recordEvent(const std::string& componentName,
                                   LifecyclePhase phase,
                                   std::chrono::microseconds duration,
                                   bool success, const std::string& error) {
    std::unique_lock lock(mutex_);

    LifecycleEvent event;
    event.componentName = componentName;
    event.phase = phase;
    event.timestamp = std::chrono::steady_clock::now();
    event.duration = duration;
    event.success = success;
    event.errorMessage = error;

    history_.push_back(event);

    // Limit history size
    if (history_.size() > MAX_HISTORY_SIZE) {
        history_.erase(history_.begin(),
                       history_.begin() + (history_.size() - MAX_HISTORY_SIZE));
    }
}

bool LifecycleManager::checkCircularDependency(
    const std::string& componentName, const std::string& dependency,
    std::unordered_set<std::string>& visited,
    std::unordered_set<std::string>& recursionStack) const {
    // If we reached back to the original component, we have a cycle
    if (dependency == componentName) {
        return true;
    }

    // Already on the current recursion path -> cycle
    if (recursionStack.contains(dependency)) {
        return true;
    }

    // Fully explored earlier without finding a path back to root
    if (visited.contains(dependency)) {
        return false;
    }

    visited.insert(dependency);
    recursionStack.insert(dependency);

    // Explore dependency's own dependencies
    if (auto it = dependencies_.find(dependency); it != dependencies_.end()) {
        for (const auto& depConstraint : it->second) {
            const auto& next = depConstraint.name;
            // Direct cycle to root or self-cycle
            if (next == componentName || next == dependency) {
                recursionStack.erase(dependency);
                return true;
            }
            if (checkCircularDependency(componentName, next, visited,
                                        recursionStack)) {
                recursionStack.erase(dependency);
                return true;
            }
        }
    }

    recursionStack.erase(dependency);
    return false;
}

std::vector<std::string> LifecycleManager::topologicalSort(
    const std::unordered_map<std::string, std::vector<std::string>>& graph)
    const {
    std::unordered_map<std::string, int> inDegree;
    std::queue<std::string> queue;
    std::vector<std::string> result;

    // Calculate in-degrees
    for (const auto& [node, neighbors] : graph) {
        if (inDegree.find(node) == inDegree.end()) {
            inDegree[node] = 0;
        }
        for (const auto& neighbor : neighbors) {
            inDegree[neighbor]++;
        }
    }

    // Find nodes with no incoming edges
    for (const auto& [node, degree] : inDegree) {
        if (degree == 0) {
            queue.push(node);
        }
    }

    // Process nodes
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);

        auto it = graph.find(current);
        if (it != graph.end()) {
            for (const auto& neighbor : it->second) {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0) {
                    queue.push(neighbor);
                }
            }
        }
    }

    return result;
}

// ComponentLifecycleGuard implementation
ComponentLifecycleGuard::ComponentLifecycleGuard(Component& component,
                                                 LifecyclePhase startPhase,
                                                 LifecyclePhase endPhase)
    : component_(component), endPhase_(endPhase), success_(true) {
    success_ =
        LifecycleManager::instance().executePhase(component_, startPhase);
}

ComponentLifecycleGuard::~ComponentLifecycleGuard() {
    if (success_) {
        LifecycleManager::instance().executePhase(component_, endPhase_);
    }
}

}  // namespace atom::components
