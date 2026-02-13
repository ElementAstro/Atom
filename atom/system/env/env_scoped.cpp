/*
 * env_scoped.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Scoped environment variable management implementation

**************************************************/

#include "env_scoped.hpp"

#include <algorithm>
#include <mutex>

#include "env_core.hpp"
#include <spdlog/spdlog.h>

namespace atom::utils {

// Static member initializations
Vector<ScopedEnv*> EnvScoped::sActiveScopes;
std::mutex EnvScoped::sScopesMutex;

ScopedEnv::ScopedEnv(const String& key, const String& value)
    : mKey(key), mHadValue(false), mActive(true), mCreationTime(std::chrono::steady_clock::now()) {
    spdlog::debug("Creating scoped environment variable: {}={}", key, value);
    mOriginalValue = EnvCore::getEnv(key, "");
    mHadValue = !mOriginalValue.empty();
    EnvCore::setEnv(key, value);
    EnvScoped::registerScope(this);
}

ScopedEnv::ScopedEnv(const String& key, const String& value, ScopeRestoreCallback callback)
    : mKey(key), mHadValue(false), mActive(true),
      mCreationTime(std::chrono::steady_clock::now()), mRestoreCallback(callback) {
    spdlog::debug("Creating scoped environment variable with callback: {}={}", key, value);
    mOriginalValue = EnvCore::getEnv(key, "");
    mHadValue = !mOriginalValue.empty();
    EnvCore::setEnv(key, value);
    EnvScoped::registerScope(this);
}

ScopedEnv::~ScopedEnv() {
    if (mActive) {
        restore();
    }
    EnvScoped::unregisterScope(this);
}

String ScopedEnv::getCurrentValue() const {
    return EnvCore::getEnv(mKey, "");
}

void ScopedEnv::updateValue(const String& newValue) {
    if (!mActive) {
        spdlog::warn("Attempting to update inactive scoped environment variable: {}", mKey);
        return;
    }

    spdlog::debug("Updating scoped environment variable: {}={}", mKey, newValue);
    EnvCore::setEnv(mKey, newValue);
}

void ScopedEnv::restore() {
    if (!mActive) {
        return;
    }

    spdlog::debug("Restoring scoped environment variable: {}", mKey);
    String currentValue = getCurrentValue();

    if (mHadValue) {
        EnvCore::setEnv(mKey, mOriginalValue);
    } else {
        EnvCore::unsetEnv(mKey);
    }

    if (mRestoreCallback) {
        try {
            mRestoreCallback(mKey, mOriginalValue, currentValue);
        } catch (const std::exception& e) {
            spdlog::error("Exception in scope restore callback for {}: {}", mKey, e.what());
        }
    }

    mActive = false;
}

// ScopedEnvBatch implementation
ScopedEnvBatch::ScopedEnvBatch(const HashMap<String, String>& vars) : mActive(true) {
    spdlog::debug("Creating scoped environment batch with {} variables", vars.size());
    mScopes.reserve(vars.size());

    for (const auto& [key, value] : vars) {
        mScopes.push_back(std::make_unique<ScopedEnv>(key, value));
    }
}

ScopedEnvBatch::ScopedEnvBatch(const Vector<std::pair<String, String>>& vars) : mActive(true) {
    spdlog::debug("Creating scoped environment batch with {} variables", vars.size());
    mScopes.reserve(vars.size());

    for (const auto& [key, value] : vars) {
        mScopes.push_back(std::make_unique<ScopedEnv>(key, value));
    }
}

ScopedEnvBatch::~ScopedEnvBatch() {
    if (mActive) {
        restoreAll();
    }
}

void ScopedEnvBatch::addVariable(const String& key, const String& value) {
    if (!mActive) {
        spdlog::warn("Attempting to add variable to inactive batch");
        return;
    }

    mScopes.push_back(std::make_unique<ScopedEnv>(key, value));
    spdlog::debug("Added variable to batch: {}={}", key, value);
}

bool ScopedEnvBatch::updateVariable(const String& key, const String& newValue) {
    if (!mActive) {
        spdlog::warn("Attempting to update variable in inactive batch");
        return false;
    }

    for (auto& scope : mScopes) {
        if (scope->getKey() == key) {
            scope->updateValue(newValue);
            spdlog::debug("Updated variable in batch: {}={}", key, newValue);
            return true;
        }
    }

    spdlog::debug("Variable not found in batch: {}", key);
    return false;
}

bool ScopedEnvBatch::removeVariable(const String& key) {
    if (!mActive) {
        spdlog::warn("Attempting to remove variable from inactive batch");
        return false;
    }

    auto it = std::find_if(mScopes.begin(), mScopes.end(),
                           [&key](const std::unique_ptr<ScopedEnv>& scope) {
                               return scope->getKey() == key;
                           });

    if (it != mScopes.end()) {
        (*it)->restore();
        mScopes.erase(it);
        spdlog::debug("Removed variable from batch: {}", key);
        return true;
    }

    spdlog::debug("Variable not found in batch: {}", key);
    return false;
}

Vector<String> ScopedEnvBatch::getKeys() const {
    Vector<String> keys;
    keys.reserve(mScopes.size());

    for (const auto& scope : mScopes) {
        keys.push_back(scope->getKey());
    }

    return keys;
}

void ScopedEnvBatch::restoreAll() {
    if (!mActive) {
        return;
    }

    spdlog::debug("Restoring all variables in batch ({} variables)", mScopes.size());

    for (auto& scope : mScopes) {
        scope->restore();
    }

    mScopes.clear();
    mActive = false;
}

// EnvScoped implementation
auto EnvScoped::createScopedEnv(const String& key, const String& value)
    -> std::shared_ptr<ScopedEnv> {
    return std::make_shared<ScopedEnv>(key, value);
}

auto EnvScoped::createUniqueScopedEnv(const String& key, const String& value)
    -> std::unique_ptr<ScopedEnv> {
    return std::make_unique<ScopedEnv>(key, value);
}

auto EnvScoped::createScopedEnvWithCallback(const String& key, const String& value,
                                            ScopeRestoreCallback callback)
    -> std::unique_ptr<ScopedEnv> {
    return std::make_unique<ScopedEnv>(key, value, callback);
}

auto EnvScoped::createScopedEnvBatch(const HashMap<String, String>& vars)
    -> std::unique_ptr<ScopedEnvBatch> {
    return std::make_unique<ScopedEnvBatch>(vars);
}

auto EnvScoped::createNestedScope(const ScopedEnv& parentScope,
                                  const HashMap<String, String>& additionalVars)
    -> std::unique_ptr<ScopedEnvBatch> {
    HashMap<String, String> allVars = additionalVars;

    // Add parent scope's current value
    allVars[parentScope.getKey()] = parentScope.getCurrentValue();

    return std::make_unique<ScopedEnvBatch>(allVars);
}

auto EnvScoped::getScopeStatistics() -> HashMap<String, size_t> {
    std::lock_guard<std::mutex> lock(sScopesMutex);

    HashMap<String, size_t> stats;
    stats["active_scopes"] = sActiveScopes.size();

    size_t withCallbacks = 0;
    for (const auto* scope : sActiveScopes) {
        (void)scope;  // Suppress unused variable warning
        // Count scopes with callbacks (this would require exposing callback info)
        // For now, just count total
    }
    stats["scopes_with_callbacks"] = withCallbacks;

    return stats;
}

auto EnvScoped::listActiveScopes() -> Vector<String> {
    std::lock_guard<std::mutex> lock(sScopesMutex);

    Vector<String> scopeInfo;
    scopeInfo.reserve(sActiveScopes.size());

    for (const auto* scope : sActiveScopes) {
        String info = scope->getKey() + "=" + scope->getCurrentValue();
        scopeInfo.push_back(info);
    }

    return scopeInfo;
}

void EnvScoped::registerScope(ScopedEnv* scope) {
    std::lock_guard<std::mutex> lock(sScopesMutex);
    sActiveScopes.push_back(scope);
}

void EnvScoped::unregisterScope(ScopedEnv* scope) {
    std::lock_guard<std::mutex> lock(sScopesMutex);
    auto it = std::find(sActiveScopes.begin(), sActiveScopes.end(), scope);
    if (it != sActiveScopes.end()) {
        sActiveScopes.erase(it);
    }
}

}  // namespace atom::utils
