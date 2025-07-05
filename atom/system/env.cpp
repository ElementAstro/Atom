/*
 * env.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable management - Main implementation
that delegates to modular components

**************************************************/

#include "env.hpp"

#include <spdlog/spdlog.h>
#include <mutex>

namespace atom::utils {

// Static members for change notifications
static std::mutex sCallbackMutex;
static std::unordered_map<size_t, Env::EnvChangeCallback> sChangeCallbacks;
static size_t sNextCallbackId = 1;

// Implementation class that holds the actual data
class Env::Impl {
public:
    explicit Impl(int argc = 0, char** argv = nullptr) {
        core_ = std::make_unique<EnvCore>();
        fileIO_ = std::make_unique<EnvFileIO>();
        path_ = std::make_unique<EnvPath>();
        persistent_ = std::make_unique<EnvPersistent>();
        utils_ = std::make_unique<EnvUtils>();
        system_ = std::make_unique<EnvSystem>();

        if (argc > 0 && argv != nullptr) {
            programName_ = argv[0];
            for (int i = 1; i < argc; ++i) {
                std::string arg = argv[i];
                size_t pos = arg.find('=');
                if (pos != std::string::npos) {
                    args_[arg.substr(0, pos)] = arg.substr(pos + 1);
                } else {
                    args_[arg] = "";
                }
            }
        }
    }

    std::unique_ptr<EnvCore> core_;
    std::unique_ptr<EnvFileIO> fileIO_;
    std::unique_ptr<EnvPath> path_;
    std::unique_ptr<EnvPersistent> persistent_;
    std::unique_ptr<EnvUtils> utils_;
    std::unique_ptr<EnvSystem> system_;

    HashMap<String, String> args_;
    String programName_;
};

// Constructors
Env::Env() : impl_(std::make_shared<Impl>()) {}

Env::Env(int argc, char** argv) : impl_(std::make_shared<Impl>(argc, argv)) {}

auto Env::createShared(int argc, char** argv) -> std::shared_ptr<Env> {
    return std::make_shared<Env>(argc, argv);
}

// Static environment methods - delegate to EnvCore
auto Env::Environ() -> HashMap<String, String> { return EnvCore::Environ(); }

auto Env::setEnv(const String& key, const String& val) -> bool {
    return EnvCore::setEnv(key, val);
}

auto Env::setEnvMultiple(const HashMap<String, String>& vars) -> bool {
    return EnvCore::setEnvMultiple(vars);
}

auto Env::getEnv(const String& key, const String& default_value) -> String {
    return EnvCore::getEnv(key, default_value);
}

void Env::unsetEnv(const String& name) { EnvCore::unsetEnv(name); }

void Env::unsetEnvMultiple(const Vector<String>& names) {
    EnvCore::unsetEnvMultiple(names);
}

auto Env::listVariables() -> Vector<String> { return EnvCore::listVariables(); }

auto Env::filterVariables(
    const std::function<bool(const String&, const String&)>& predicate)
    -> HashMap<String, String> {
    return EnvCore::filterVariables(predicate);
}

auto Env::getVariablesWithPrefix(const String& prefix)
    -> HashMap<String, String> {
    return EnvCore::getVariablesWithPrefix(prefix);
}

// Change notification method (will be added to header if needed)
static void notifyChangeCallbacks(const String& key, const String& oldValue,
                                  const String& newValue) {
    spdlog::info(
        "Environment variable change notification: key={}, old_value={}, "
        "new_value={}",
        key, oldValue, newValue);
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    for (const auto& [id, callback] : sChangeCallbacks) {
        try {
            callback(key, oldValue, newValue);
        } catch (const std::exception& e) {
            spdlog::error("Exception in environment change callback: {}",
                          e.what());
        }
    }
}

// Instance methods - delegate to core
void Env::add(const String& key, const String& val) {
    impl_->core_->add(key, val);
}

void Env::addMultiple(const HashMap<String, String>& vars) {
    impl_->core_->addMultiple(vars);
}

bool Env::has(const String& key) { return impl_->core_->has(key); }

bool Env::hasAll(const Vector<String>& keys) {
    return impl_->core_->hasAll(keys);
}

bool Env::hasAny(const Vector<String>& keys) {
    return impl_->core_->hasAny(keys);
}

void Env::del(const String& key) { impl_->core_->del(key); }

void Env::delMultiple(const Vector<String>& keys) {
    impl_->core_->delMultiple(keys);
}

auto Env::get(const String& key, const String& default_value) -> String {
    return impl_->core_->get(key, default_value);
}

// File I/O methods - delegate to EnvFileIO
auto Env::saveToFile(const std::filesystem::path& filePath,
                     const HashMap<String, String>& vars) -> bool {
    return EnvFileIO::saveToFile(filePath, vars);
}

auto Env::loadFromFile(const std::filesystem::path& filePath, bool overwrite)
    -> bool {
    return EnvFileIO::loadFromFile(filePath, overwrite);
}

// PATH methods - delegate to EnvPath
auto Env::addToPath(const String& path, bool prepend) -> bool {
    return EnvPath::addToPath(path, prepend);
}

auto Env::removeFromPath(const String& path) -> bool {
    return EnvPath::removeFromPath(path);
}

auto Env::isInPath(const String& path) -> bool {
    return EnvPath::isInPath(path);
}

auto Env::getPathEntries() -> Vector<String> {
    return EnvPath::getPathEntries();
}

// Persistent methods - delegate to EnvPersistent
auto Env::setPersistentEnv(const String& key, const String& val,
                           PersistLevel level) -> bool {
    return EnvPersistent::setPersistentEnv(key, val, level);
}

auto Env::deletePersistentEnv(const String& key, PersistLevel level) -> bool {
    return EnvPersistent::deletePersistentEnv(key, level);
}

// Utility methods - delegate to EnvUtils
auto Env::expandVariables(const String& str, VariableFormat format) -> String {
    return EnvUtils::expandVariables(str, format);
}

// Notification methods
auto Env::registerChangeNotification(EnvChangeCallback callback) -> size_t {
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    size_t id = sNextCallbackId++;
    sChangeCallbacks[id] = callback;
    return id;
}

auto Env::unregisterChangeNotification(size_t id) -> bool {
    std::lock_guard<std::mutex> lock(sCallbackMutex);
    return sChangeCallbacks.erase(id) > 0;
}

// Additional missing delegated methods that may be called
auto Env::diffEnvironments(const HashMap<String, String>& env1,
                           const HashMap<String, String>& env2)
    -> std::tuple<HashMap<String, String>, HashMap<String, String>,
                  HashMap<String, String>> {
    return EnvUtils::diffEnvironments(env1, env2);
}

auto Env::mergeEnvironments(const HashMap<String, String>& baseEnv,
                            const HashMap<String, String>& overlayEnv,
                            bool override) -> HashMap<String, String> {
    return EnvUtils::mergeEnvironments(baseEnv, overlayEnv, override);
}

// System methods - delegate to EnvSystem
auto Env::getHomeDir() -> String { return EnvSystem::getHomeDir(); }

auto Env::getTempDir() -> String { return EnvSystem::getTempDir(); }

auto Env::getConfigDir() -> String { return EnvSystem::getConfigDir(); }

auto Env::getDataDir() -> String { return EnvSystem::getDataDir(); }

auto Env::getSystemName() -> String { return EnvSystem::getSystemName(); }

auto Env::getSystemArch() -> String { return EnvSystem::getSystemArch(); }

auto Env::getCurrentUser() -> String { return EnvSystem::getCurrentUser(); }

auto Env::getHostName() -> String { return EnvSystem::getHostName(); }

// Program information methods
auto Env::getExecutablePath() const -> String { return impl_->programName_; }

auto Env::getWorkingDirectory() const -> String {
    return EnvSystem::getHomeDir();  // Return home dir as working dir
}

auto Env::getProgramName() const -> String { return impl_->programName_; }

auto Env::getAllArgs() const -> HashMap<String, String> { return impl_->args_; }

// Scoped environment methods
auto Env::createScopedEnv(const String& key, const String& value)
    -> std::shared_ptr<ScopedEnv> {
    return std::make_shared<ScopedEnv>(key, value);
}

// ScopedEnv implementation
Env::ScopedEnv::ScopedEnv(const String& key, const String& value)
    : mKey(key), mHadValue(false) {
    // Save current value if it exists
    auto current = EnvCore::getEnv(key, "");
    if (!current.empty()) {
        mOriginalValue = current;
        mHadValue = true;
    }
    // Set new value
    EnvCore::setEnv(key, value);
}

Env::ScopedEnv::~ScopedEnv() {
    if (mHadValue) {
        EnvCore::setEnv(mKey, mOriginalValue);
    } else {
        EnvCore::unsetEnv(mKey);
    }
}

#if ATOM_ENABLE_DEBUG
void Env::printAllVariables() { EnvCore::printAllVariables(); }

void Env::printAllArgs() const {
    for (const auto& [key, value] : impl_->args_) {
        spdlog::debug("Arg: {} = {}", key, value);
    }
}
#endif

}  // namespace atom::utils
